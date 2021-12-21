#include "imagingcontrol.h"

#include <set>

#include "logging.h"

ImagingControl::ImagingControl(DeviceHub *hub, Hamamatsu::DCam *dcam)
    : channel_control(hub), sample_manager(hub)
{
    this->hub = hub;
    this->dcam = dcam;

    live_view_task = new LiveViewTask(hub, dcam, &data_manager);
    multichannel_task =
        new MultiChannelTask(hub, dcam, &channel_control, &data_manager);

    hub->SubscribeEvents(&dev_event_stream);
    handle_dev_event_future = std::async(
        std::launch::async, &ImagingControl::handleDeviceEvents, this);
}

ImagingControl::~ImagingControl()
{
    dev_event_stream.Close();
    handle_dev_event_future.get();
}

void ImagingControl::SubscribeEvents(EventStream *channel)
{
    EventSender::SubscribeEvents(channel);

    channel_control.SubscribeEvents(channel);
    sample_manager.SubscribeEvents(channel);
    data_manager.SubscribeEvents(channel);

    live_view_task->SubscribeEvents(channel);
    multichannel_task->SubscribeEvents(channel);
}

void ImagingControl::runLiveView()
{
    if (is_busy) {
        throw std::runtime_error(
            "Cannot start live view: task control is in busy state");
    }

    std::lock_guard<std::mutex> lk(task_mutex);

    if (is_busy) {
        throw std::runtime_error(
            "Cannot start live view: task control is in busy state");
    }

    channel_control.OpenCurrentShutter();

    SendEvent({
        .type = EventType::TaskStateChanged,
        .value = "Live",
    });
    LOG_INFO("Live view started");

    is_busy = true;
    try {
        live_view_task->Run();
    } catch (std::exception &e) {
        is_busy = false;
        std::string message = fmt::format("Error in live view: {}", e.what());
        LOG_ERROR(message);
        SendEvent({
            .type = EventType::TaskStateChanged,
            .value = "Error",
        });
        SendEvent({
            .type = EventType::TaskMessage,
            .value = message,
        });
        channel_control.CloseCurrentShutter();
        throw std::runtime_error(message);
    }

    is_busy = false;
    SendEvent({
        .type = EventType::TaskStateChanged,
        .value = "Ready",
    });
    LOG_INFO("Live view stopped");
    channel_control.CloseCurrentShutter();
}

void ImagingControl::StartLiveView()
{
    //
    // Check for obvious errors and throw exception immediately
    //
    if (is_busy) {
        throw std::runtime_error(
            "Cannot start live view: task control is in busy state");
    }

    //
    // Clear the future and log errors we missed
    //
    if (current_task_future.valid()) {
        try {
            current_task_future.get();
        } catch (std::exception &e) {
            LOG_WARN("Ignore error in previous task: {}", e.what());
        }
    }

    //
    // Start async task
    //
    current_task_future =
        std::async(std::launch::async, &ImagingControl::runLiveView, this);
}

void ImagingControl::StopLiveView()
{
    try {
        live_view_task->Stop();
    } catch (std::exception &e) {
        // TODO: check whether the future is ready

        std::string message =
            fmt::format("Failed to stop live view: {}", e.what());
        SendEvent({
            .type = EventType::TaskMessage,
            .value = message,
        });
        LOG_ERROR(message);
        return;
    }

    // Get the exception
    current_task_future.get();
}

void ImagingControl::runMultiChannelTask(std::string ndimage_name,
                                         std::vector<Channel> channels, int i_z,
                                         int i_t,
                                         nlohmann::ordered_json metadata)
{
    if (is_busy) {
        throw std::runtime_error(
            "Cannot start task: task control is in busy state");
    }

    std::lock_guard<std::mutex> lk(task_mutex);

    if (is_busy) {
        throw std::runtime_error(
            "Cannot start task: task control is in busy state");
    }

    is_busy = true;
    try {
        Status status = multichannel_task->Acquire(ndimage_name, channels, i_z,
                                                   i_t, metadata);
        if (!status.ok()) {
            throw std::runtime_error(status.ToString());
        }
    } catch (std::exception &e) {
        is_busy = false;
        std::string message = fmt::format("Error in task: {}", e.what());
        LOG_ERROR(message);
        SendEvent({
            .type = EventType::TaskStateChanged,
            .value = "Error",
        });
        SendEvent({
            .type = EventType::TaskMessage,
            .value = message,
        });
        throw std::runtime_error(message);
    }

    is_busy = false;
    SendEvent({
        .type = EventType::TaskStateChanged,
        .value = "Ready",
    });
    LOG_INFO("Task completed");
}

void ImagingControl::AcquireMultiChannel(std::string ndimage_name,
                                         std::vector<Channel> channels, int i_z,
                                         int i_t,
                                         nlohmann::ordered_json metadata)
{
    //
    // Check for obvious errors and throw exception immediately
    //
    if (is_busy) {
        throw std::runtime_error(
            "Cannot start task: task control is in busy state");
    }

    //
    // Clear the future and log errors we missed
    //
    if (current_task_future.valid()) {
        try {
            current_task_future.get();
        } catch (std::exception &e) {
            LOG_WARN("Ignore error in previous task: {}", e.what());
        }
    }

    //
    // Start async task
    //
    current_task_future =
        std::async(std::launch::async, &ImagingControl::runMultiChannelTask,
                   this, ndimage_name, channels, i_z, i_t, metadata);
}

void ImagingControl::WaitMultiChannelTask()
{
    // Wait and get exception
    current_task_future.get();
}


void ImagingControl::handleDeviceEvents()
{
    const std::set<std::string> dev_required = {"NikonTi", "Hamamatsu",
                                                "PriorProScan"};

    std::set<std::string> dev_connected;

    Event e;
    while (dev_event_stream.Receive(&e)) {
        if ((e.type == EventType::DeviceConnectionStateChanged) &&
            (e.value == DeviceConnectionState::Connected))
        {
            dev_connected.insert(e.device);

            bool all_connected = true;
            for (const auto &dev_name : dev_required) {
                if (!dev_connected.contains(dev_name)) {
                    all_connected = false;
                    break;
                }
            }
            if (all_connected) {
                SendEvent({
                    .type = EventType::TaskStateChanged,
                    .value = "Ready",
                });
            }
        }
    }
}
