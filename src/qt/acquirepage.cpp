#include "qt/acquirepage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

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

    sampleManagerView = new SampleManagerView;
    dataManagerView = new DataManagerView;
    sampleManagerView->setMaximumWidth(280);
    sampleManagerView->setMinimumWidth(280);
    dataManagerView->setMaximumWidth(280);
    dataManagerView->setMinimumWidth(280);

    leftLayout->addWidget(sampleManagerView);
    leftLayout->addWidget(dataManagerView);

    //
    // Middle
    //
    QVBoxLayout *middleLayout = new QVBoxLayout;
    middleLayout->setAlignment(Qt::AlignTop);

    imagingControlView = new ImagingControlView;
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

    connect(imagingControlView, &ImagingControlView::liveViewStarted, this,
            &AcquirePage::startLiveViewDisplay);
    connect(dataManagerView, &DataManagerView::ndImageSelected, this,
            &AcquirePage::handleNDImageSelection);
}

void AcquirePage::setImagingControlModel(ImagingControlModel *model)
{
    sampleManagerView->setModel(model->SampleManagerModel());
    dataManagerView->setModel(model->DataManagerModel());
    imagingControlView->setModel(model);
    imagingControlModel = model;

    connect(model->DataManagerModel(), &DataManagerModel::ndImageCreated, this,
            &AcquirePage::handleNDImageUpdate);
    connect(model->DataManagerModel(), &DataManagerModel::ndImageChanged, this,
            &AcquirePage::handleNDImageUpdate);
}

void AcquirePage::setDeviceControlModel(DeviceControlModel *model)
{
    deviceControlView->setModel(model);
    deviceControlModel = model;
}

void AcquirePage::runLiveViewDisplay()
{
    LOG_DEBUG("live view display started");
    for (;;) {
        ImageData frame =
            imagingControlModel->DataManagerModel()->GetNextLiveViewFrame();
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
    ndImageView->setNumChannels(1);
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

void AcquirePage::displayNDImage(QString name)
{
    NDImage *im = imagingControlModel->DataManagerModel()->GetNDImage(name);
    ndImageView->setNumChannels(im->NChannels());

    if (im->NDimT() == 0) {
        return;
    }

    // Display latest images
    int i_t = im->NDimT() - 1;
    int i_z = 0;
    if (im->NDimZ() > 0) {
        i_z = im->NDimZ() - 1;
    }

    for (int i_ch = 0; i_ch < im->NChannels(); i_ch++) {
        if (im->HasData(i_ch, i_z, i_t)) {
            ImageData d = im->GetData(i_ch, i_z, i_t);
            ndImageView->setFrameData(i_ch, d);
            ndImageView->setChannelName(i_ch, im->ChannelName(i_ch).c_str());
        }
    }
}
