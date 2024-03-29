#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "moduledefines.h"

#include <QString>
#include <QThread>
#include <QColor>
#include <QMessageBox>
#include <QUrl>
#include <QPalette>
#include <QTimer>

#include <cmath>

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

void TableRow::highlight(TableRow::Highlight highlight)
{
    switch (highlight) {
        case TableRow::Highlight::RippleChannel:
            id->setBackground(QColor(0x95, 0xd0, 0xfc)); // XKCD light blue from https://xkcd.com/color/rgb
            break;
        case TableRow::Highlight::NoiseChannel:
            id->setBackground(QColor(0xff, 0xd1, 0xdf)); // XKCD light pink from https://xkcd.com/color/rgb
            break;
        case TableRow::Highlight::None:
        default:
            id->setBackground(Qt::NoBrush);
            break;
    }
}

MainWindow::MainWindow(QWidget *parent, QList<int> nTrodeIds)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow), nTrodeIds(nTrodeIds)
    , trodesNetworkStatus(TrodesInterface::TrodesNetworkStatus::not_connected)
    , currentlyTraining(false), currentlyStimulating(false)
{
    ui->setupUi(this);

    redrawNTrodeTable();

    ui->tableWidget->horizontalHeaderItem(0)->setText("NTrode\nId");

    ui->trainingDurationSpinBox->setEnabled(false);
    ui->trainingDurationLabel->setEnabled(false);
    ui->trainingProgressBar->setEnabled(false);
    ui->trainingProgressLabel->setEnabled(false);
    ui->trainLFPStatisticsButton->setEnabled(false);
    ui->enableStimulationButton->setEnabled(false);

    ui->rippleParamGroupBox->setEnabled(false);
    ui->stimParamsGroupBox->setEnabled(false);
    ui->rejectionParamsGroupBox->setEnabled(false);


    ui->rippleThreshold->setValue(DEFAULT_RIPPLE_THRESHOLD);
    ui->numActiveChannels->setValue(DEFAULT_NUM_ACTIVE_CHANNELS);
    ui->minInterStim->setValue(DEFAULT_MINIMUM_STIM_ISI);
    ui->maxStimRate->setValue(DEFAULT_MAXIMUM_STIM_RATE);

    ui->noiseThreshold->setValue(DEFAULT_NOISE_THRESHOLD);


    ui->statusbar->showMessage("Establishing Trodes interface.");

    ui->raspberryPiLineEdit->setText(QString("udp://%1:%2").arg(DEFAULT_STIM_SERVER_ADDRESS, QString::number(DEFAULT_STIM_SERVER_PORT)));

    on_raspberryPiLineEdit_editingFinished(); // updates stim server interface status
}



MainWindow::~MainWindow()
{
    delete ui;
}

/* ================================================================================== */
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

/* ================================================================================== */
/* Code related to Trodes Interface */

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
            if (trodesNetworkStatus != TrodesInterface::TrodesNetworkStatus::streaming) {
                ui->rippleParamGroupBox->setEnabled(true);
                ui->stimParamsGroupBox->setEnabled(true);
                ui->rejectionParamsGroupBox->setEnabled(true);

                // Send updated parameters
                on_updateParametersButton_clicked();
            }            
            break;
    }
    trodesNetworkStatus = status;
}


void MainWindow::on_rippleThreshold_valueChanged(double newValue) {
    newParametersNotUpdated();
}

void MainWindow::on_numActiveChannels_valueChanged(int newValue) {
    newParametersNotUpdated();    
}

void MainWindow::on_minInterStim_valueChanged(double newValue) {
    newParametersNotUpdated();  
}

void MainWindow::on_maxStimRate_valueChanged(double newValue) {
    newParametersNotUpdated();   
}

void MainWindow::on_controlStimulationCheckBox_stateChanged(int state) {
    newParametersNotUpdated();   
}

void MainWindow::newParametersNotUpdated(void) {
    ui->rippleParamGroupBox->setStyleSheet("background-color: pink");
    ui->rejectionParamsGroupBox->setStyleSheet("background-color: pink");
    
    ui->updateParametersButton->setEnabled(true);
}

void MainWindow::on_updateParametersButton_clicked()
{
    /*
    struct RippleParameters {
        double ripple_threshold;
        unsigned int num_active_channels;
        double minimum_ripple_isi; // in samples
        double minimum_ripple_average_isi; // in samples
        bool post_detection_delay; 
    }
    */

    RippleParameters newParams = {ui->rippleThreshold->value(),
                                  (unsigned int)  ui->numActiveChannels->value(), 
                                  (unsigned int) (ui->minInterStim->value() * SAMPLES_PER_SECOND / 1000), // convert to samples from ms
                                  (unsigned int) (1 / ui->maxStimRate->value() * SAMPLES_PER_SECOND), // convert to samples from rate
                                  ui->controlStimulationCheckBox->checkState() == Qt::Checked,
                                  ui->noiseThreshold->value()};
    emit newParameters(newParams);
    qDebug() << "Update parameters clicked! From thread " << QThread::currentThreadId();
    ui->updateParametersButton->setEnabled(false);
    ui->rippleParamGroupBox->setStyleSheet("");
    ui->rejectionParamsGroupBox->setStyleSheet("");   
}

void MainWindow::reflectParametersUpdated()
{
    qDebug() << "Received response";
    ui->updateParametersButton->setEnabled(true);
}

/* Code related to the NTrode Selection Process*/

