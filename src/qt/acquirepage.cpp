#include "qt/acquirepage.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QVBoxLayout>

#include "logging.h"

AcquirePage::AcquirePage(QWidget *parent) : QWidget(parent)
{
    //
    // Left Sidebar
    //
    QLabel *experimentTitle = new QLabel("Experiment");
    experimentTitle->setFixedHeight(20);
    experimentDirButton = new QPushButton("Select directory...");
    experimentDirButton->setFixedHeight(25);
    experimentDirButton->setStyleSheet("text-align:left");

    sampleManagerView = new SampleManagerView;
    sampleManagerView->setMinimumHeight(100);

    dataManagerView = new ImageManagerView;
    dataManagerView->setMinimumHeight(100);

    QWidget *leftTopGroup = new QWidget;
    leftTopGroup->setLayout(new QVBoxLayout);
    leftTopGroup->layout()->setAlignment(Qt::AlignTop);
    leftTopGroup->layout()->setSpacing(5);
    leftTopGroup->layout()->setContentsMargins(0, 0, 10, 10);
    leftTopGroup->layout()->addWidget(experimentTitle);
    leftTopGroup->layout()->addWidget(experimentDirButton);
    leftTopGroup->layout()->addWidget(sampleManagerView);

    QWidget *leftBottomGroup = new QWidget;
    leftBottomGroup->setLayout(new QVBoxLayout);
    leftBottomGroup->layout()->setAlignment(Qt::AlignTop);
    leftBottomGroup->layout()->setSpacing(5);
    leftBottomGroup->layout()->setContentsMargins(0, 0, 10, 0);
    leftBottomGroup->layout()->addWidget(dataManagerView);

    QSplitter *leftSidebar = new QSplitter(Qt::Vertical);
    leftSidebar->setChildrenCollapsible(false);
    leftSidebar->setHandleWidth(leftSidebar->handleWidth() / 4);
    leftSidebar->addWidget(leftTopGroup);
    leftSidebar->addWidget(leftBottomGroup);
    leftSidebar->setMinimumWidth(300);
    leftSidebar->setStretchFactor(0, 1);
    leftSidebar->setStretchFactor(1, 2);

    //
    // Middle
    //
    QVBoxLayout *middleLayout = new QVBoxLayout;
    middleLayout->setAlignment(Qt::AlignTop);

    imagingControlView = new AcquisitionControlView;
    ndImageView = new NDImageView;
    ndImageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    middleLayout->addWidget(imagingControlView);
    middleLayout->addWidget(ndImageView);

    //
    // Right
    //
    deviceControlView = new DeviceControlView;
    deviceControlView->setMaximumWidth(235);
    deviceControlView->setMinimumWidth(235);

    //
    // Main Container = Middle & Right
    //
    QWidget *mainContainer = new QWidget;
    QHBoxLayout *mainGroupLayout = new QHBoxLayout(mainContainer);
    mainGroupLayout->setSpacing(5);
    mainGroupLayout->setContentsMargins(10, 0, 0, 0);
    mainGroupLayout->addLayout(middleLayout);
    mainGroupLayout->addWidget(deviceControlView);

    //
    // Left Sidebar | Main Container
    //
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->setHandleWidth(mainSplitter->handleWidth() / 4);
    mainSplitter->addWidget(leftSidebar);
    mainSplitter->addWidget(mainContainer);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 3);

    //
    // Add Left/Right Splitter
    //
    this->setLayout(new QHBoxLayout);
    this->layout()->setSpacing(0);
    this->layout()->setContentsMargins(5, 5, 5, 5);
    this->layout()->addWidget(mainSplitter);

    connect(imagingControlView, &AcquisitionControlView::liveViewStarted, this,
            &AcquirePage::startLiveViewDisplay);
    connect(dataManagerView, &ImageManagerView::ndImageSelected, this,
            &AcquirePage::handleNDImageSelection);
}

void AcquirePage::setExperimentControlModel(ExperimentControlModel *model)
{
    sampleManagerView->setModel(model->SampleManagerModel());
    dataManagerView->setModel(model->ImageManagerModel());
    imagingControlView->setModel(model);
    expControlModel = model;

    if (!model->ExperimentDir().isEmpty()) {
        this->experimentDirButton->setText(model->ExperimentDir());
    }
    connect(model, &ExperimentControlModel::experimentOpened,
            this->experimentDirButton, &QPushButton::setText);
    connect(experimentDirButton, &QPushButton::clicked, this,
            &AcquirePage::selectExperimentDir);

    connect(model->ImageManagerModel(), &ImageManagerModel::ndImageCreated,
            this, &AcquirePage::handleNDImageUpdate);
    connect(model->ImageManagerModel(), &ImageManagerModel::ndImageChanged,
            this, &AcquirePage::handleNDImageUpdate);
    connect(model, &ExperimentControlModel::experimentClosed, this,
            &AcquirePage::handleExperimentClose);

    connect(ndImageView->tSlider, &QSlider::valueChanged, this,
            &AcquirePage::handleTSliderValueChange);
    connect(ndImageView->zSlider, &QSlider::valueChanged, this,
            &AcquirePage::handleZSliderValueChange);
}

