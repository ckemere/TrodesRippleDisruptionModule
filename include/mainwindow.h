#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "trodesinterface.h"
#include "stiminterface.h"
#include "Trodes/src-config/configuration.h"


#include <QMainWindow>
#include <QString>
#include <QList>
#include <QTableWidgetItem>
#include <QMessageBox>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TableRow
{
public:
    TableRow(QString idStr = "", QString meanStr = "", QString stdStr = "", int id_index = 0);
    ~TableRow();

public:
    void setTableRow(QTableWidget *table, int row);
    void highlight(bool highlight = true);
    void setParams(double m, double s) {mean_val = m; std_val = s; mean->setData(Qt::DisplayRole,m); std->setData(Qt::DisplayRole,s);}

    int id_val;
    double mean_val;
    double std_val;
    int id_index;

private:
    QTableWidgetItem *id;
    QTableWidgetItem *mean;
    QTableWidgetItem *std;
};

// struct RippleParameters
// {
//     double rippleThreshold; // in std devs over mean
//     int numberOfChannels; // voting requirement

//     vector whichChannels; // need to figure out

//     double noiseThreshold;
//     int noiseNumberOfChannels;
//     vector noiseChannels;
// }

// struct RippleData
// {
//     vector means;
//     vector stds;
// }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, QStringList arguments = QStringList());
    ~MainWindow();


    void redrawNTrodeTable(QList<unsigned int> order = QList<unsigned int>());

    QString server_address = "127.0.0.1";
    int server_port = 10000;

signals:
    void updateParametersButton_clicked();
    void updatedStimServerUrl(QString, quint16);
    void testStimulation();
    void appClosing();
    void newRippleChannels(QList<unsigned int>);
    void startTraining(unsigned int);

public slots:
    void networkStatusUpdate(TrodesInterface::TrodesNetworkStatus);
    void stimServerStatusUpdate(StimInterface::StimIFaceStatus newStatus);
    void closeEvent(QCloseEvent *event);

    void newRipplePowerData(std::vector<double>, std::vector<double>, int);

private slots:
    void on_updateParametersButton_clicked();
    void on_tableWidget_cellClicked(int row, int column);
    void on_freezeSelectionButton_clicked();
    void reflectParametersUpdated();
    void on_raspberryPiLineEdit_editingFinished();
    void on_testStimButton_clicked();
    void on_trainLFPStatisticsButton_clicked();

private:
    Ui::MainWindow *ui;
    // int numNTrodes;
    QList<int> nTrodeIds; // loaded from config file
    bool nTrodeTableFrozen = false;

    QList<unsigned int> rippleNTrodeIndices; // indices into nTrodeIds
    QList<unsigned int> noiseNTrodeIndices; // indices into nTrodeIds


    QList<TableRow*> nTrodeTableRows; // For display

    QMessageBox *startupErrorMsgBox;

    bool currentlyTraining;

};
#endif // MAINWINDOW_H
