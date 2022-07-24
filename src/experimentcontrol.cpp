#include "experimentcontrol.h"

#include <set>

#include "logging.h"

ExperimentControl::ExperimentControl(DeviceHub *dev)
{
    this->dev = dev;
    this->sample_manager = new SampleManager(this);
    this->image_manager = new ImageManager(this);
    this->analysis_manager = new AnalysisManager(this);

    this->channel_control = new ChannelControl(dev);
    this->live_view_task = new LiveViewTask(this);
    this->multichannel_task = new MultiChannelTask(this);

    dev->SubscribeEvents(&dev_event_stream);
    handle_dev_event_future = std::async(
        std::launch::async, &ExperimentControl::handleDeviceEvents, this);
}

ExperimentControl::~ExperimentControl()
{
    dev_event_stream.Close();
    handle_dev_event_future.get();
    
    if (db) {
        delete db;
    }

    delete sample_manager;
    delete image_manager;

    delete channel_control;
    delete live_view_task;
    delete multichannel_task;
}

void ExperimentControl::SubscribeEvents(EventStream *channel)
{
    EventSender::SubscribeEvents(channel);

    sample_manager->SubscribeEvents(channel);
    image_manager->SubscribeEvents(channel);

    channel_control->SubscribeEvents(channel);
    live_view_task->SubscribeEvents(channel);
    multichannel_task->SubscribeEvents(channel);
}

std::filesystem::path ExperimentControl::BaseDir()
{
    if (!base_dir.empty()) {
        return base_dir;
    }

    if (!std::filesystem::exists(config.user.data_root)) {
        if (!std::filesystem::create_directories(config.user.data_root)) {
            throw std::runtime_error(
                fmt::format("failed to create base dir {}", config.user.data_root.string()));
        }
    }
    base_dir = config.user.data_root;
    return base_dir;
}

void ExperimentControl::SetBaseDir(std::filesystem::path base_dir)
{
    if (base_dir.empty()) {
        throw std::invalid_argument("empty base dir");
    }

    if (!std::filesystem::exists(base_dir)) {
        if (!std::filesystem::create_directories(base_dir)) {
            throw std::runtime_error(
                fmt::format("failed to create base dir {}", base_dir.string()));
        }
    }
    this->base_dir = base_dir;
}

void ExperimentControl::OpenExperiment(std::string name)
{
    if (name.empty()) {
        throw std::invalid_argument("empty experiment name");
    }

    // Create experiment dir
    std::filesystem::path exp_dir = BaseDir() / name;
    if (!std::filesystem::exists(exp_dir)) {
        if (!std::filesystem::create_directories(exp_dir)) {
            throw std::runtime_error(
                fmt::format("failed to create exp dir {}", exp_dir.string()));
        }
    }
    OpenExperimentDir(exp_dir);
}

void ExperimentControl::OpenExperimentDir(std::filesystem::path exp_dir)
{
    CloseExperiment();

    this->exp_dir = exp_dir;

    // Open or create experiment DB
    std::filesystem::path filename = exp_dir / "index.db";
    this->db = new ExperimentDB(filename);

    // Switch to experiment dir and load DB
    this->exp_dir = exp_dir;
    sample_manager->LoadFromDB();
    image_manager->LoadFromDB();

    // Done
    SendEvent({
        .type = EventType::ExperimentPathChanged,
        .value = exp_dir.string(),
    });
}

void ExperimentControl::CloseExperiment()
{
    if (this->db) {
        delete this->db;
        this->db = nullptr;
    }
    this->exp_dir = "";
    // TODO: notify
}

std::filesystem::path ExperimentControl::ExperimentDir()
{
    return exp_dir;
}

ExperimentDB *ExperimentControl::DB()
{
    return db;
}

DeviceHub *ExperimentControl::Devices()
{
    return dev;
}

SampleManager *ExperimentControl::Samples()
{
    return sample_manager;
}
ChannelControl *ExperimentControl::Channels()
{
    return channel_control;
}
ImageManager *ExperimentControl::Images()
{
    return image_manager;
}

AnalysisManager *ExperimentControl::Analysis()
{
    return analysis_manager;
}

void ExperimentControl::runLiveView()
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

    channel_control->OpenCurrentShutter();

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
        channel_control->CloseCurrentShutter();
        throw std::runtime_error(message);
    }

    is_busy = false;
    SendEvent({
        .type = EventType::TaskStateChanged,
        .value = "Ready",
    });
    LOG_INFO("Live view stopped");
    channel_control->CloseCurrentShutter();
}

void ExperimentControl::StartLiveView()
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
        std::async(std::launch::async, &ExperimentControl::runLiveView, this);
}

void ExperimentControl::StopLiveView()
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

bool ExperimentControl::IsLiveRunning()
{
    return live_view_task->IsRunning();
}

void ExperimentControl::runMultiChannelTask(std::string ndimage_name,
                                         std::vector<Channel> channels, int i_z,
                                         int i_t, Site *site,
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
                                                   i_t, site, metadata);
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

void ExperimentControl::AcquireMultiChannel(std::string ndimage_name,
                                         std::vector<Channel> channels, int i_z,
                                         int i_t, Site *site,
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
        std::async(std::launch::async, &ExperimentControl::runMultiChannelTask,
                   this, ndimage_name, channels, i_z, i_t, site, metadata);
}

void ExperimentControl::WaitMultiChannelTask()
{
    // Wait and get exception
    current_task_future.get();
}


void ExperimentControl::handleDeviceEvents()
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