void MainWindow::redrawNTrodeTable(QList<unsigned int> order)
{
    if (ui->tableWidget->rowCount() != nTrodeIds.count()) 
        ui->tableWidget->setRowCount(nTrodeIds.count());


    // Delete old rows to avoid a memory leak
    for (int i=0; i < nTrodeTableRows.count(); i++)
        delete nTrodeTableRows[i];
    nTrodeTableRows.clear();

    if (order.isEmpty()) // Default order is natural
        for (unsigned int i=0; i < nTrodeIds.length(); i++)
            order.append(i);

    for (unsigned int i=0; i < order.length(); i++) {
        unsigned int idx = order[i];
        TableRow *row = new TableRow(QString::number(nTrodeIds[idx]), "", "", idx);
        nTrodeTableRows.append(row);
        row->setTableRow(ui->tableWidget, i);
        // Colorize table
        if (rippleNTrodeIndices.contains(idx))
            row->highlight(TableRow::Highlight::RippleChannel);
        else if (noiseNTrodeIndices.contains(idx))
            row->highlight(TableRow::Highlight::NoiseChannel);
        else
            row->highlight(TableRow::Highlight::None);
    }
}

void MainWindow::on_tableWidget_cellClicked(int row, int column)
{
    qDebug() << "Clicked a cell!";
    unsigned int nTrodeId_index = nTrodeTableRows[row]->id_index;
    if (column == 0) { // force click of ID cell
        // Process scenarios where an electrode has already been selected and unhighlight
        if (rippleNTrodeIndices.contains(nTrodeId_index)) {
            nTrodeTableRows[row]->highlight(TableRow::Highlight::None);
            rippleNTrodeIndices.removeOne(nTrodeId_index);
        }
        else if (noiseNTrodeIndices.contains(nTrodeId_index)) {
            nTrodeTableRows[row]->highlight(TableRow::Highlight::None);
            noiseNTrodeIndices.removeOne(nTrodeId_index);
        }
        // Process scenarios where an electrode is being selected now and highlight appropriately
        else {
            if (QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
                nTrodeTableRows[row]->highlight(TableRow::Highlight::NoiseChannel);
                noiseNTrodeIndices.append(nTrodeId_index);
            }
            else {
                nTrodeTableRows[row]->highlight(TableRow::Highlight::RippleChannel);
                rippleNTrodeIndices.append(nTrodeId_index);
            }
        }
    }
}

void MainWindow::on_freezeSelectionButton_clicked()
{   
    if (nTrodeTableFrozen) {
        QList<unsigned int> raw_order;
        for (int i = 0; i < nTrodeIds.length(); i++)
            raw_order.append(i);
        
        redrawNTrodeTable(raw_order);

        ui->freezeSelectionButton->setText("Freeze Selection");
        nTrodeTableFrozen = false;

        ui->trainingDurationSpinBox->setEnabled(false);
        ui->trainingDurationLabel->setEnabled(false);
        ui->trainingProgressBar->setEnabled(false);
        ui->trainingProgressLabel->setEnabled(false);
        ui->trainLFPStatisticsButton->setEnabled(false);
        ui->enableStimulationButton->setEnabled(false);
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
            QList<unsigned int> new_order;
            new_order.append(rippleNTrodeIndices);
            new_order.append(noiseNTrodeIndices);

            // Get unselected channels
            for (unsigned int i = 0; i < nTrodeIds.length(); i++) {
                if (!new_order.contains(i))
                new_order.append(i);
            }

            redrawNTrodeTable(new_order);

            ui->freezeSelectionButton->setText("Unfreeze Selection");
            nTrodeTableFrozen = true;

            emit newRippleChannels(rippleNTrodeIndices, noiseNTrodeIndices);

            ui->trainingDurationSpinBox->setEnabled(true);
            ui->trainingDurationLabel->setEnabled(true);
            ui->trainingProgressBar->setEnabled(true);
            ui->trainingProgressLabel->setEnabled(true);
            ui->trainLFPStatisticsButton->setEnabled(true);

        }
    }
}


void MainWindow::on_trainLFPStatisticsButton_clicked() 
{
    if (!currentlyTraining) {
        int training_duration_samples = ui->trainingDurationSpinBox->value() * SAMPLES_PER_SECOND;
        ui->trainingProgressBar->setRange(0, training_duration_samples);
        emit startTraining(training_duration_samples);
        qDebug() << "Start training!";
        currentlyTraining = true;
        ui->trainLFPStatisticsButton->setText("Abort Training");
    }
    else {
        ui->trainingProgressBar->reset();
        emit startTraining(0);
        qDebug() << "Aborting training!";
    }
}

void MainWindow::newRipplePowerData(std::vector<double> means, std::vector<double> vars, int training_left, double stim_isi)
{
    for (auto row : nTrodeTableRows) {
        unsigned int ch = row->id_index;
        row->setParams(means[ch], std::sqrt(vars[ch]));
        // row->setParams(means[ch], vars[ch]);
        // qDebug() << means[ch] << " "<< vars[ch];
    }

    // qDebug() << "Got training update " << training_left;
    if (currentlyTraining) {
        ui->trainingProgressBar->setValue(training_left);
        if ((training_left == 0) && currentlyTraining) {
            currentlyTraining = false;
            ui->trainLFPStatisticsButton->setText("Train LFP Statistics");

            // Once we've trained at least once, stimulation should be enabled
            ui->enableStimulationButton->setEnabled(true);
        }
    }
    else {
        if (currentlyStimulating)
            ui->rippleRateLabel->setText(QString::number(stim_isi));
    }
}

void MainWindow::on_enableStimulationButton_clicked() 
{
    if (currentlyStimulating) {
        emit enableStimulation(false);
        ui->enableStimulationButton->setText("Enable Stimulation");
        currentlyStimulating = false;
    }
    else {
        // Should test for things like training, what else?
        if (!currentlyTraining) {
            emit enableStimulation(true);
            ui->enableStimulationButton->setText("Stop Stimulation");
            currentlyStimulating = true;
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
