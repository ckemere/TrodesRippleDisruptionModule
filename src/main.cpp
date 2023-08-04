#include "mainwindow.h"
#include "trodesinterface.h"
#include "stiminterface.h"
#include <QApplication>
#include <QThread>
#include <QTimer>

#include <vector>
#include <string>
#include <iostream>

int main(int argc, char *argv[])
{


    QApplication a(argc, argv);
    MainWindow w(nullptr, a.arguments());
    w.show();
    
    // TrodesInterface trodesInterface(&w, w.server_address.toStdString(), w.server_port);
    TrodesInterface trodesInterface(nullptr, w.server_address.toStdString(), w.server_port);

    QThread* interface_thread = new QThread;
    trodesInterface.moveToThread(interface_thread);
    // connect(trodesInterface, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    QObject::connect(interface_thread, SIGNAL(started()), &trodesInterface, SLOT(run()));
    QObject::connect(&w, &MainWindow::appClosing, &trodesInterface, &TrodesInterface::finish);
    // QObject::connect(&trodesInterface, SIGNAL(finished()), interface_thread, SLOT(quit()));
    // QObject::connect(&trodesInterface, SIGNAL(finished()), &trodesInterface, SLOT(deleteLater()));
    // QObject::connect(interface_thread, SIGNAL(finished()), interface_thread, SLOT(deleteLater()));

    QObject::connect(&w, SIGNAL(updateParametersButton_clicked()), &trodesInterface, SLOT(updateParameters()));
    QObject::connect(&trodesInterface, SIGNAL(parametersUpdated()), &w, SLOT(reflectParametersUpdated()));
    QObject::connect(&w, &MainWindow::newRippleChannels, &trodesInterface, &TrodesInterface::newRippleChannels);
    QObject::connect(&trodesInterface, &TrodesInterface::networkStatus, &w, &MainWindow::networkStatusUpdate);
    QObject::connect(&trodesInterface, &TrodesInterface::newTrainingStats, &w, &MainWindow::newRipplePowerData);
    QObject::connect(&w, &MainWindow::startTraining, &trodesInterface, &TrodesInterface::startTraining);
    QObject::connect(&w, &MainWindow::enableStimulation, &trodesInterface, &TrodesInterface::enableStimulation);
    
    // QObject::connect(&w, &MainWindow::newRippleChannels, &trodesInterface, &TrodesInterface::newRippleChannels, Qt::QueuedConnection);

    QTimer statusUpdateTimer;
    QObject::connect(&statusUpdateTimer, SIGNAL(timeout()), &trodesInterface, SLOT(updateNetworkStatus()));
    statusUpdateTimer.start(250);
    interface_thread->start();


    StimInterface stimInterface(nullptr);
    QThread* stim_thread = new QThread;
    stimInterface.moveToThread(stim_thread);
    // connect(trodesInterface, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    QObject::connect(stim_thread, SIGNAL(started()), &stimInterface, SLOT(run()));
    // QObject::connect(&stimInterface, SIGNAL(finished()), stim_thread, SLOT(quit()));
    // QObject::connect(&stimInterface, SIGNAL(finished()), &stimInterface, SLOT(deleteLater()));
    // QObject::connect(stim_thread, SIGNAL(finished()), stim_thread, SLOT(deleteLater()));

    // QObject::connect(&w, SIGNAL(updateParametersButton_clicked()), &stimInterface, SLOT(updateParameters()));
    QObject::connect(&stimInterface, &StimInterface::stimStatusUpdate, &w, &MainWindow::stimServerStatusUpdate);
    QObject::connect(&w, &MainWindow::updatedStimServerUrl, &stimInterface, &StimInterface::updateAddress);
    QObject::connect(&w, &MainWindow::testStimulation, &stimInterface, &StimInterface::testStimulation);
    stim_thread->start();


    return a.exec();
}
