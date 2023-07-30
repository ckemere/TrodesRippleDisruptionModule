#ifndef STIMINTERFACE_H
#define STIMINTERFACE_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>

#define DEFAULT_STIM_SERVER_PORT 12345


class StimInterface : public QObject
{
    Q_OBJECT

public:
    enum StimIFaceStatus {invalid_url, trying_to_connect, connected};

    StimInterface(QObject *parent);

public slots:
    void run();
    void updateAddress(QString, quint16);
    void testStimulation();

signals:
    void stimStatusUpdate(StimIFaceStatus newStatus);
    void finished();
    void error(QString err);

private slots:
    void stateMotivator();
    void checkServer();
    void readPendingDatagrams();    

private:
    StimIFaceStatus current_status;
    bool url_initialized;

    QUdpSocket *socket;
    QHostAddress server_address;
    quint16 server_port;

    QTimer *stateMotivatorTimer;

};

#endif // STIMINTERFACE_H
