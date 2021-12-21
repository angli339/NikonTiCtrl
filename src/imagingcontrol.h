#ifndef IMAGINGCONTROL_H
#define IMAGINGCONTROL_H

#include <future>

#include "data/datamanager.h"
#include "device/devicehub.h"
#include "device/hamamatsu/hamamatsu_dcam.h"
#include "eventstream.h"
#include "sample/samplemanager.h"
#include "task/channelcontrol.h"
#include "task/live_view_task.h"
#include "task/multi_channel_task.h"

class ImagingControl : public EventSender {
public:
    ImagingControl(DeviceHub *hub, Hamamatsu::DCam *dcam);
    ~ImagingControl();

    void SubscribeEvents(EventStream *channel) override;

    SampleManager *SampleManager()
    {
        return &sample_manager;
    }
    ChannelControl *ChannelControl()
    {
        return &channel_control;
    }
    DataManager *DataManager()
    {
        return &data_manager;
    }

    void StartLiveView();
    void StopLiveView();
    bool IsLiveRunning()
    {
        return live_view_task->IsRunning();
    }

    void AcquireMultiChannel(std::string ndimage_name,
                             std::vector<Channel> channels, int i_z, int i_t,
                             nlohmann::ordered_json metadata = nullptr);
    void WaitMultiChannelTask();

private:
    DeviceHub *hub;
    Hamamatsu::DCam *dcam;
    EventStream dev_event_stream;
    std::future<void> handle_dev_event_future;
    void handleDeviceEvents();

    ::SampleManager sample_manager;
    ::ChannelControl channel_control;
    ::DataManager data_manager;

    LiveViewTask *live_view_task;
    MultiChannelTask *multichannel_task;

    std::mutex task_mutex;
    std::atomic<bool> is_busy = false;
    std::future<void> current_task_future;
    void runLiveView();
    void runMultiChannelTask(std::string ndimage_name,
                             std::vector<Channel> channels, int i_z, int i_t,
                             nlohmann::ordered_json metadata);
};

#endif
