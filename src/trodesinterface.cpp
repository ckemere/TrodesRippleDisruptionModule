#include "trodesinterface.h"
#include "ripplepower.h"
#include "moduledefines.h"

#include <TrodesNetwork/Connection.h>
#include <TrodesNetwork/Generated/AcquisitionCommand.h>
#include <TrodesNetwork/Generated/SourceStatus.h>
#include <TrodesNetwork/Generated/TrodesLFPData.h>
#include "ZmqSourceSubscriber.h"


#include <Trodes/TimestampUtil.h>

// #include <chrono>
#include <iostream>
// #include <sstream>
#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

#include <QDebug>
#include <QThread>

#include <QTimer>


// Global variables for communicating between TrodesNet thread and QT

// Status
std::atomic<TrodesInterface::TrodesNetworkStatus> trodesNetworkStatus = TrodesInterface::TrodesNetworkStatus::not_connected;

// Ripple detetection parameters 
std::atomic<double> ripple_threshold;
std::atomic<unsigned int> num_active_channels;
std::atomic<double> minimum_ripple_isi;
std::atomic<double> minimum_ripple_average_isi;
std::atomic<int> post_detection_delay; // in samples
std::atomic<bool> stimulation_enabled;


alignas(64) uint16_t inter_stim_intervals[8];
unsigned int isi_idx;
std::atomic<unsigned int> isi_sum;
unsigned int current_isi;

std::atomic<double> recent_ripple_rate; // over last 30 s

// Which channels of data are we using
std::vector<unsigned int> ripple_channels;
std::atomic<bool> ripple_channels_changed;

// Related to trainging mean and std-dev
std::mutex statistics_lock;
bool currently_training;
bool abort_training_flag;
unsigned int training_duration; // in samples
std::vector<double> means;
std::vector<double> vars;
std::vector<double> std_devs;

// Make these global to avoid reallocation
int training_sample_count;
RipplePower *ripple_power;

void initialize_vectors(unsigned int n_channels) {
    means.resize(n_channels, 0.0);
    vars.resize(n_channels, 0.0);
    std_devs.resize(n_channels, 0.0);

    ripple_power = new RipplePower(n_channels);
}

void update_statistics(std::vector<double> new_data)
{
    double old_mean_sq;
    
    // This is called inside a mutex, so it should be protected
    training_sample_count++;
    for (auto ch : ripple_channels) {
        old_mean_sq = means[ch]*means[ch];
        means[ch] = means[ch] + (new_data[ch] - means[ch]) / training_sample_count;
        vars[ch] = vars[ch] + means[ch]*means[ch] - old_mean_sq + (new_data[ch]*new_data[ch] - vars[ch] - old_mean_sq)/training_sample_count;
    }
}

void network_processing_loop (std::thread *trodes_network, std::string lfp_pub_endpoint)
{

    trodes_network = new std::thread([endpoint = lfp_pub_endpoint]() {
        
        std::cerr << "Trodes lfp thread starting";

        ZmqSourceSubscriber<trodes::network::TrodesLFPData> lfp_data(endpoint);
        std::cerr << "Established connection to LFP data endpoint" << endpoint << std::endl;

        std::stringstream result;

        long int data_count = 0;
        while (true) {
            auto recvd = lfp_data.receive();
            // recvd has     uint32_t localTimestamp; std::vector< int16_t > lfpData; int64_t systemTimestamp;

            if (ripple_channels_changed) { // this is atomic
                ripple_power->reset(ripple_channels);
                ripple_channels_changed = false; // this is atomic
            }

            ripple_power->new_data(recvd.lfpData); // Filter and envelope

            statistics_lock.lock();
            if (currently_training) {
                update_statistics(ripple_power->output);
                if (abort_training_flag) // If we want to abort, set duration to 1. Next check will make it 0
                    training_duration = 1;

                if (--training_duration == 0) {
                    currently_training = false;
                    for (unsigned int ch = 0; ch < vars.size(); ch++)
                        std_devs[ch] = sqrt(vars[ch]);
                }
            }            
            statistics_lock.unlock();

            // stimulation decision: (1) over threshold (2) with the right number of channels 
            // (3) with instantaneous rate less than X and (4) at least XX after previous stimulation

            if (stimulation_enabled) {
                current_isi++; // incremement time since last stimulation

                int stimulation_vote = 0;
                for (auto ch : ripple_channels) {
                    double norm_power = (ripple_power->output[ch] - means[ch])/std_devs[ch];
                    if (norm_power > ripple_threshold)
                        stimulation_vote++;
                }

                if ((stimulation_vote >= num_active_channels) || // Did vote succeed?
                    // STIMULATE HERE

                    (current_isi > minimum_ripple_isi) || // Are we more than our minimum inter-stim difference?
                    ((current_isi + isi_sum) / 9 > minimum_ripple_average_isi)) {// Are we going to make our average stim rate too high?
               
                    isi_sum = isi_sum - inter_stim_intervals[isi_idx];
                    inter_stim_intervals[isi_idx] = current_isi;
                    isi_idx = (isi_idx + 1) & 0x7; // I know that the array is size 8
                    isi_sum = isi_sum + current_isi;
                    current_isi = 0;
                }
            }
            

            data_count++;
            if (data_count % 1500 == 0)
                std::cerr << "Data count " << data_count << std::endl;
        }                    
    });

    trodes_network->detach();

}

