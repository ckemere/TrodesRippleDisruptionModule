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


#include <Trodes/TimestampUtil.h>

// #include <chrono>
#include <iostream>
// #include <sstream>
#include <thread>
#include <vector>
#include <string>
#include <atomic>

// Global variables for communicating between TrodesNet thread and QT
std::atomic<TrodesInterface::TrodesNetworkStatus> trodesNetworkStatus;

// Parameters
std::atomic<double> rippleThreshold;
std::atomic<int>    numActiveChannels;
std::atomic<double> postDetectionDelay;

void network_processing_loop (std::string server_address, int server_port) {


    std::thread lfp_listener([ address = server_address, port = server_port]() {
        trodes::network::Connection c(address, port);
        std::string endpoint;

        std::cerr << "[lfp listener] thread starting!" << std::endl;

        while ((endpoint = c.get_endpoint("source.lfp")) == "") {
            std::cerr << "[lfp listener] Endpoint `source.lfp` is not available on the network yet. Retrying in 500ms..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        trodes::network::RawSourceSubscriber<trodes::network::TrodesLFPData> lfp(endpoint);

        // trodes::network::SourceSubscriber<trodes::network::TrodesLFPData> lfp(address, port, "source.lfp");

        std::stringstream result;

        while (true) {
            auto recvd = lfp.receive();
            result << "L: ";
            result << recvd.localTimestamp << " ";
            result << recvd.systemTimestamp << " ";
            for (auto i : recvd.lfpData) {
                result << i << ",";
            }
            result << "\n";
            // std::cerr << "[lfp listener]" << result.str();
            result.str({});
            result.clear();
        }

    });

    lfp_listener.detach();


    std::thread acq_listener([ address = server_address, port = server_port]() {
        trodes::network::Connection c(address, port);
        std::string endpoint;

        std::cerr << "[acq listener] thread starting!" << std::endl;

        while ((endpoint = c.get_endpoint("trodes.acquisition")) == "") {
            std::cerr << "[acq listener] Endpoint `trodes.acquisition` is not available on the network yet. Retrying in 500ms..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        trodes::network::RawSourceSubscriber<trodes::network::AcquisitionCommand> acq(endpoint);

        std::stringstream result;

        while (true) {
            auto recvd = acq.receive();
            std::cerr << "[trodes.acquisition]" << recvd.command;
            result.str({});
            result.clear();
        }

    });

    acq_listener.detach();


    std::thread sstatus_listener([ address = server_address, port = server_port]() {
        trodes::network::Connection c(address, port);
        std::string endpoint;

        std::cerr << "[trodes.source.pub] thread starting!" << std::endl;

        while ((endpoint = c.get_endpoint("trodes.source.pub")) == "") {
            std::cerr << "[acq listener] Endpoint `trodes.acquisition` is not available on the network yet. Retrying in 500ms..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        trodes::network::RawSourceSubscriber<trodes::network::SourceStatus> sstatus(endpoint);

        std::stringstream result;

        while (true) {
            auto recvd = sstatus.receive();
            std::cerr << "[trodes.source.pub]" << recvd.message;
            result.str({});
            result.clear();
        }

    });

    sstatus_listener.detach();
}

TrodesInterface::TrodesInterface(QObject *parent = nullptr, std::string server_address = "127.0.0.1", int server_port = 10000)
    : QObject{parent}
{
    network_processing_loop(server_address, server_port);
}
