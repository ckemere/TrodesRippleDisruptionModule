#include "trodesinterface.h"

#include "ripplepower.h"

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

#include <QDebug>
#include <QThread>

// Global variables for communicating between TrodesNet thread and QT
std::atomic<TrodesInterface::TrodesNetworkStatus> trodesNetworkStatus = TrodesInterface::TrodesNetworkStatus::not_connected;

// Parameters that might be dynamic
std::atomic<double> rippleThreshold;
std::atomic<int>    numActiveChannels;
std::atomic<double> postDetectionDelay;

// void network_processing_loop (std::string server_address, int server_port) {
        // std::thread trodes_network([ address = server_address, port = server_port]() {

void network_processing_loop (std::thread *trodes_network, 
                              std::string lfp_pub_endpoint, 
                              std::vector<int> ripple_channels) {

    trodes_network = new std::thread([endpoint = lfp_pub_endpoint, ripple_channels = ripple_channels]() {
        std::cerr << "Ripple channels size: " << ripple_channels.size() << std::endl;

        RipplePower ripple_power(ripple_channels);
        std::cerr << "Trodes lfp thread starting";

        ZmqSourceSubscriber<trodes::network::TrodesLFPData> lfp_data(endpoint);
        std::cerr << "Established connection to LFP data endpoint" << endpoint << std::endl;

        std::stringstream result;

        long int data_count = 0;
        while (true) {
            auto recvd = lfp_data.receive();
            // recvd has     uint32_t localTimestamp; std::vector< int16_t > lfpData; int64_t systemTimestamp;

            // The data that I want to filter is in recvd.lfpData

            ripple_power.new_data(recvd.lfpData);
            
            // stimulation decision: (1) over threshold (2) 

            data_count++;
            if (data_count % 15000 == 0)
                std::cerr << "Data count " << data_count;
        }                    
    });

    trodes_network->detach();

}

TrodesInterface::TrodesInterface(QObject *parent = nullptr, std::string server_address = "127.0.0.1", int server_port = 10000)
    : QObject{parent}
    , server_address(server_address), server_port(server_port)
{

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

    // NOTE - network_processing_loop returns, so this function returns. The benefit of this is that we can process signals
    //        and slots. The cost is that it makes communication with the networking more complicated. 
    
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
                    network_processing_loop(lfp_thread, lfp_pub_endpoint, ripple_channels);
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

void TrodesInterface::newRippleChannels(QList<int> channels)
{
    qDebug() << "updated ripple channels ";
    foreach(auto &x,channels)
        qDebug()<<x;
    ripple_channels = std::vector<int>(channels.begin(), channels.end());
}
