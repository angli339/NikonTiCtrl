#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "fmt/format.h"
#include "logging.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->label_statusTask->setText("ready");

    initHistogramView();
    initDeviceControlView();
    initTaskControlView();
}

MainWindow::~MainWindow()
{
    delete histAxisX;
    histAxisX = nullptr;

    delete histAxisY;
    histAxisY = nullptr;

    delete histSeries;
    histSeries = nullptr;

    delete histChart;
    histChart = nullptr;

    delete ui;
    ui = nullptr;
}

void MainWindow::initHistogramView()
{
    connect(ui->vminSlider, &QSlider::valueChanged,
            ui->glImageViewer, &GLImageViewer::setVmin);
    connect(ui->vmaxSlider, &QSlider::valueChanged,
            ui->glImageViewer, &GLImageViewer::setVmax);

    ui->histogramChartView->setRenderHint(QPainter::Antialiasing);

    histAxisX = new QValueAxis;
    histAxisY = new QValueAxis;
    histAxisX->setRange(0, 65535);
    histAxisY->setRange(0, 1);

    histChart = new QChart();
    histChart->legend()->hide();
    histChart->addAxis(histAxisX, Qt::AlignBottom);
    histChart->addAxis(histAxisY, Qt::AlignLeft);

    histSeries = new QLineSeries();

    histChart->addSeries(histSeries);

    histSeries->setUseOpenGL(true);
    histSeries->append(0, 0);
    histSeries->append(65535, 0);

    histSeries->attachAxis(histAxisX);
    histSeries->attachAxis(histAxisY);

    histChart->setMargins(QMargins(0,0,0,0));

    ui->histogramChartView->setChart(histChart);
}

void MainWindow::initDeviceControlView()
{
    filterBlock1Buttons = {
        {"1", ui->button_filterBlock1},
        {"2", ui->button_filterBlock2},
        {"3", ui->button_filterBlock3},
        {"4", ui->button_filterBlock4},
        {"5", ui->button_filterBlock5},
        {"6", ui->button_filterBlock6},
    };

    lightPathButtons = {
        {"1", ui->button_lightPath1},
        {"2", ui->button_lightPath2},
        {"3", ui->button_lightPath3},
        {"4", ui->button_lightPath4},
    };

    nosePieceButtons = {
        {"1", ui->button_nosePiece1},
        {"2", ui->button_nosePiece2},
        {"3", ui->button_nosePiece3},
        {"4", ui->button_nosePiece4},
        {"5", ui->button_nosePiece5},
        {"6", ui->button_nosePiece6},
    };

    diaShutterButtons = {
        {"On", ui->button_diaShutterOn},
        {"Off", ui->button_diaShutterOff},
    };

    pfsStateButtons = {
        {"On", ui->button_PFSStateOn},
        {"Off", ui->button_PFSStateOff},
    };

    excitationFilterButtons = {
        {"1", ui->button_excitationFilter1},
        {"2", ui->button_excitationFilter2},
        {"3", ui->button_excitationFilter3},
        {"4", ui->button_excitationFilter4},
        {"5", ui->button_excitationFilter5},
        {"6", ui->button_excitationFilter6},
    };

    emissionFilterButtons = {
        {"1", ui->button_emissionFilter1},
        {"2", ui->button_emissionFilter2},
        {"3", ui->button_emissionFilter3},
        {"4", ui->button_emissionFilter4},
        {"5", ui->button_emissionFilter5},
        {"6", ui->button_emissionFilter6},
        {"7", ui->button_emissionFilter7},
        {"8", ui->button_emissionFilter8},
        {"9", ui->button_emissionFilter9},
        {"10", ui->button_emissionFilter10},
    };

    setButtonGroupEnabled(filterBlock1Buttons, false);
    setButtonGroupEnabled(lightPathButtons, false);
    setButtonGroupEnabled(nosePieceButtons, false);
    setButtonGroupEnabled(diaShutterButtons, false);
    setButtonGroupEnabled(pfsStateButtons, false);

    setButtonGroupEnabled(excitationFilterButtons, false);
    setButtonGroupEnabled(emissionFilterButtons, false);

    // Connect device property buttons
    for (const auto& kv : filterBlock1Buttons) {
        std::string value = kv.first;
        auto button = kv.second;
        connect(button, &QPushButton::clicked, [this, value]{
            emit requestDevicePropertySet("NikonTi/FilterBlock1", value);
        });
    }
    for (const auto& kv : lightPathButtons) {
        std::string value = kv.first;
        auto button = kv.second;
        connect(button, &QPushButton::clicked, [this, value]{
            emit requestDevicePropertySet("NikonTi/LightPath", value);
        });
    }
    for (const auto& kv : nosePieceButtons) {
        std::string value = kv.first;
        auto button = kv.second;
        connect(button, &QPushButton::clicked, [this, value]{
            emit requestDevicePropertySet("NikonTi/NosePiece", value);
        });
    }
    for (const auto& kv : diaShutterButtons) {
        std::string value = kv.first;
        auto button = kv.second;
        connect(button, &QPushButton::clicked, [this, value]{
            emit requestDevicePropertySet("NikonTi/DiaShutter", value);
        });
    }
    for (const auto& kv : pfsStateButtons) {
        std::string value = kv.first;
        auto button = kv.second;
        connect(button, &QPushButton::clicked, [this, value]{
            emit requestDevicePropertySet("NikonTi/PFSState", value);
        });
    }
    for (const auto& kv : excitationFilterButtons) {
        std::string value = kv.first;
        auto button = kv.second;
        connect(button, &QPushButton::clicked, [this, value]{
            emit requestDevicePropertySet("PriorProScan/FilterWheel1", value);
        });
    }
    for (const auto& kv : emissionFilterButtons) {
        std::string value = kv.first;
        auto button = kv.second;
        connect(button, &QPushButton::clicked, [this, value]{
            emit requestDevicePropertySet("PriorProScan/FilterWheel3", value);
        });
    }
}

