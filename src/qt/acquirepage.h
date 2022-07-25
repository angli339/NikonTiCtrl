#ifndef ACQUIREPAGE_H
#define ACQUIREPAGE_H

#include <future>

#include <QTableView>
#include <QTreeView>
#include <QWidget>

#include "experimentcontrol.h"
#include "qt/imagemanager_view.h"
#include "qt/devicecontrol_view.h"
#include "qt/acquisitioncontrol_view.h"
#include "qt/ndimage_view.h"
#include "qt/samplemanager_view.h"

class AcquirePage : public QWidget {
    Q_OBJECT
public:
    explicit AcquirePage(QWidget *parent = nullptr);
    void setExperimentControlModel(ExperimentControlModel *model);
    void setDeviceControlModel(DeviceControlModel *model);

    void selectExperimentDir();

    void runLiveViewDisplay();
    void displayNDImage(QString name);
    void displayNDImage(QString name, int i_z, int i_t);

    void handleExperimentClose();
    void handleNDImageSelection(QString name);
    void handleNDImageUpdate(QString name);
    void handleZSliderValueChange(int i_z);
    void handleTSliderValueChange(int i_t);

public slots:
    void startLiveViewDisplay();

public:
    ExperimentControlModel *expControlModel;
    DeviceControlModel *devControlModel;

    std::future<void> liveViewDisplayFuture;

    QPushButton *experimentDirButton;
    ImageManagerView *dataManagerView;
    SampleManagerView *sampleManagerView;
    DeviceControlView *deviceControlView;
    AcquisitionControlView *imagingControlView;
    NDImageView *ndImageView;

    std::mutex im_mutex;
    QString ndImageSelected;
    QString ndImageLatest;
    int current_i_z;
    int current_i_t;
};

#endif // ACQUIREPAGE_H
