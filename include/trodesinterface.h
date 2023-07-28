#ifndef TRODESINTERFACE_H
#define TRODESINTERFACE_H

#include <QObject>
#include <string>

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
    void run(void);

signals:
    void networkStatus(TrodesNetworkStatus newStatus);
    void parametersUpdated(void);
    void finished();
    void error(QString err);

private:
    TrodesNetworkStatus currentNetworkStatus;
    std::string server_address;
    int server_port;

};

#endif // TRODESINTERFACE_H
