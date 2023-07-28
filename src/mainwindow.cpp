#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QString>
#include <QThread>

MainWindow::MainWindow(QWidget *parent, QStringList arguments)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->rejectionParamsGroupBox->setEnabled(false);

    QString config_filename = "";

    int optionInd = 0;
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

    TrodesConfiguration parsedConfiguration;
    QString errors = parsedConfiguration.readTrodesConfig(config_filename);
    qDebug() << "Read config with errors: " << errors;
    qDebug() << "Length of spikeConf" << parsedConfiguration.spikeConf.ntrodes.length();
    for (int i=0; i < parsedConfiguration.spikeConf.ntrodes.length(); i++) {
        qDebug() << "ID " << i << "is " << parsedConfiguration.spikeConf.ntrodes[i]->nTrodeId;
    }

    ui->tableWidget->setRowCount(parsedConfiguration.spikeConf.ntrodes.length());
    for (int i=0; i < parsedConfiguration.spikeConf.ntrodes.length(); i++) {
        ui->tableWidget->setItem(i, 0, 
        new QTableWidgetItem(QString::number(parsedConfiguration.spikeConf.ntrodes[i]->nTrodeId)));
    }


    ui->statusbar->showMessage("Establishing Trodes interface.");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_updateParametersButton_clicked()
{
    qDebug() << "Update parameters clicked! From thread " << QThread::currentThreadId();
    ui->updateParametersButton->setEnabled(false);
    emit updateParametersButton_clicked();
}

void MainWindow::reflectParametersUpdated()
{
    qDebug() << "Received response";
    ui->updateParametersButton->setEnabled(true);
}

void MainWindow::networkStatusUpdate(TrodesInterface::TrodesNetworkStatus status)
{
    switch (status){
        case TrodesInterface::TrodesNetworkStatus::not_connected:
            ui->statusbar->showMessage("Still waiting on Trodes interface.");
            break;
        case TrodesInterface::TrodesNetworkStatus::connected:
            ui->statusbar->showMessage("Trodes interface connected.");
            break;
        case TrodesInterface::TrodesNetworkStatus::streaming:
            ui->statusbar->showMessage("Streaming data.");
            break;
    }
}
