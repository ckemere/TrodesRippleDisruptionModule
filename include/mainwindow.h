#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "trodesinterface.h"
#include "stiminterface.h"
#include "moduledefines.h"


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
    enum Highlight {None, RippleChannel, NoiseChannel};

public:
    void setTableRow(QTableWidget *table, int row);
    void highlight(Highlight highlight = None);
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
    MainWindow(QWidget *parent = nullptr, QList<int> = QList<int>());
    ~MainWindow();

    void redrawNTrodeTable(QList<unsigned int> order = QList<unsigned int>());

signals:
    // void updateParametersButton_clicked();
    void updatedStimServerUrl(QString, quint16);
    void testStimulation();
    void appClosing();
    void newRippleChannels(QList<unsigned int>, QList<unsigned int>);
    void startTraining(unsigned int);
    void newParameters(RippleParameters);
    void enableStimulation(bool);

public slots:
    void networkStatusUpdate(TrodesInterface::TrodesNetworkStatus);
    void stimServerStatusUpdate(StimInterface::StimIFaceStatus newStatus);
    void closeEvent(QCloseEvent *event);

    void newRipplePowerData(std::vector<double>, std::vector<double>, int, double);

private slots:
    void on_rippleThreshold_valueChanged(double newValue);
    void on_numActiveChannels_valueChanged(int newValue);
    void on_minInterStim_valueChanged(double newValue);
    void on_maxStimRate_valueChanged(double newValue);
    void on_controlStimulationCheckBox_stateChanged(int state);
    void newParametersNotUpdated(void);
    void on_updateParametersButton_clicked();
    void reflectParametersUpdated();

    void on_tableWidget_cellClicked(int row, int column);
    void on_freezeSelectionButton_clicked();

    void on_raspberryPiLineEdit_editingFinished();
    void on_testStimButton_clicked();
    void on_trainLFPStatisticsButton_clicked();
    void on_enableStimulationButton_clicked();

private:
    Ui::MainWindow *ui;
    
    TrodesInterface::TrodesNetworkStatus trodesNetworkStatus;

    QList<int> nTrodeIds; // loaded from config file
    bool nTrodeTableFrozen = false;

    QList<unsigned int> rippleNTrodeIndices; // indices into nTrodeIds
    QList<unsigned int> noiseNTrodeIndices; // indices into nTrodeIds


    QList<TableRow*> nTrodeTableRows; // For display

    QMessageBox *startupErrorMsgBox;

    bool currentlyTraining;
    bool currentlyStimulating;

};
#endif // MAINWINDOW_H
