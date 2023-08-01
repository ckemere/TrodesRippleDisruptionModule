#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "trodesinterface.h"
#include "stiminterface.h"
#include "Trodes/src-config/configuration.h"


#include <QMainWindow>
#include <QString>
#include <QList>
#include <QTableWidgetItem>

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


    void redrawNTrodeTable(QList<int> order = QList<int>());

    QString server_address = "127.0.0.1";
    int server_port = 10000;

signals:
    void updateParametersButton_clicked();
    void updatedStimServerUrl(QString, quint16);
    void testStimulation();
    void appClosing();

public slots:
    void networkStatusUpdate(TrodesInterface::TrodesNetworkStatus);
    void stimServerStatusUpdate(StimInterface::StimIFaceStatus newStatus);
    void closeEvent(QCloseEvent *event);

private slots:
    void on_updateParametersButton_clicked();
    void on_tableWidget_cellClicked(int row, int column);
    void on_freezeSelectionButton_clicked();
    void reflectParametersUpdated();
    void on_raspberryPiLineEdit_editingFinished();
    void on_testStimButton_clicked();

private:
    Ui::MainWindow *ui;
    // int numNTrodes;
    QList<int> nTrodeIds; // loaded from config file
    bool nTrodeTableFrozen = false;

    QList<int> rippleNTrodeIndices; // indices into nTrodeIds
    QList<int> noiseNTrodeIndices; // indices into nTrodeIds


    QList<TableRow*> nTrodeTableRows; // For display

};
#endif // MAINWINDOW_H
