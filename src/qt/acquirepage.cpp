#include "qt/acquirepage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>

#include "logging.h"

AcquirePage::AcquirePage(QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(5);
    layout->setContentsMargins(0, 0, 0, 0);

    //
    // Left
    //
    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->setAlignment(Qt::AlignTop);

    QLabel *experimentTitle = new QLabel("Experiment");
    experimentTitle->setFixedHeight(20);
    experimentDirButton = new QPushButton("Select directory...");
    experimentDirButton->setFixedHeight(25);
    experimentDirButton->setStyleSheet("text-align:left");

    sampleManagerView = new SampleManagerView;
    dataManagerView = new ImageManagerView;
    sampleManagerView->setMaximumWidth(280);
    sampleManagerView->setMinimumWidth(280);
    dataManagerView->setMaximumWidth(280);
    dataManagerView->setMinimumWidth(280);

    leftLayout->addWidget(experimentTitle);
    leftLayout->addWidget(experimentDirButton);
    leftLayout->addWidget(sampleManagerView);
    leftLayout->addWidget(dataManagerView);

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

    layout->addLayout(leftLayout);
    layout->addLayout(middleLayout);
    layout->addWidget(deviceControlView);

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
    connect(model, &ExperimentControlModel::experimentPathChanged,
            this->experimentDirButton, &QPushButton::setText);
    connect(experimentDirButton, &QPushButton::clicked, this, &AcquirePage::selectExperimentDir);

    connect(model->ImageManagerModel(), &ImageManagerModel::ndImageCreated, this,
            &AcquirePage::handleNDImageUpdate);
    connect(model->ImageManagerModel(), &ImageManagerModel::ndImageChanged, this,
            &AcquirePage::handleNDImageUpdate);
    
    connect(ndImageView->tSlider, &QSlider::valueChanged, this, &AcquirePage::handleTSliderValueChange);
    connect(ndImageView->zSlider, &QSlider::valueChanged, this, &AcquirePage::handleZSliderValueChange);
}

void AcquirePage::selectExperimentDir()
{

    QString dir = QFileDialog::getExistingDirectory(this,
        "Select Experiment Directory",
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
    NDImage *im = expControlModel->ImageManagerModel()->GetNDImage(name);
    ndImageView->setNChannels(im->NChannels());

    if (im->NDimT() == 0) {
        return;
    }

    ndImageView->setNDimZ(im->NDimZ());
    ndImageView->setNDimT(im->NDimT());

    ndImageView->setIndexZ(i_z);
    ndImageView->setIndexT(i_t);

    for (int i_ch = 0; i_ch < im->NChannels(); i_ch++) {
        if (im->HasData(i_ch, i_z, i_t)) {
            ImageData d = im->GetData(i_ch, i_z, i_t);
            ndImageView->setFrameData(i_ch, d);
            ndImageView->setChannelName(i_ch, im->ChannelName(i_ch).c_str());
        }
    }
}
