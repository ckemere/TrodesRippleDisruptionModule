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
#include <QTimer>

#include <thread>

#include "moduledefines.h"


class TrodesInterface : public QObject
{
    Q_OBJECT

public:
    enum TrodesNetworkStatus {not_connected, connected, streaming};

    // explicit TrodesInterface(QObject *parent = nullptr, std::string server_address, int server_port);
    TrodesInterface(QObject *parent, std::string server_address, int server_port, unsigned int num_lfp_channels);

public slots:
    void updateParameters(RippleParameters);
    void updateNetworkStatus();
    void newRippleChannels(QList<unsigned int>, QList<unsigned int>);
    void run(void);
    
    void startTraining(unsigned int numberOfTrainingSamples);
    void enableStimulation(bool enable);

    void reportIFaceData();

    void sstatus_activity();
    void acq_activity();

    void stimServerUpdated(QString, quint16);

    void finish();

signals:
    void networkStatus(TrodesNetworkStatus newStatus);
    void parametersUpdated(void);
    void finished();
    void error(QString err);
    void newTrainingStats(std::vector<double>, std::vector<double>, int, double);

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

    unsigned int nchan_lfp;
    std::thread *lfp_thread;

    QTimer *ifaceUpdateTimer;

};

#endif // TRODESINTERFACE_H
