#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

#include "device/devicehub.h"
#include "experimentcontrol.h"

#include "qt/acquirepage.h"
#include "qt/datapage.h"
#include "qt/setuppage.h"

#include "qt/devicecontrol_model.h"
#include "qt/experimentcontrol_model.h"

class NavBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setDeviceHub(DeviceHub *dev);
    void setExperimentControl(ExperimentControl *exp);
    void updateLabels();

public slots:
    void showSetupPage();
    void showAcquirePage();
    void showDataPage();

public:
    NavBar *navBar;

    QStackedWidget *contentArea;
    SetupPage *setupPage;
    AcquirePage *acquirePage;
    DataPage *dataPage;

    DeviceControlModel *devControlModel;
    ExperimentControlModel *expControlModel;
};

class NavBar : public QWidget {
    Q_OBJECT
public:
    explicit NavBar(QWidget *parent = nullptr);

    void setCurrentButton(QPushButton *currentButton);

public:
    QPushButton *buttonSetup;
    QPushButton *buttonAcquire;
    QPushButton *ButtonData;
};

#endif // MAINWINDOW_H
