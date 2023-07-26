#include "trodesinterface.h"

#include <atomic>

// Global variables for communicating between TrodesNet thread and QT
std::atomic<TrodesInterface::TrodesNetworkStatus> trodesNetworkStatus;

// Parameters
std::atomic<double> rippleThreshold;
std::atomic<int>    numActiveChannels;
std::atomic<double> postDetectionDelay;

void network_processing_loop () {

}

TrodesInterface::TrodesInterface(QObject *parent)
    : QObject{parent}
{

}
