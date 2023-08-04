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
std::atomic<TrodesInterface::TrodesNetworkStatus> trodesNetworkStatus = TrodesInterface::TrodesNetworkStatus::not_connected;

// Parameters that might be dynamic
std::atomic<double> ripple_threshold;
std::atomic<int> num_active_channels;
std::atomic<int> post_detection_delay; // in samples

std::atomic<bool> currently_training;
std::atomic<int> training_duration; // in samples
std::atomic<bool> stimulation_enabled;

std::mutex statistics_lock;
std::vector<unsigned int> ripple_channels;
std::atomic<bool> ripple_channels_changed;
std::vector<double> means;
std::vector<double> vars;
std::vector<double> std_devs;
int training_sample_count;

std::atomic<double> recent_ripple_rate; // over last 30 s

void update_statistics(std::vector<double> new_data)
{
    double old_mean_sq;
    
    statistics_lock.lock();
    training_sample_count++;
    for (int i=0; i < means.size(); i++) {
        old_mean_sq = means[i]*means[i];
        means[i] = means[i] + (new_data[i] - means[i]) / training_sample_count;
        vars[i] = vars[i] + means[i]*means[i] - old_mean_sq + (new_data[i]*new_data[i] - vars[i] - old_mean_sq)/training_sample_count; // this is old means
    }
    statistics_lock.unlock();
}

void network_processing_loop (std::thread *trodes_network, std::string lfp_pub_endpoint)
{

    trodes_network = new std::thread([endpoint = lfp_pub_endpoint]() {
        
        statistics_lock.lock();
        RipplePower ripple_power(ripple_channels);
        statistics_lock.unlock();

        std::cerr << "Trodes lfp thread starting";

        ZmqSourceSubscriber<trodes::network::TrodesLFPData> lfp_data(endpoint);
        std::cerr << "Established connection to LFP data endpoint" << endpoint << std::endl;

        std::stringstream result;

        long int data_count = 0;
        while (true) {
            if (ripple_channels_changed) {
                ripple_power.reset(ripple_channels);
                ripple_channels_changed = false; // this is atomic
            }

            auto recvd = lfp_data.receive();
            // recvd has     uint32_t localTimestamp; std::vector< int16_t > lfpData; int64_t systemTimestamp;

            // The data that I want to filter is in recvd.lfpData

            ripple_power.new_data(recvd.lfpData);

            if (currently_training) {
                update_statistics(ripple_power.output);
                if (--training_duration <= 0) {
                    currently_training = false;
                    statistics_lock.lock();
                    for (unsigned int i = 0; i < vars.size(); i++)
                        std_devs[i] = sqrt(vars[i]);
                    statistics_lock.unlock();

                }
            }            // stimulation decision: (1) over threshold (2) with the right number of channels 
            // (3) with instantaneous rate less than X and (4) at least XX after previous stimulation


            // stimulation decision: (1) over threshold (2) with the right number of channels 
            // (3) with instantaneous rate less than X and (4) at least XX after previous stimulation

            if (stimulation_enabled) {
                
                int should_stimulate_temp = 0;
                for (unsigned int ch=0; ch < ripple_channels.size(); ch++) {
                    norm_power = (ripple_power.output[ch] - means[ch])/std_devs[i];
                    if (norm_power > ripple_threshold)
                        should_stimulate_temp++;
                }

                if (should_stimulate_temp >= num_active_channels)
            }
            

            data_count++;
            if (data_count % 1500 == 0)
                std::cerr << "Data count " << data_count << std::endl;
        }                    
    });

    trodes_network->detach();

}

TrodesInterface::TrodesInterface(QObject *parent = nullptr, std::string server_address = "127.0.0.1", int server_port = 10000)
    : QObject{parent}
    , server_address(server_address), server_port(server_port)
{
    // set up globals
    num_active_channels = 1; // default value is 1
    post_detection_delay = 0; 

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
    means.resize(channels.size(), 0);
    vars.resize(channels.size(), 0);
    std_devs.resize(channels.size(), 0);
    statistics_lock.unlock();

    ripple_channels_changed = true; // this is atomic!

    qDebug() << "updated ripple channels ";

}

void TrodesInterface::startTraining(int new_training_duration)
{
    statistics_lock.lock();
    training_duration = new_training_duration; // This is changing an atomic variable
    if (!currently_training) {
        for (int i=0; i < means.size(); i++)
        {
            means[i] = 0.0;
            vars[i] = 0.0;
        }
        currently_training = true;
    }
    statistics_lock.unlock();

    qDebug() << "New training duration recevied";
}

void TrodesInterface::reportIFaceData()
{
    statistics_lock.lock();
    emit newTrainingStats(means, vars, training_duration);
    statistics_lock.unlock();
}