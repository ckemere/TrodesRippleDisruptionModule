#include "trodesinterface.h"
#include "ripplepower.h"
#include "moduledefines.h"
#include "stiminterface.h"

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

// Stimulation server address
struct sockaddr_in stimserver_addr;
bool stimserver_addr_initialized;
std::mutex stim_server_addr_lock;


// Status
std::atomic<TrodesInterface::TrodesNetworkStatus> trodesNetworkStatus = TrodesInterface::TrodesNetworkStatus::not_connected;

// Ripple detetection parameters
std::mutex parameters_lock;
double ripple_threshold = 3.0;
double noise_threshold = 0.0;
unsigned int num_active_channels = 1;
double minimum_stim_isi;
double minimum_stim_average_isi;
bool post_detection_delay;

// std::atomic<int> post_detection_delay; // in samples
std::atomic<bool> stimulation_enabled;


alignas(64) uint16_t inter_stim_intervals[8];
unsigned int isi_idx;
std::atomic<unsigned int> isi_sum;
unsigned int current_isi;

std::atomic<double> recent_ripple_rate; // over last 30 s

// Which channels of data are we using
std::vector<unsigned int> signal_channels;
std::vector<unsigned int> rip_channels;
std::vector<unsigned int> noise_channels;
std::atomic<bool> ripple_channels_changed;

// Related to trainging mean and std-dev
std::mutex statistics_lock;
bool currently_training;
bool abort_training_flag;
unsigned int training_duration; // in samples
std::vector<double> means; // entry for every channel, but only updates channels in use
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
    for (auto ch : signal_channels) {
    // https://math.stackexchange.com/questions/374881/recursive-formula-for-variance
        old_mean_sq = means[ch]*means[ch];
        means[ch] = means[ch] + (new_data[ch] - means[ch]) / training_sample_count;
        vars[ch] = vars[ch] + old_mean_sq - means[ch]*means[ch] + (new_data[ch]*new_data[ch] - vars[ch] - old_mean_sq)/training_sample_count;
    }
}

void network_processing_loop (std::thread *trodes_network, std::string lfp_pub_endpoint)
{

    trodes_network = new std::thread([endpoint = lfp_pub_endpoint]() {
        

        /* In this thread, estabish a connection to the stim interface */
        // TODO - send the address and port over as part of starting this thread!
        int sockfd;
        const char *regular_stim_cmd = "T0";
        const char *delay_stim_cmd = "D0";


        if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            std::cerr << "Failure opening socket to stim server.";
        }

        std::cerr << "Trodes lfp thread starting";

        ZmqSourceSubscriber<trodes::network::TrodesLFPData> lfp_data(endpoint);
        std::cerr << "Established connection to LFP data endpoint" << endpoint << std::endl;

        trodesNetworkStatus = TrodesInterface::TrodesNetworkStatus::streaming;

        std::stringstream result;

        long int data_count = 0;
        while (true) {
            auto recvd = lfp_data.receive();
            // recvd has     uint32_t localTimestamp; std::vector< int16_t > lfpData; int64_t systemTimestamp;

            if (ripple_channels_changed) { // this is atomic
                std::cerr << "Ripple channels size " << signal_channels.size() << std::endl;
                ripple_power->reset(signal_channels);
                statistics_lock.lock();
                for (auto ch : signal_channels) {
                    means[ch] = 0;
                    vars[ch] = 0;
                }
                statistics_lock.unlock();
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

                unsigned int ripple_vote = 0;
                double max_norm_ripple_power = -100; // Use for rejecting too large of ripples. Maybe redundant with noise channel detection
                double total_norm_noise_power = 0;

                parameters_lock.lock(); // we're going to access the parameters in a second. protect with mutex

                // Z-score ripple channels and check for threshold crossing. Increment vote and max power if so
                for (auto ch : rip_channels) {
                    double norm_power = (ripple_power->output[ch] - means[ch])/std_devs[ch]; // z-score
                    if (norm_power > ripple_threshold) {
                        ripple_vote++;
                        max_norm_ripple_power = std::max(max_norm_ripple_power, norm_power);
                    }
                }

                // Z-score noise channels and check for threshold crossing
                bool noise_vote = false;
                if (noise_channels.size() > 0) {
                    for (auto ch : noise_channels) {
                        total_norm_noise_power += (ripple_power->output[ch] - means[ch])/std_devs[ch];  // z-score
                    }

                    noise_vote = (total_norm_noise_power / noise_channels.size()) > noise_threshold;
                }

                if ((!noise_vote) &&
                    (ripple_vote >= num_active_channels) && // Did vote succeed?
                    (current_isi > minimum_stim_isi) && // Are we more than our minimum inter-stim difference?
                    ((current_isi + isi_sum) / 9 > minimum_stim_average_isi)) {// Are we going to make our average stim rate too high?

                    if (!post_detection_delay) {
                        // STIMULATE HERE
                        sendto(sockfd, regular_stim_cmd, strlen(regular_stim_cmd), 0, (const struct sockaddr *)&stimserver_addr, sizeof(stimserver_addr));
                        // TODO - verify response?
                        
                        std::cerr << "STIMULATE " << max_norm_ripple_power << std::endl;

                    }
                    else {
                        sendto(sockfd, delay_stim_cmd, strlen(delay_stim_cmd), 0, (const struct sockaddr *)&stimserver_addr, sizeof(stimserver_addr));
                        // TODO - verify response?
                        std::cerr << "CONTROL STIMULATE " << max_norm_ripple_power << std::endl;
                    }

                    isi_sum = isi_sum - inter_stim_intervals[isi_idx];
                    inter_stim_intervals[isi_idx] = current_isi;
                    isi_idx = (isi_idx + 1) & 0x7; // I know that the array is size 8
                    isi_sum = isi_sum + current_isi;
                    current_isi = 0;
                }
                parameters_lock.unlock(); // unlock mutex
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

    stimserver_addr_initialized = false;
    memset(&stimserver_addr, 0, sizeof(stimserver_addr));
    stimserver_addr.sin_family = AF_INET;
    stimserver_addr.sin_port = htons(DEFAULT_STIM_SERVER_PORT);
    stimserver_addr.sin_addr.s_addr = inet_addr(DEFAULT_STIM_SERVER_ADDRESS);
    
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
                    trodesNetworkStatus = TrodesInterface::TrodesNetworkStatus::connected;

                }
                // probably emit something here
            }
        }
    }
}