TrodesInterface::TrodesInterface(QObject *parent = nullptr, std::string server_address = "127.0.0.1", int server_port = 10000, unsigned int num_channels = 1)
    : QObject{parent}
    , server_address(server_address), server_port(server_port), nchan_lfp(num_channels)
{
    // set up globals
    num_active_channels = 1; // default value is 1
    post_detection_delay = 0; 
    
    initialize_vectors(nchan_lfp);
}

void TrodesInterface::run()
{
    c = new trodes::network::Connection(server_address, server_port);

    std::cerr << "[trodes network] thread starting!" << std::endl;

    std::string source_pub_endpoint;
    std::string acq_pub_endpoint;

    while ((source_pub_endpoint = c->get_endpoint("trodes.source.pub")) == "") {
        std::cerr << "[source pub] Endpoint `trodes.source.pub` is not available on the network yet. Retrying in 500ms..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    sstatus = new ZmqSourceSubscriber<trodes::network::SourceStatus>(source_pub_endpoint);
    // int fd;
    // size_t sizeof_fd = sizeof(fd);
    // if(zmq_getsockopt(sstatus->socket_, ZMQ_FD, &fd, &sizeof_fd))
    //     std::cerr << "Error retrieving sstatus zmq fd";
    sstatus_notifier = new QSocketNotifier(sstatus->socket_.get(zmq::sockopt::fd), QSocketNotifier::Read, this); // Setup a socket notifier for the source status channel
    connect(sstatus_notifier, SIGNAL(activated(QSocketDescriptor, QSocketNotifier::Type)), this, SLOT(sstatus_activity()));

    while ((acq_pub_endpoint = c->get_endpoint("trodes.acquisition")) == "") {
        std::cerr << "[source pub] Endpoint `trodes.acquisition` is not available on the network yet. Retrying in 500ms..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    acq = new ZmqSourceSubscriber<trodes::network::AcquisitionCommand>(acq_pub_endpoint);
    //  if(zmq_getsockopt(acq->socket_, ZMQ_FD, &fd, &sizeof_fd))
    //     std::cerr << "Error retrieving sstatus zmq fd";
    acq_notifier = new QSocketNotifier(acq->socket_.get(zmq::sockopt::fd), QSocketNotifier::Read, this); // Setup a socket notifier for the acquisition commands channel
    connect(acq_notifier, SIGNAL(activated(QSocketDescriptor, QSocketNotifier::Type)), this, SLOT(acq_activity()));

    trodesNetworkStatus = TrodesInterface::TrodesNetworkStatus::connected;

    ifaceUpdateTimer = new QTimer();
    QObject::connect(ifaceUpdateTimer, SIGNAL(timeout()), this, SLOT(reportIFaceData()));
    ifaceUpdateTimer->start(500);
    
    // emit finished();
}

void TrodesInterface::finish()
{
    delete acq_notifier;
    delete sstatus_notifier;
    delete sstatus;
    delete acq;
    delete lfp;
    delete c;
}


void TrodesInterface::sstatus_activity()
{
    auto flags = sstatus->socket_.get(zmq::sockopt::events); // zmq getsockopt could return an error that cppzmq just catches with an assert
    if(flags & ZMQ_POLLIN) {    // not checking other flags like ZMQ_POLLOUT
        bool done = false;
        while (!done) {
            zmq::message_t message;
            auto rv = sstatus->socket_.recv(message, zmq::recv_flags::dontwait);
            if (!rv.has_value()) {
                done = true;
            }
            else {
                auto msg = trodes::network::util::unpack<trodes::network::SourceStatus>(message.to_string());
                qDebug() << "[In TrodesInterface] Got a status message" << QString::fromStdString(msg.message);
                // probably emit something here
            }
        }

    }
}

void TrodesInterface::acq_activity()
{

    auto flags = acq->socket_.get(zmq::sockopt::events); // zmq getsockopt could return an error that cppzmq just catches with an assert

    if(flags & ZMQ_POLLIN) {
        bool done = false;
        while (!done) {
            zmq::message_t message;
            auto rv = acq->socket_.recv(message, zmq::recv_flags::dontwait);
            if (!rv.has_value()) {
                done = true;
            }
            else {
                auto msg = trodes::network::util::unpack<trodes::network::AcquisitionCommand>(message.to_string());
                qDebug() << "[In TrodesInterface] Got an acq message" << QString::fromStdString(msg.command);
                if (msg.command == "play") { // Streaming is beginning
                    // this will block, but I think that's ok?
                    
                    // QThread::msleep(100); // required before bug fix to make sure that there was time to replace the old endpoint in the table
                    std::string lfp_pub_endpoint;
                    while ((lfp_pub_endpoint = c->get_endpoint("source.lfp")) == "") {
                        std::cerr << "[source lfp] Endpoint `source.lfp` is not available on the network yet. Retrying in 100ms..." << std::endl;
                        QThread::msleep(100);
                        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    network_processing_loop(lfp_thread, lfp_pub_endpoint);
                }
                else if (msg.command == "stop") { // Streaming is beginning
                    // this will block, but I think that's ok?
                    delete lfp_thread; // have we leaked anything?
                }
                // probably emit something here
            }
        }
    }
}

void TrodesInterface::updateParameters()
{
    qDebug() << "Got new params! " << QThread::currentThreadId();

    emit parametersUpdated();
}

void TrodesInterface::enableStimulation(bool enable)
{
    if (enable) {
        if (!stimulation_enabled) { // This is atomic
            // Reset stimulation statistics
            for (auto isi : inter_stim_intervals)
                isi = 15000; // ten seconds
            isi_idx = 0;
            isi_sum = 8*15000;
            stimulation_enabled = true;
            current_isi = 0;
        }
    }
    else {
        stimulation_enabled = false; // Shut it down!
    }
}

void TrodesInterface::updateNetworkStatus()
{
    currentNetworkStatus = trodesNetworkStatus;
    emit networkStatus(currentNetworkStatus);
}

void TrodesInterface::newRippleChannels(QList<unsigned int> channels)
{

    // TODO - this needs to reshape the ripple structure in the other thread!
    // It won't necessarily be done until after streaming has started.

    foreach(auto &x,channels)
        qDebug()<<x;

    statistics_lock.lock();
    ripple_channels = std::vector<unsigned int>(channels.begin(), channels.end()); // copy a qlist to a std::vector
    ripple_channels_changed = true;
    statistics_lock.unlock();

    qDebug() << "updated ripple channels ";

}

void TrodesInterface::startTraining(unsigned int new_training_duration)
{
    if (new_training_duration > 0) { // We really do want to start training
        statistics_lock.lock(); // This will block updates if we're currently training?
        abort_training_flag = false; // We're going to start now, so we don't want to stop
        training_duration = new_training_duration;
        if (!currently_training) {
            for (unsigned int ch=0; ch < means.size(); ch++)
            {
                means[ch] = 0.0;
                vars[ch] = 0.0;
            }
            currently_training = true;
        }
        statistics_lock.unlock();
    }
    else { // new_training_duration == 0, meaning we want to abort training
        statistics_lock.lock();
        abort_training_flag = true; // Note that this gets reset next time we start training
        statistics_lock.unlock();
    }

    qDebug() << "New training duration recevied";
}

void TrodesInterface::reportIFaceData()
{
    statistics_lock.lock();
    emit newTrainingStats(means, vars, training_duration);
    statistics_lock.unlock();
}