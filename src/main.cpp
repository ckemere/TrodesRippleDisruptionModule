#include "mainwindow.h"
#include "trodesinterface.h"
#include <QApplication>
#include <QThread>
#include <QTimer>

#include <vector>
#include <string>
#include <iostream>

int main(int argc, char *argv[])
{

    // std::string server_address = "tcp://127.0.0.1";
    // int server_port = 10000;
    // std::string config_filename;

    // std::vector<std::string> all_args;

    // if (argc > 1) {
    //     all_args.assign(argv + 1, argv + argc);
    //     std::vector<std::string>::iterator iter = all_args.begin();

    //     while (iter != all_args.end()) {
    //         if ((iter->compare("-serverAddress")==0) && (iter != all_args.end())) {
    //             iter++;
    //             server_address = *iter;
    //         } else if ((iter->compare("-serverPort")==0) && (iter != all_args.end())) {
    //             iter++;
    //             auto server_port_str = *iter;
    //             try {
    //                 server_port = std::stoi(server_port_str);
                    
    //             }
    //             catch (std::invalid_argument const& ex)
    //             {
    //                 std::cerr << "Invalid server port number." << std::endl;
    //             }

    //         } else if ((iter->compare("-trodesConfig")==0) && (iter != all_args.end())) {
    //             iter++;
    //             config_filename = *iter;
    //         }
    //         // else if ((arguments.at(optionInd).compare("-trodesConfig",Qt::CaseInsensitive)==0) && (arguments.length() > optionInd+1)) {
    //         iter++;
    //     }
        
    // }

    QApplication a(argc, argv);
    MainWindow w(nullptr, a.arguments());
    w.show();
    
    // TrodesInterface trodesInterface(&w, w.server_address.toStdString(), w.server_port);
    TrodesInterface trodesInterface(nullptr, w.server_address.toStdString(), w.server_port);

    QThread* interface_thread = new QThread;
    trodesInterface.moveToThread(interface_thread);
    // connect(trodesInterface, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    QObject::connect(interface_thread, SIGNAL(started()), &trodesInterface, SLOT(run()));
    QObject::connect(&trodesInterface, SIGNAL(finished()), interface_thread, SLOT(quit()));
    QObject::connect(&trodesInterface, SIGNAL(finished()), &trodesInterface, SLOT(deleteLater()));
    QObject::connect(interface_thread, SIGNAL(finished()), interface_thread, SLOT(deleteLater()));

    QObject::connect(&w, SIGNAL(updateParametersButton_clicked()), &trodesInterface, SLOT(updateParameters()));
    QObject::connect(&trodesInterface, SIGNAL(parametersUpdated()), &w, SLOT(reflectParametersUpdated()));
    QObject::connect(&trodesInterface, &TrodesInterface::networkStatus, 
                     &w, &MainWindow::networkStatusUpdate);

    QTimer statusUpdateTimer;
    QObject::connect(&statusUpdateTimer, SIGNAL(timeout()), &trodesInterface, SLOT(updateNetworkStatus()));
    statusUpdateTimer.start(250);

    interface_thread->start();

    return a.exec();
}
