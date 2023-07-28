#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "trodesinterface.h"
#include "Trodes/src-config/configuration.h"


#include <QMainWindow>
#include <QString>
#include <QList>
#include <QTableWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, QStringList arguments = QStringList());
    ~MainWindow();

    void parseConfigurationFile(QString);

    QString server_address = "127.0.0.1";
    int server_port = 10000;

signals:
    void updateParametersButton_clicked();
public slots:
    void networkStatusUpdate(TrodesInterface::TrodesNetworkStatus);

private slots:
    void on_updateParametersButton_clicked();
    void on_tableWidget_cellClicked(int row, int column);
    void reflectParametersUpdated();

private:
    Ui::MainWindow *ui;
    int numNTrodes;
    QList<int> rippleNTrodeIndices;
    QList<int> noiseNTrodeIndices;

    QList<QTableWidgetItem *> nTrodeIdItems;
    QList<QTableWidgetItem *> meanPowerItems;
    QList<QTableWidgetItem *> stdPowerItems;


};
#endif // MAINWINDOW_H
