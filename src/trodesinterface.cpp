#include "trodesinterface.h"


#include <TrodesNetwork/Connection.h>
#include <TrodesNetwork/Server.h>
#include <TrodesNetwork/Resources/SourceSubscriber.h>
#include <TrodesNetwork/Generated/AcquisitionCommand.h>
#include <TrodesNetwork/Generated/SourceStatus.h>
#include <TrodesNetwork/Generated/TrodesAnalogData.h>
#include <TrodesNetwork/Generated/TrodesDigitalData.h>
#include <TrodesNetwork/Generated/TrodesNeuralData.h>
#include <TrodesNetwork/Generated/TrodesTimestampData.h>
#include <TrodesNetwork/Generated/TrodesLFPData.h>
#include <TrodesNetwork/Generated/TrodesSpikeWaveformData.h>
#include <TrodesNetwork/Resources/RawSourceSubscriber.h>


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

// Parameters
std::atomic<double> rippleThreshold;
std::atomic<int>    numActiveChannels;
std::atomic<double> postDetectionDelay;

void network_processing_loop (std::string server_address, int server_port) {

    std::thread trodes_network([ address = server_address, port = server_port]() {
        trodes::network::Connection c(address, port);

        std::cerr << "[trodes network] thread starting!" << std::endl;

        std::string source_pub_endpoint;
        std::string acq_pub_endpoint;
        std::string lfp_pub_endpoint;

        while ((source_pub_endpoint = c.get_endpoint("trodes.source.pub")) == "") {
            std::cerr << "[source pub] Endpoint `trodes.source.pub` is not available on the network yet. Retrying in 500ms..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        ZmqSourceSubscriber<trodes::network::SourceStatus> sstatus(source_pub_endpoint);

        while ((acq_pub_endpoint = c.get_endpoint("trodes.acquisition")) == "") {
            std::cerr << "[source pub] Endpoint `trodes.acquisition` is not available on the network yet. Retrying in 500ms..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        ZmqSourceSubscriber<trodes::network::AcquisitionCommand> acq(acq_pub_endpoint);

        trodesNetworkStatus = TrodesInterface::TrodesNetworkStatus::connected;

        // Want to wait for a [trodes.source.pub]connect and a [trodes.acquisition]play


        while ((lfp_pub_endpoint = c.get_endpoint("source.lfp")) == "") {
            std::cerr << "[source pub] Endpoint `source.lfp` is not available on the network yet. Retrying in 500ms..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        ZmqSourceSubscriber<trodes::network::AcquisitionCommand> lfp(lfp_pub_endpoint);


        // Only valid until a [trodes.acquisition]stop. Stop is always generated with another [trodes.source.pub]connect if that's useful

        std::stringstream result;

        while (true) {
            auto recvd = sstatus.receive();
            std::cerr << "[trodes.source.pub]" << recvd.message;
            result.str({});
            result.clear();
        }
            //     while (true) {
    //         auto recvd = sstatus.receive();
    //         std::cerr << "[trodes.source.pub]" << recvd.message;
    //         result.str({});
    //         result.clear();
    //     }
    //     while (true) {
    //         auto recvd = lfp.receive();
    //         result << "L: ";
    //         result << recvd.localTimestamp << " ";
    //         result << recvd.systemTimestamp << " ";
    //         for (auto i : recvd.lfpData) {
    //             result << i << ",";
    //         }
    //         result << "\n";
    //         // std::cerr << "[lfp listener]" << result.str();
    //         result.str({});
    //         result.clear();
    //     }
    //     while (true) {
    //         auto recvd = acq.receive();
    //         // zmq::message_t message;
    //         // auto rv = acq.socket_.recv(message);
    //         // if (!rv.has_value()) {
    //         // // failed to receive // process errors!
    //         // }
    //         // auto recv = trodes::network::util::unpack<trodes::network::AcquisitionCommand>(message.to_string());

    //         std::cerr << "[trodes.acquisition]" << recvd.command;
    //         result.str({});
    //         result.clear();
    //     }


    });

    trodes_network.detach();

}

TrodesInterface::TrodesInterface(QObject *parent = nullptr, std::string server_address = "127.0.0.1", int server_port = 10000)
    : QObject{parent}
    , server_address(server_address), server_port(server_port)
{

}

void TrodesInterface::run()
{
    network_processing_loop(server_address, server_port);


    // NOTE - network_processing_loop returns, so this function returns. The benefit of this is that we can process signals
    //        and slots. The cost is that it makes communication with the networking more complicated. 
    
    // emit finished();
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