void MainWindow::initTaskControlView()
{
    channelButtons = {
        {"BF", ui->button_channelBF},
        {"BFP", ui->button_channelBFP},
        {"CFP", ui->button_channelCFP},
        {"CFP/YFP FRET", ui->button_channelCFPYFPFRET},
        {"GFP", ui->button_channelGFP},
        {"YFP", ui->button_channelYFP},
        {"RFP", ui->button_channelRFP},
    };
    setButtonGroupEnabled(channelButtons, false);

    for (const auto& kv : channelButtons) {
        std::string value = kv.first;
        auto button = kv.second;
        connect(button, &QPushButton::clicked, [this, value]{
            emit requestSetChannel(value);
        });
    }

    connect(ui->button_live_stop, &QPushButton::clicked, [this] {
        if (ui->button_live_stop->text() == "Live") {
            emit requestSetTaskState("Live");
        } else if (ui->button_live_stop->text() == "Stop") {
            emit requestSetTaskState("Stop");
        }
    });
    connect(ui->button_capture, &QPushButton::clicked, [this] {
        emit requestSetTaskState("Capture");
    });
    connect(ui->button_start_pause, &QPushButton::clicked, [this] {
        if (ui->button_start_pause->text() == "Start") {
            emit requestSetTaskState("Run");
        } else if (ui->button_start_pause->text() == "Resume") {
            emit requestSetTaskState("Run");
        } else if (ui->button_start_pause->text() == "Pause") {
            emit requestSetTaskState("Pause");
        }
    });
    connect(ui->button_stop, &QPushButton::clicked, [this] {
        emit requestSetTaskState("Stop");
    });
}

void MainWindow::displayImage(Image *im)
{
    ui->glImageViewer->displayImage(im);
    std::vector<double> hist = im->getHistogram();

    //
    // Update QtCharts LineSeries
    //
    QVector<QPointF> seriesData;
    seriesData.reserve(256);
    for (int i = 0; i < 256; i++) {
        seriesData.append(QPointF(i * 256, hist[i]));
    }
    if (histSeries) {
        histSeries->replace(seriesData);
    }
}

