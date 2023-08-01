#ifndef TRODESINTERFACE_H
#define TRODESINTERFACE_H

#include <QObject>
#include <string>

#include "ZmqSourceSubscriber.h"
#include <TrodesNetwork/Connection.h>
#include <TrodesNetwork/Generated/AcquisitionCommand.h>
#include <TrodesNetwork/Generated/SourceStatus.h>
#include <TrodesNetwork/Generated/TrodesLFPData.h>

#include <QSocketNotifier>

#include <thread>


class TrodesInterface : public QObject
{
    Q_OBJECT

public:
    enum TrodesNetworkStatus {not_connected, connected, streaming};

    // explicit TrodesInterface(QObject *parent = nullptr, std::string server_address, int server_port);
    TrodesInterface(QObject *parent, std::string server_address, int server_port);

public slots:
    void updateParameters();
    void updateNetworkStatus();
    void newRippleChannels(QList<int>);
    void run(void);

    void sstatus_activity();
    void acq_activity();

    void finish();

signals:
    void networkStatus(TrodesNetworkStatus newStatus);
    void parametersUpdated(void);
    void finished();
    void error(QString err);

private:
    TrodesNetworkStatus currentNetworkStatus;
    std::string server_address;
    int server_port;

    trodes::network::Connection *c;
    ZmqSourceSubscriber<trodes::network::SourceStatus> *sstatus;
    QSocketNotifier *sstatus_notifier;
    ZmqSourceSubscriber<trodes::network::AcquisitionCommand> *acq;
    QSocketNotifier *acq_notifier;

    ZmqSourceSubscriber<trodes::network::TrodesLFPData> *lfp;

    std::thread *lfp_thread;
    std::vector<int> ripple_channels;
};

#endif // TRODESINTERFACE_H