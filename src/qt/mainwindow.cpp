#include "qt/mainwindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    const QString appName = "Nikon Ti Control";
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

    testData();
}

MainWindow::~MainWindow()
{
    delete deviceControlModel;
    delete imagingControlModel;
}

void MainWindow::setDeviceHub(DeviceHub *hub)
{
    deviceControlModel = new DeviceControlModel(hub);
    acquirePage->setDeviceControlModel(deviceControlModel);
}

void MainWindow::setImagingControl(ImagingControl *imagingControl)
{
    imagingControlModel = new ImagingControlModel(imagingControl);
    acquirePage->setImagingControlModel(imagingControlModel);
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


void MainWindow::testData()
{

    //     emit devicePropertyUpdate("/NikonTi/ZDrivePosition", "501.675");
    //     emit devicePropertyUpdate("/PriorProScan/FilterWheel1", "3");
    //     emit devicePropertyUpdate("/PriorProScan/FilterWheel3", "10");
    //     emit devicePropertyUpdate("/PriorProScan/XYPosition",
    //     "14937.5,-11022.5"); emit
    //     devicePropertyUpdate("/NikonTi/FilterBlock1", "2"); emit
    //     devicePropertyUpdate("/NikonTi/NosePiece", "3"); emit
    //     devicePropertyUpdate("/NikonTi/LightPath", "2"); emit
    //     devicePropertyUpdate("/NikonTi/PFSOffset", "55.2"); emit
    //     devicePropertyUpdate("/NikonTi/PFSState", "Off"); emit
    //     devicePropertyUpdate("/NikonTi/DiaShutter", "On"); emit
    //     devicePropertyUpdate("/PriorProScan/LumenShutter", "Off");

    QMap<QString, QMap<QString, QString>> propertyValueLabels = {
        {"/NikonTi/NosePiece",
         {{"1", "10x"},
          {"2", "40x"},
          {"3", "60xO"},
          {"4", "100xO"},
          {"5", "---"},
          {"6", "---"}}},
        {"/NikonTi/FilterBlock1",
         {
             {"1", "---"},
             {"2", "BCYR"},
             {"3", "CFP"},
             {"4", "GFP"},
             {"5", "YFP"},
             {"6", "RFP"},
         }},
        {"/PriorProScan/FilterWheel1",
         {
             {"1", "---"},
             {"2", "RFP"},
             {"3", "BFP"},
             {"4", "YFP"},
             {"5", "---"},
             {"6", "---"},
         }},
        {"/PriorProScan/FilterWheel3",
         {
             {"1", "RFP"},
             {"2", "BFP"},
             {"3", "YFP"},
             {"4", "CFP"},
             {"5", "---"},
             {"6", "Dark"},
             {"7", "---"},
             {"8", "---"},
             {"9", "---"},
             {"10", "---"},
         }},
        {"/NikonTi/LightPath",
         {
             {"1", "E100"},
             {"2", "L100"},
             {"3", "R100"},
             {"4", "L80"},
         }}};

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
