#ifndef TRODESINTERFACE_H
#define TRODESINTERFACE_H

#include <QObject>

class TrodesInterface : public QObject
{
    Q_OBJECT

public:
    enum TrodesNetworkStatus {not_connected, connected, streaming};

    // explicit TrodesInterface(QObject *parent = nullptr, std::string server_address, int server_port);
    TrodesInterface(QObject *parent, std::string server_address, int server_port);

    TrodesNetworkStatus getStatus();


public slots:

signals:
    void trodesNetworkStatusUpdate(TrodesNetworkStatus newStatus);

private:
    TrodesNetworkStatus currentNetworkStatus;

};

#endif // TRODESINTERFACE_H
