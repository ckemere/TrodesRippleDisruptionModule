#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->rejectionParamsGroupBox->setVisible(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_checkBox_4_stateChanged(int state)
{
    if (state)
        ui->rejectionParamsGroupBox->setVisible(true);
    else
        ui->rejectionParamsGroupBox->setVisible(false);
}

