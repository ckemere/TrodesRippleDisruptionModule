#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QString>
#include <QThread>
#include <QColor>
#include <QMessageBox>
#include <QUrl>
#include <QPalette>

TableRow::TableRow(QString idStr, QString meanStr, QString stdStr, int id_index)
: id_index(id_index), id(new QTableWidgetItem(idStr)), mean(new QTableWidgetItem(meanStr)), std(new QTableWidgetItem(stdStr))
{
    id->setFlags(Qt::ItemIsEnabled);
    mean->setFlags(Qt::ItemIsEnabled);
    std->setFlags(Qt::ItemIsEnabled);

    id->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
}

TableRow::~TableRow()
{
    delete id; delete mean; delete std;
}

void TableRow::setTableRow(QTableWidget *table, int row)
{
    table->setItem(row, 0, id);
    table->setItem(row, 1, mean);
    table->setItem(row, 2, std);
}

void TableRow::highlight(bool highlight)
{
    if (highlight) {
        id->setBackground(QColor(0x95, 0xd0, 0xfc)); // XKCD light blue from https://xkcd.com/color/rgb
    }
    else {
        id->setBackground(Qt::NoBrush);
    }
}

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
    qDebug() << "[RippleDisruption] Read config. Errors? " << errors;

    for (int i=0; i < parsedConfiguration.spikeConf.ntrodes.length(); i++) {
        nTrodeIds.append(parsedConfiguration.spikeConf.ntrodes[i]->nTrodeId);
    }

    redrawNTrodeTable();

    ui->tableWidget->horizontalHeaderItem(0)->setText("NTrode\nId");

    ui->statusbar->showMessage("Establishing Trodes interface.");
    on_raspberryPiLineEdit_editingFinished(); // updates trodes interface status
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

/* Code related to stimulation server */

void MainWindow::on_raspberryPiLineEdit_editingFinished() {
    QString url_string = ui->raspberryPiLineEdit->text();
    QUrl url = QUrl(url_string, QUrl::StrictMode);

    // Accomodate something like "raspilocal:20782"
    if ((!url.scheme().isEmpty()) && (url.host().isEmpty()))
        url = QUrl("udp://"+url_string, QUrl::StrictMode);

    // Accomodate things without the schmee like 10.38.0.1:20782
    if (!url.isValid()) { // Could be missing the pre-pended "udp://""
        url = QUrl("udp://"+url_string, QUrl::StrictMode); 
    }

    // Make sure there's a port and the scheme is 'udp' (don't let people do something else though it doesn't matter)
    if ((url.isValid()) && (url.port() > 0) && (url.scheme() == "udp")) {
        stimServerStatusUpdate(StimInterface::StimIFaceStatus::trying_to_connect);
        emit updatedStimServerUrl(url.host(), url.port());
    }
    else {
        stimServerStatusUpdate(StimInterface::StimIFaceStatus::invalid_url);
        emit updatedStimServerUrl("", 0); // 0 is the flag for invalid
    }
}

void MainWindow::stimServerStatusUpdate(StimInterface::StimIFaceStatus newStatus) {
    QPalette palette = ui->raspberryPiLineEdit->palette();
    palette.setColor(QPalette::Base, Qt::white);

    switch (newStatus) {
        // case StimInterface::StimIFaceStatus::unintialized:
        // {
        //     ui->raspberryPiStatusLabel->setText("Uninitialized.");
        //     ui->testStimButton->setEnabled(false);
        //     break;
        // }
        case StimInterface::StimIFaceStatus::trying_to_connect:
        {
            ui->raspberryPiStatusLabel->setText("Not connected.");
            ui->testStimButton->setEnabled(false);
            break;
        }
        case StimInterface::StimIFaceStatus::connected:
        {
            palette.setColor(QPalette::Base, QColor(0x96, 0xf9, 0x7b)); // XKCD light-green
            ui->raspberryPiStatusLabel->setText("Connected.");
            ui->testStimButton->setEnabled(true);
            break;
        }
        case StimInterface::StimIFaceStatus::invalid_url:
        {
            palette.setColor(QPalette::Base, QColor(0xff, 0xd1, 0xdf)); // XKCD light-pink
            ui->raspberryPiStatusLabel->setText("Invalid URL.");
            ui->testStimButton->setEnabled(false);
            break;
        }

    }
    ui->raspberryPiLineEdit->setPalette(palette);
}

void MainWindow::on_testStimButton_clicked()
{
    emit testStimulation();
}


/* Code related to the NTrode Selection Process*/

void MainWindow::redrawNTrodeTable(QList<int> order)
{
    if (ui->tableWidget->rowCount() != nTrodeIds.count()) 
        ui->tableWidget->setRowCount(nTrodeIds.count());


    // Delete old rows to avoid a memory leak
    for (int i=0; i < nTrodeTableRows.count(); i++)
        delete nTrodeTableRows[i];
    nTrodeTableRows.clear();

    if (order.isEmpty()) // Default order is natural
        for (int i=0; i < nTrodeIds.length(); i++)
            order.append(i);

    for (int i=0; i < order.length(); i++) {
        int idx = order[i];
        TableRow *row = new TableRow(QString::number(nTrodeIds[idx]), "", "", idx);
        nTrodeTableRows.append(row);
        row->setTableRow(ui->tableWidget, i);
        // Colorize table
        if (rippleNTrodeIndices.contains(idx))
            row->highlight(true);
        else
            row->highlight(false);
    }
}

void MainWindow::on_tableWidget_cellClicked(int row, int column)
{
    qDebug() << "Clicked a cell!";
    int nTrodeId_index = nTrodeTableRows[row]->id_index;
    if (column == 0) { // force click of ID cell
        if (rippleNTrodeIndices.contains(nTrodeId_index)) {
            // unhighlight!
            nTrodeTableRows[row]->highlight(false);
            rippleNTrodeIndices.removeOne(nTrodeId_index);
        }
        else {
            nTrodeTableRows[row]->highlight(true);
            rippleNTrodeIndices.append(nTrodeId_index);
        }
    }
}

void MainWindow::on_freezeSelectionButton_clicked()
{
    if (nTrodeTableFrozen) {
        QList<int> raw_order;
        for (int i = 0; i < nTrodeIds.length(); i++)
            raw_order.append(i);
        
        redrawNTrodeTable(raw_order);

        ui->freezeSelectionButton->setText("Freeze Selection");
        nTrodeTableFrozen = false;
    }
    else {
        if (rippleNTrodeIndices.isEmpty()) {
            QMessageBox msgBox(QMessageBox::Warning, "No electrodes chosen", 
                "No electrodes have been selected for real time processing.",
                QMessageBox::Ok);
            msgBox.exec();
        }
        else {
            // Push selected channels to the top
            QList<int> new_order;
            new_order.append(rippleNTrodeIndices);

            // Get unselected channels
            for (int i = 0; i < nTrodeIds.length(); i++) {
                if (!new_order.contains(i))
                new_order.append(i);
            }

            redrawNTrodeTable(new_order);

            ui->freezeSelectionButton->setText("Unfreeze Selection");
            nTrodeTableFrozen = true;
        }
    }
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    emit appClosing();
    // QSettings settings("MyCompany", "MyApp");
    // settings.setValue("geometry", saveGeometry());
    // settings.setValue("windowState", saveState());
    QMainWindow::closeEvent(event);
}