void MainWindow::setDataManager(DataManager *dataManager)
{
    this->dataManager = dataManager;

    QObject::connect(dataManager, &DataManager::requestDisplayImage, this, &MainWindow::displayImage);

    ui->treeView->setModel(dataManager);
}


void MainWindow::setTaskControl(TaskControl *taskControl)
{
    this->taskControl = taskControl;

    updateTaskState(taskControl->getTaskState());

    QObject::connect(this, &MainWindow::requestSetTaskState, taskControl, &TaskControl::setTaskState);
    QObject::connect(this, &MainWindow::requestSetChannel, taskControl, &TaskControl::switchToChannel);
    QObject::connect(taskControl, &TaskControl::channelChanged, this, &MainWindow::updateChannel);
    QObject::connect(taskControl, &TaskControl::taskStateChanged, this, &MainWindow::updateTaskState);
}

void MainWindow::updateButtonGroup(std::unordered_map<std::string, QPushButton *> buttons, std::string value)
{
    for (const auto& kv : buttons) {
        if (kv.first == value) {
            kv.second->setDefault(true);
        } else {
            kv.second->setDefault(false);
        }
    }
}

void MainWindow::setButtonGroupEnabled(std::unordered_map<std::string, QPushButton *> buttons, bool enabled)
{
    for (const auto& kv : buttons) {
        auto button = kv.second;
        button->setEnabled(enabled);
    }
}

void MainWindow::updateDeviceProperty(std::string name, std::string value)
{
    if (name == "NikonTi/") {
        ui->label_statusNikon->setText(fmt::format("Nikon Ti: {}", value).c_str());
        if (value == "Connected") {
            setButtonGroupEnabled(filterBlock1Buttons, true);
            setButtonGroupEnabled(lightPathButtons, true);
            setButtonGroupEnabled(nosePieceButtons, true);
            setButtonGroupEnabled(diaShutterButtons, true);
            setButtonGroupEnabled(pfsStateButtons, true);

            std::vector<std::string> propertyList = {
                "NikonTi/FilterBlock1",
                "NikonTi/LightPath",
                "NikonTi/NosePiece",
                "NikonTi/DiaShutter",
                "NikonTi/PFSState",
                "NikonTi/ZDrivePosition",
                "NikonTi/PFSStatus",
                "NikonTi/PFSOffset",
            };

            for (const auto& name : propertyList) {
                emit requestDevicePropertyGet(name);
            }
        } else if ((value == "Disconnecting") || (value == "Disconnected")) {
            setButtonGroupEnabled(filterBlock1Buttons, false);
            setButtonGroupEnabled(lightPathButtons, false);
            setButtonGroupEnabled(nosePieceButtons, false);
            setButtonGroupEnabled(diaShutterButtons, false);
            setButtonGroupEnabled(pfsStateButtons, false);
            ui->label_ZDrivePosition->setText("---");
            ui->label_PFSStatus->setText("---");
            ui->label_PFSOffset->setText("---");
        }
    }

    if (name == "NikonTi/FilterBlock1") {
        updateButtonGroup(filterBlock1Buttons, value);
    } else if (name == "NikonTi/LightPath") {
        updateButtonGroup(lightPathButtons, value);
    } else if (name == "NikonTi/LightPath") {
        updateButtonGroup(lightPathButtons, value);
    } else if (name == "NikonTi/NosePiece") {
        updateButtonGroup(nosePieceButtons, value);
    } else if (name == "NikonTi/ZDrivePosition") {
        ui->label_ZDrivePosition->setText(value.c_str());
    } else if (name == "NikonTi/DiaShutter") {
        updateButtonGroup(diaShutterButtons, value);
    } else if (name == "NikonTi/PFSStatus") {
        ui->label_PFSStatus->setText(value.c_str());
    } else if (name == "NikonTi/PFSOffset") {
        ui->label_PFSOffset->setText(value.c_str());
    } else if (name == "NikonTi/PFSState") {
        updateButtonGroup(pfsStateButtons, value);
    }

    if (name == "Hamamatsu/") {
        ui->label_statusHamamatsu->setText(fmt::format("Hamamatsu: {}", value).c_str());
    }

    if (name == "PriorProScan/") {
        ui->label_statusPrior->setText(fmt::format("Prior Proscan: {}", value).c_str());
        if (value == "Connected") {
            setButtonGroupEnabled(excitationFilterButtons, true);
            setButtonGroupEnabled(emissionFilterButtons, true);

            std::vector<std::string> propertyList = {
                "PriorProScan/FilterWheel1",
                "PriorProScan/FilterWheel3",
            };
            for (const auto& name : propertyList) {
                emit requestDevicePropertyGet(name);
            }

        } else if ((value == "Disconnecting") || (value == "Disconnected")) {
            setButtonGroupEnabled(excitationFilterButtons, false);
            setButtonGroupEnabled(emissionFilterButtons, false);
            ui->label_positionX->setText("---");
            ui->label_positionY->setText("---");
        }
    }

    if (name == "PriorProScan/FilterWheel1") {
        updateButtonGroup(excitationFilterButtons, value);
    } else if (name == "PriorProScan/FilterWheel3") {
        updateButtonGroup(emissionFilterButtons, value);
    } else if (name == "PriorProScan/XYPosition") {
        auto n = value.find(',');
        if ((n != std::string::npos) && (n + 1 < value.size())) {
            std::string x = value.substr(0, n);
            std::string y = value.substr(n + 1);
            ui->label_positionX->setText(x.c_str());
            ui->label_positionY->setText(y.c_str());
        }
    }
}

