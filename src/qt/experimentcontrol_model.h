#ifndef EXPERIMENTCONTROL_MODEL_H
#define EXPERIMENTCONTROL_MODEL_H

#include <future>

#include <QObject>

#include "eventstream.h"
#include "experimentcontrol.h"
#include "logging.h"

#include "channelcontrol_model.h"
#include "imagemanager_model.h"
#include "samplemanager_model.h"

class ExperimentControlModel : public QObject {
    Q_OBJECT
public:
    explicit ExperimentControlModel(ExperimentControl *exp,
                                 QObject *parent = nullptr);
    ~ExperimentControlModel();

    ChannelControlModel *ChannelControlModel();
    ImageManagerModel *ImageManagerModel();
    SampleManagerModel *SampleManagerModel();

    QString GetUserDataRoot();
    void SetExperimentDir(QString exp_dir);
    QString ExperimentDir();

    void SetSelectedSite(Site *site);

    void StartLiveView();
    void StopLiveView();

    void StartMultiChannelTask();

signals:
    void stateChanged(QString state);
    void channelChanged(QString channel_name);
    void experimentOpened(QString path);
    void experimentClosed();
    void messageReceived(QString message);
    void liveViewStarted();

private:
    ExperimentControl *exp;

    ::ChannelControlModel *channelControlModel;
    ::SampleManagerModel *sampleManagerModel;
    ::ImageManagerModel *imageManagerModel;

    Plate *selected_plate = nullptr;
    Well *selected_well = nullptr;
    Site *selected_site = nullptr;

    EventStream eventStream;
    std::future<void> handleEventFuture;
    void handleEvents();
};

#endif
