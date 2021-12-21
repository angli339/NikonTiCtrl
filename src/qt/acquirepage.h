#ifndef ACQUIREPAGE_H
#define ACQUIREPAGE_H

#include <future>

#include <QTableView>
#include <QTreeView>
#include <QWidget>

#include "imagingcontrol.h"
#include "qt/datamanager_view.h"
#include "qt/devicecontrol_view.h"
#include "qt/imagingcontrol_view.h"
#include "qt/ndimageview.h"
#include "qt/samplemanager_view.h"

class AcquirePage : public QWidget {
    Q_OBJECT
public:
    explicit AcquirePage(QWidget *parent = nullptr);
    void setImagingControlModel(ImagingControlModel *model);
    void setDeviceControlModel(DeviceControlModel *model);

    void runLiveViewDisplay();
    void displayNDImage(QString name);
    void handleNDImageSelection(QString name);
    void handleNDImageUpdate(QString name);

public slots:
    void startLiveViewDisplay();

public:
    ImagingControlModel *imagingControlModel;
    DeviceControlModel *deviceControlModel;

    std::future<void> liveViewDisplayFuture;

    DataManagerView *dataManagerView;
    SampleManagerView *sampleManagerView;
    DeviceControlView *deviceControlView;
    ImagingControlView *imagingControlView;
    NDImageView *ndImageView;

    std::mutex im_mutex;
    QString ndImageSelected;
    QString ndImageLatest;
};

#endif // ACQUIREPAGE_H
