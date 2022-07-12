#ifndef IMAGINGCONTROL_MODEL_H
#define IMAGINGCONTROL_MODEL_H

#include <future>

#include <QObject>

#include "eventstream.h"
#include "experimentcontrol.h"
#include "logging.h"

#include "channelcontrol_model.h"
#include "datamanager_model.h"
#include "samplemanager_model.h"

class ExperimentControlModel : public QObject {
    Q_OBJECT
public:
    explicit ExperimentControlModel(ExperimentControl *imagingControl,
                                 QObject *parent = nullptr);
    ~ExperimentControlModel();

    ChannelControlModel *ChannelControlModel();
    ImageManagerModel *DataManagerModel();
    SampleManagerModel *SampleManagerModel();

    void SetExperimentDir(QString exp_dir);
    QString ExperimentDir();

    void SetSelectedSite(Site *site)
    {
        if (site != nullptr) {
            LOG_DEBUG("site {} selected", site->FullID());
        } else {
            LOG_DEBUG("site selection cleared");
        }

        selected_array = nullptr;
        selected_sample = nullptr;
        selected_site = site;
    }

    void StartLiveView();
    void StopLiveView();

    void StartMultiChannelTask();

signals:
    void stateChanged(QString state);
    void channelChanged(QString channel_name);
    void experimentPathChanged(QString path);
    void messageReceived(QString message);
    void liveViewStarted();

private:
    ExperimentControl *experimentControl;

    ::ChannelControlModel *channelControlModel;
    ::SampleManagerModel *sampleManagerModel;
    ::ImageManagerModel *imageManagerModel;

    SampleArray *selected_array = nullptr;
    Sample *selected_sample = nullptr;
    Site *selected_site = nullptr;

    EventStream eventStream;
    std::future<void> handleEventFuture;
    void handleEvents();
};

#endif
