#include "stiminterface.h"

#include <QDebug>
#include <QThread>
#include <QTimer>


StimInterface::StimInterface(QObject *parent = nullptr)
    : QObject{parent}, url_initialized(false)
{
    server_address = QHostAddress("0.0.0.0"); // default is localhost
    server_port = DEFAULT_STIM_SERVER_PORT; // Marker of an invalid_url

    // TODO: make the stim server info a parameter on the command line and load it in the constructor
}

void StimInterface::run()
{
    socket = new QUdpSocket(); // This will be how we communicate with Raspberry Pi Stim server
    socket->bind(QHostAddress::AnyIPv4, 7755);

    connect(socket, &QUdpSocket::readyRead, this, &StimInterface::readPendingDatagrams);

    // This QTimer will be how we control the simple state machine.
    stateMotivatorTimer = new QTimer();
    stateMotivatorTimer->setSingleShot(true);
    QObject::connect(stateMotivatorTimer, SIGNAL(timeout()), this, SLOT(stateMotivator()));

    // We have defaulted valid URL parameters, so we start in the "trying_to_connect" state.
    current_status = StimIFaceStatus::trying_to_connect;
    checkServer(); // tick the state machine to start going;
}

void StimInterface::stateMotivator()
{
    // This slot gets triggered by the stateMotivatorTimer.
    // Depending on our current state, we should go forward appropriately.
    switch (current_status) {
        case (StimIFaceStatus::trying_to_connect): {
            // We timed out waiting for a readyRead after checkServer()
            // We won't change our status, we'll just try again!
            emit stimStatusUpdate(current_status); // Send an update signal
            checkServer(); // restart the check
            break;
        }
        case (StimIFaceStatus::connected): {
            // This means that we got a UDP reply before the timeout.
            // We've already emitted the right signal, so we should just
            //   restart the timer to keep going on periodic checks.
            stateMotivatorTimer->start(500);
            break;
        }
        case (StimIFaceStatus::invalid_url): {
            // This means that in the interim we got a message from the GUI that indicated
            //   that the user was now in an invalid_url state.
            // At this point, we want to let the timer continue to be stopped.
            break;
        }        
    }
}

void StimInterface::checkServer()
{
    qint64 ret = socket->writeDatagram("C", 2, server_address, server_port); // Send the C command to the server. This should trigger a reply.
    stateMotivatorTimer->start(500);
    // qDebug() << "Sent a C command to " << server_address << "port " << server_port << "ret" << ret;
    // qDebug() << "Error" << socket->error() << ". Error string" << socket->errorString();
}

void StimInterface::readPendingDatagrams()
{
    char data[2];
    while (socket->hasPendingDatagrams()) {
        qint64 size = socket->readDatagram(data, 2); // I know who's sending me stuff!
    }
    if (data[0] == 'A') {
        current_status = StimIFaceStatus::connected;
        emit stimStatusUpdate(current_status);
        stateMotivatorTimer->stop();
        stateMotivatorTimer->start(500);
    }
}

void StimInterface::updateAddress(QString newAddress, quint16 port)
{
    server_address = QHostAddress(newAddress);
    server_port = port;
    if (port == 0) // flag that url is invalid
    {
        current_status = StimIFaceStatus::invalid_url; // this will stop the periodic checks
    }
    else if (current_status == StimIFaceStatus::invalid_url) { // we had previously stopped
            current_status = StimIFaceStatus::trying_to_connect;
            checkServer(); // restart checking server
    }
    // qDebug() << "Got new params! " << newAddress << port << QThread::currentThreadId();
}


void StimInterface::testStimulation() // should be length of stimulation?
{
    if (current_status == StimIFaceStatus::connected) { // after initialization
        socket->writeDatagram("T", 2, server_address, server_port); // Send the T command to the server. 
        qDebug() << "Sent a stim trigger command.";
    }
}