void MainWindow::updateTaskState(std::string state)
{
    ui->label_statusTask->setText(state.c_str());

    if (state == "Not Connected") {
        ui->button_live_stop->setText("Live");
        ui->button_start_pause->setText("Start");
        ui->button_stop->setText("Stop");

        ui->button_live_stop->setEnabled(false);
        ui->button_capture->setEnabled(false);
        ui->button_start_pause->setEnabled(false);
        ui->button_stop->setEnabled(false);
    } else if (state == "Ready") {
        ui->button_live_stop->setText("Live");
        ui->button_start_pause->setText("Start");
        ui->button_stop->setText("Stop");

        ui->button_live_stop->setEnabled(true);
        ui->button_capture->setEnabled(true);
        ui->button_start_pause->setEnabled(true);
        ui->button_stop->setEnabled(true);
    } else if (state == "Live") {
        ui->button_live_stop->setText("Stop");
        ui->button_start_pause->setText("Start");
        ui->button_stop->setText("Stop");

        ui->button_live_stop->setEnabled(true);
        ui->button_capture->setEnabled(false);
        ui->button_start_pause->setEnabled(false);
        ui->button_stop->setEnabled(false);
    } else if (state == "Capture") {
        ui->button_live_stop->setText("Live");
        ui->button_start_pause->setText("Start");
        ui->button_stop->setText("Stop");

        ui->button_live_stop->setEnabled(false);
        ui->button_capture->setEnabled(false);
        ui->button_start_pause->setEnabled(false);
        ui->button_stop->setEnabled(true);
    } else if (state == "Run") {
        ui->button_live_stop->setText("Live");
        ui->button_start_pause->setText("Pause");
        ui->button_stop->setText("Stop");

        ui->button_live_stop->setEnabled(false);
        ui->button_capture->setEnabled(false);
        ui->button_start_pause->setEnabled(true);
        ui->button_stop->setEnabled(true);
    } else if (state == "Pause") {
        ui->button_live_stop->setText("Live");
        ui->button_start_pause->setText("Resume");
        ui->button_stop->setText("Stop");

        ui->button_live_stop->setEnabled(false);
        ui->button_capture->setEnabled(false);
        ui->button_start_pause->setEnabled(true);
        ui->button_stop->setEnabled(true);
    }
}

void MainWindow::updateChannel(std::string channel)
{
    setButtonGroupEnabled(channelButtons, true);
    updateButtonGroup(channelButtons, channel);
}
