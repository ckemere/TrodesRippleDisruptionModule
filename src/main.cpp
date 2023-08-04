#include "mainwindow.h"
#include "trodesinterface.h"
#include "stiminterface.h"
#include <QApplication>
#include <QThread>
#include <QTimer>
#include <QList>

#include <vector>
#include <string>
#include <iostream>

#include "Trodes/src-config/configuration.h"


int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    QString config_filename = "";
    QString server_address = "127.0.0.1";
    int server_port = 10000;


    int optionInd = 0;
    auto arguments = a.arguments();
    while (optionInd < arguments.length()) {
        qDebug() << "Option: " << arguments.at(optionInd);
        if (arguments.length() > optionInd+1 )
            qDebug() << "Option(+1): " << arguments.at(optionInd+1);
        if ((arguments.at(optionInd).compare("-serverAddress",Qt::CaseInsensitive)==0) /*&& (arguments.length() > optionInd+1)*/) {
            server_address = arguments.at(optionInd+1);
            optionInd++;
        } else if ((arguments.at(optionInd).compare("-serverPort",Qt::CaseInsensitive)==0) && (arguments.length() > optionInd+1)) {
            server_port = arguments.at(optionInd+1).toInt();
            optionInd++;
        }
        else if ((arguments.at(optionInd).compare("-trodesConfig",Qt::CaseInsensitive)==0) && (arguments.length() > optionInd+1)) {
            config_filename = arguments.at(optionInd+1);
            optionInd++;
        }
        optionInd++;
    }
    qDebug() << "[RippleDisruption]" << "Server Address: " << server_address <<"Server Port: " <<server_port <<"Config file name: " << config_filename;

    if (config_filename == "") {
        qDebug() << "[RippleDisruption] Fatal error. Config file name [-trodesConfig] option is required";
        return -1;
    }
    
    TrodesConfiguration parsedConfiguration;
    QString errors = parsedConfiguration.readTrodesConfig(config_filename);

    if ((parsedConfiguration.hardwareConf.sourceSamplingRate != 30000) || (parsedConfiguration.hardwareConf.lfpSubsamplingInterval != 20)) {
        qDebug() << "[RippleDisruption] Fatal error." << 
            "LFP sampling rate has to be 1500. Expecting samplingRate of 30k and lfpSubsamplingInterval of 20!";
        return -1;
    }
          
    if (parsedConfiguration.spikeConf.ntrodes.length() < 1) {
        qDebug() << "[RippleDisruption] Fatal error. Config file contains no lfp channels.";
    }

    QList<int> nTrodeIds;
    for (unsigned int i = 0; i < parsedConfiguration.spikeConf.ntrodes.size(); i++) {
        nTrodeIds.append(parsedConfiguration.spikeConf.ntrodes[i]->nTrodeId);
    }

    MainWindow w(nullptr, nTrodeIds);
    w.show();
    
    // TrodesInterface trodesInterface(&w, w.server_address.toStdString(), w.server_port);
    TrodesInterface trodesInterface(nullptr, server_address.toStdString(), server_port, parsedConfiguration.spikeConf.ntrodes.length());

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
