#ifndef EXPERIMENTCONTROL_H
#define EXPERIMENTCONTROL_H

#include <future>

#include "experimentdb.h"
#include "image/imagemanager.h"
#include "device/devicehub.h"
#include "eventstream.h"
#include "sample/samplemanager.h"
#include "task/channelcontrol.h"
#include "task/live_view_task.h"
#include "task/multi_channel_task.h"

class ExperimentControl : public EventSender {
public:
    ExperimentControl(DeviceHub *dev);
    ~ExperimentControl();

    void SubscribeEvents(EventStream *channel) override;

    void SetBaseDir(std::filesystem::path base_dir);
    std::filesystem::path BaseDir();
    void OpenExperimentDir(std::filesystem::path exp_dir);
    void OpenExperiment(std::string name);
    void CloseExperiment();
    std::filesystem::path ExperimentDir();
    ExperimentDB *DB();

    DeviceHub *Devices();
    SampleManager *Samples();
    ChannelControl *Channels();
    ImageManager *Images();

    void StartLiveView();
    void StopLiveView();
    bool IsLiveRunning();

    void AcquireMultiChannel(std::string ndimage_name,
                             std::vector<Channel> channels, int i_z, int i_t,
                             Site *site = nullptr,
                             nlohmann::ordered_json metadata = nullptr);
    void WaitMultiChannelTask();

private:
    DeviceHub *dev;
    SampleManager *sample_manager;
    ChannelControl *channel_control;
    ImageManager *image_manager;

    std::filesystem::path base_dir;
    std::filesystem::path exp_dir;
    ExperimentDB *db =  nullptr;

    EventStream dev_event_stream;
    std::future<void> handle_dev_event_future;
    void handleDeviceEvents();

    LiveViewTask *live_view_task;
    MultiChannelTask *multichannel_task;

    std::mutex task_mutex;
    std::atomic<bool> is_busy = false;
    std::future<void> current_task_future;
    void runLiveView();
    void runMultiChannelTask(std::string ndimage_name,
                             std::vector<Channel> channels, int i_z, int i_t, Site *site,
                             nlohmann::ordered_json metadata);
};

#endif
