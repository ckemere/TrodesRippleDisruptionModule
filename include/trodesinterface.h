#ifndef TRODESINTERFACE_H
#define TRODESINTERFACE_H

#include <QObject>

class TrodesInterface : public QObject
{
    Q_OBJECT

public:
    enum TrodesNetworkStatus {not_connected, connected, streaming};

    explicit TrodesInterface(QObject *parent = nullptr);

    TrodesNetworkStatus getStatus();


public slots:

signals:
    void trodesNetworkStatusUpdate(TrodesNetworkStatus newStatus);

private:
    TrodesNetworkStatus currentNetworkStatus;

};

#endif // TRODESINTERFACE_H