/*
struct RippleParameters {
    double ripple_threshold;
    unsigned int num_active_channels;
    unsigned int minimum_stim_isi; // in samples
    unsigned int minimum_stim_average_isi; // in samples
    bool post_detection_delay; 
}
*/

void TrodesInterface::updateParameters(RippleParameters new_params)
{
    parameters_lock.lock();
    ripple_threshold = new_params.ripple_threshold;
    num_active_channels = new_params.num_active_channels;
    minimum_stim_isi = new_params.minimum_stim_isi;
    minimum_stim_average_isi = new_params.minimum_stim_average_isi;
    post_detection_delay = new_params.post_detection_delay;
    noise_threshold = new_params.noise_threshold;
    parameters_lock.unlock();
    qDebug() << "Got new params! " << new_params.ripple_threshold << " " 
                                << new_params.num_active_channels << " " 
                                << new_params.minimum_stim_isi << " " 
                                << new_params.minimum_stim_average_isi << " "
                                << new_params.noise_threshold;
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

void TrodesInterface::newRippleChannels(QList<unsigned int> rip_chans, QList<unsigned int> noise_chans)
{
    // TODO - this needs to reshape the ripple structure in the other thread!
    // It won't necessarily be done until after streaming has started.
    qDebug() << rip_chans.size() << " " << noise_chans.size();
    statistics_lock.lock();
    signal_channels.resize(rip_chans.size() + noise_chans.size());
    rip_channels.resize(rip_chans.size());
    noise_channels.resize(noise_chans.size());
    int k = 0;

    qDebug() << rip_channels.size() << " " << noise_channels.size();

    for (unsigned int i = 0; i < rip_chans.size(); i++) {
        signal_channels[k++] = rip_chans[i];
        rip_channels[i] = rip_chans[i];
    }

    qDebug() << rip_channels.size() << " " << noise_channels.size();


    for (unsigned int i = 0; i < noise_chans.size(); i++) {
        signal_channels[k++] = noise_chans[i];
        noise_channels[i] = noise_chans[i];
    }
    ripple_channels_changed = true;
    statistics_lock.unlock();
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
    emit newTrainingStats(means, vars, training_duration, SAMPLES_PER_SECOND * 8.0 / isi_sum);
    statistics_lock.unlock();
}

void TrodesInterface::stimServerUpdated(QString new_address, quint16 new_port)
{
    // Update address and port to which the stim pulse command will be sent.

    // We don't need to double check because this signal has been emitted only after
    //  the address was validated...
    // sendto(sockfd, status_cmd, strlen(status_cmd), 0, (const struct sockaddr *)&stimserver_addr, sizeof(stimserver_addr));

    stim_server_addr_lock.lock();
    stimserver_addr.sin_port = htons(DEFAULT_STIM_SERVER_PORT);
    stimserver_addr.sin_addr.s_addr = inet_addr(DEFAULT_STIM_SERVER_ADDRESS);
    stim_server_addr_lock.unlock();
}