void AcquirePage::selectExperimentDir()
{

    QString dir =
        QFileDialog::getExistingDirectory(this, "Select Experiment Directory",
                                          expControlModel->GetUserDataRoot());
    if (dir.isEmpty()) {
        return;
    }
    try {
        expControlModel->SetExperimentDir(dir);
    } catch (std::exception &e) {
        QMessageBox::warning(this, QString("Error"), QString(e.what()));
    }
}

void AcquirePage::setDeviceControlModel(DeviceControlModel *model)
{
    deviceControlView->setModel(model);
    devControlModel = model;
}

void AcquirePage::runLiveViewDisplay()
{
    LOG_DEBUG("live view display started");
    for (;;) {
        ImageData frame =
            expControlModel->ImageManagerModel()->GetNextLiveViewFrame();
        if (frame.empty()) {
            LOG_DEBUG("live view display stopped");
            return;
        }
        ndImageView->setFrameData(0, frame);
    }
}

void AcquirePage::startLiveViewDisplay()
{
    LOG_DEBUG("AcquirePage::StartLiveViewDisplay");
    ndImageView->setNChannels(1);
    ndImageView->setChannelName(0, "BF");
    if (liveViewDisplayFuture.valid()) {
        try {
            liveViewDisplayFuture.get();
        } catch (std::exception &e) {
            LOG_WARN("Ignored previous error: {}", e.what());
        }
    }

    liveViewDisplayFuture =
        std::async(std::launch::async, &AcquirePage::runLiveViewDisplay, this);
}

void AcquirePage::handleExperimentClose()
{
    std::unique_lock<std::mutex> lk(im_mutex);
    ndImageSelected = "";
    ndImageLatest = "";
    lk.unlock();

    displayNDImage("");
    experimentDirButton->setText("Select directory...");
}

void AcquirePage::handleNDImageSelection(QString name)
{
    std::unique_lock<std::mutex> lk(im_mutex);
    ndImageSelected = name;
    if (name.isEmpty()) {
        if (!ndImageLatest.isEmpty()) {
            displayNDImage(ndImageLatest);
            return;
        }
    } else {
        displayNDImage(ndImageSelected);
        return;
    }
}

void AcquirePage::handleNDImageUpdate(QString name)
{
    std::unique_lock<std::mutex> lk(im_mutex);
    ndImageLatest = name;
    if (ndImageSelected.isEmpty()) {
        displayNDImage(name);
        return;
    }
}

void AcquirePage::handleZSliderValueChange(int i_z)
{
    if (current_i_z == i_z) {
        return;
    }

    std::unique_lock<std::mutex> lk(im_mutex);
    current_i_z = i_z;
    if (ndImageSelected.isEmpty()) {
        displayNDImage(ndImageLatest, current_i_z, current_i_t);
    } else {
        displayNDImage(ndImageSelected, current_i_z, current_i_t);
    }
}

void AcquirePage::handleTSliderValueChange(int i_t)
{
    if (current_i_t == i_t) {
        return;
    }

    std::unique_lock<std::mutex> lk(im_mutex);
    current_i_t = i_t;
    if (ndImageSelected.isEmpty()) {
        displayNDImage(ndImageLatest, current_i_z, current_i_t);
    } else {
        displayNDImage(ndImageSelected, current_i_z, current_i_t);
    }
}

void AcquirePage::displayNDImage(QString name)
{
    if (name.isEmpty()) {
        displayNDImage("", 0, 0);
        return;
    }

    NDImage *im = expControlModel->ImageManagerModel()->GetNDImage(name);
    ndImageView->setNChannels(im->NChannels());

    if (im->NDimT() == 0) {
        return;
    }

    // find latested image
    int i_t = im->NDimT() - 1;
    int i_z = 0;
    if (im->NDimZ() > 0) {
        i_z = im->NDimZ() - 1;
    }
    current_i_z = i_z;
    current_i_t = i_t;

    displayNDImage(name, current_i_z, current_i_t);
}

void AcquirePage::displayNDImage(QString name, int i_z, int i_t)
{
    if (name.isEmpty()) {
        ndImageView->clear();
        ndImageView->setNChannels(1);

        // Reset slider
        ndImageView->setNDimZ(0);
        ndImageView->setNDimT(0);
        ndImageView->setIndexZ(0);
        ndImageView->setIndexT(0);

        return;
    }

    NDImage *im = expControlModel->ImageManagerModel()->GetNDImage(name);
    ndImageView->setNChannels(im->NChannels());

    if (im->NDimT() == 0) {
        return;
    }

    // Update slider
    ndImageView->setNDimZ(im->NDimZ());
    ndImageView->setNDimT(im->NDimT());
    ndImageView->setIndexZ(i_z);
    ndImageView->setIndexT(i_t);

    // Display image
    for (int i_ch = 0; i_ch < im->NChannels(); i_ch++) {
        if (im->HasData(i_ch, i_z, i_t)) {
            ImageData d = im->GetData(i_ch, i_z, i_t);
            ndImageView->setFrameData(i_ch, d);
            ndImageView->setChannelName(i_ch, im->ChannelName(i_ch).c_str());
        }
    }
}
