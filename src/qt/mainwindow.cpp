#include "qt/mainwindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "version.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    QString appName =
        QString("Nikon Ti Control (%1)").arg(gitTagVersion.c_str());
    setWindowTitle(appName);

    //
    // centralWidget: [activityBar, contentArea]
    //
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *layout = new QHBoxLayout(centralWidget);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignLeft);

    navBar = new NavBar;
    contentArea = new QStackedWidget;

    layout->addWidget(navBar);
    layout->addWidget(contentArea);

    //
    // contentArea: [samplePage, acquirePage, dataPage]
    //
    setupPage = new SetupPage;
    acquirePage = new AcquirePage;
    dataPage = new DataPage;

    contentArea->addWidget(setupPage);
    contentArea->addWidget(acquirePage);
    contentArea->addWidget(dataPage);

    showAcquirePage();

    connect(navBar->buttonSetup, &QPushButton::clicked, this,
            &MainWindow::showSetupPage);
    connect(navBar->buttonAcquire, &QPushButton::clicked, this,
            &MainWindow::showAcquirePage);
    connect(navBar->ButtonData, &QPushButton::clicked, this,
            &MainWindow::showDataPage);

    updateLabels();
}

MainWindow::~MainWindow()
{
    delete devControlModel;
    delete expControlModel;
}

void MainWindow::setDeviceHub(DeviceHub *dev)
{
    devControlModel = new DeviceControlModel(dev);
    acquirePage->setDeviceControlModel(devControlModel);
}

void MainWindow::setExperimentControl(ExperimentControl *imagingControl)
{
    expControlModel = new ExperimentControlModel(imagingControl);
    acquirePage->setExperimentControlModel(expControlModel);
}

void MainWindow::showSetupPage()
{
    navBar->setCurrentButton(navBar->buttonSetup);
    contentArea->setCurrentWidget(setupPage);
}

void MainWindow::showAcquirePage()
{
    navBar->setCurrentButton(navBar->buttonAcquire);
    contentArea->setCurrentWidget(acquirePage);
}

void MainWindow::showDataPage()
{
    navBar->setCurrentButton(navBar->ButtonData);
    contentArea->setCurrentWidget(dataPage);
}

void MainWindow::updateLabels()
{
    QMap<QString, QMap<QString, QString>> propertyValueLabels;
    for (const auto &[property_path, value_label_map] : config.system.labels) {
        for (const auto &[value, label] : value_label_map) {
            propertyValueLabels[property_path.ToString().c_str()]
                               [value.c_str()] = label.name.c_str();
        }
    }
    acquirePage->deviceControlView->setPropertyValueLabels(propertyValueLabels);
}

NavBar::NavBar(QWidget *parent) : QWidget(parent)
{
    buttonSetup = new QPushButton("Setup", this);
    buttonAcquire = new QPushButton("Acquire", this);
    ButtonData = new QPushButton("Data", this);

    for (auto &button : {buttonSetup, buttonAcquire, ButtonData}) {
        button->setFixedSize(QSize(45, 45));

        QFont font = button->font();
        font.setPixelSize(10);
        button->setFont(font);
    }

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(3);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignTop);

    layout->addWidget(buttonSetup);
    layout->addWidget(buttonAcquire);
    layout->addWidget(ButtonData);
}

void NavBar::setCurrentButton(QPushButton *currentButton)
{
    for (auto &button : {buttonSetup, buttonAcquire, ButtonData}) {
        if (button == currentButton) {
            button->setDown(true);
            QFont font = button->font();
            font.setBold(true);
            button->setFont(font);
        } else {
            button->setDown(false);
            QFont font = button->font();
            font.setBold(false);
            button->setFont(font);
        }
    }
}
