#include "task/multi_channel_task.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "logging.h"
#include "utils/time_utils.h"

MultiChannelTask::MultiChannelTask(DeviceHub *hub, Hamamatsu::DCam *dcam,
                                   ChannelControl *channel_control,
                                   ImageManager *data_manager)
{
    this->hub = hub;
    this->dcam = dcam;
    this->channel_control = channel_control;
    this->image_manager = data_manager;
}

Status MultiChannelTask::EnableTrigger()
{
    // Enable trigger
    utils::StopWatch sw_trigger;
    StatusOr<std::string> trigger_source = dcam->GetProperty("TRIGGER SOURCE");
    if (!trigger_source.ok()) {
        return trigger_source.status();
    }

    if (trigger_source.value() != "SOFTWARE") {
        Status status = dcam->SetProperty("TRIGGER SOURCE", "SOFTWARE");
        if (!status.ok()) {
            return status;
        }
        LOG_DEBUG("[{}] Software trigger enabled [{:.1f} ms]", ndimage_name,
                  sw_trigger.Milliseconds());
    } else {
        LOG_DEBUG("[{}] Software trigger already enabled [{:.1f} ms]",
                  ndimage_name, sw_trigger.Milliseconds());
    }
    return absl::OkStatus();
}

Status MultiChannelTask::PrepareBuffer()
{
    utils::StopWatch sw;
    if (dcam->BufferAllocated() < channels.size()) {
        if (dcam->BufferAllocated() > 0) {
            LOG_DEBUG("[{}] Releasing Buffer (n_frame={})...", ndimage_name,
                      dcam->BufferAllocated());
            sw.Reset();
            dcam->ReleaseBuffer();
            LOG_DEBUG("[{}] Buffer released [{:.1f} ms]", ndimage_name,
                      sw.Milliseconds());
        }
        sw.Reset();
        Status status = dcam->AllocBuffer(channels.size());
        if (!status.ok()) {
            return status;
        }
        LOG_DEBUG("[{}] Buffer allocated (n_frame={}) [{:.1f} ms]",
                  ndimage_name, channels.size(), sw.Milliseconds());
    } else {
        LOG_DEBUG("[{}] Using existing buffer (n_frame={})", ndimage_name,
                  channels.size());
    }
    return absl::OkStatus();
}

Status MultiChannelTask::StartAcqusition()
{
    utils::StopWatch sw;
    Status status = dcam->StartContinousAcquisition();
    if (!status.ok()) {
        return status;
    }
    LOG_DEBUG("[{}] Sequence acquisition started [{:.1f} ms]", ndimage_name,
              sw.Milliseconds());
    return absl::OkStatus();
}

PropertyValueMap MultiChannelTask::ExposeFrame(int i_ch)
{
    Channel channel = channels[i_ch];

    utils::StopWatch sw;

    Status status = channel_control->OpenCurrentShutter();
    if (!status.ok()) {
        LOG_ERROR("[{}][{}] Shutter failed to turn on: {} [{:.1f} ms]",
                  ndimage_name, i_ch + 1, status.ToString(), sw.Milliseconds());
    }
    status = channel_control->WaitShutter();
    if (!status.ok()) {
        LOG_ERROR(
            "[{}][{}] Shutter failed to turn on after waiting: {} [{:.1f} ms]",
            ndimage_name, i_ch + 1, status.ToString(), sw.Milliseconds());
    }
    LOG_DEBUG("[{}][{}] Shutter turned on [{:.1f} ms]", ndimage_name, i_ch + 1,
              sw.Milliseconds());

    sw.Reset();
    dcam->FireTrigger();
    LOG_DEBUG("[{}][{}] Trigger fired [{:.1f} ms]", ndimage_name, i_ch + 1,
              sw.Milliseconds());

    sw.Reset();
    auto property_snapshot = hub->GetPropertySnapshot();
    LOG_DEBUG("[{}][{}] Device status snapshot got [{:.1f} ms]", ndimage_name,
              i_ch + 1, sw.Milliseconds());

    sw.Reset();
    dcam->WaitExposureEnd(channel.exposure_ms + 500);
    sw_exposure_end.Reset();
    LOG_DEBUG("[{}][{}] Exposure completed [{:.1f} ms]", ndimage_name, i_ch + 1,
              sw.Milliseconds());

    sw.Reset();
    status = channel_control->CloseCurrentShutter();
    if (!status.ok()) {
        LOG_ERROR("[{}][{}] Shutter failed to turn off: {} [{:.1f} ms]",
                  ndimage_name, i_ch + 1, status.ToString(), sw.Milliseconds());
    }
    status = channel_control->WaitShutter();
    if (!status.ok()) {
        LOG_ERROR(
            "[{}][{}] Shutter failed to turn off after waiting: {} [{:.1f} ms]",
            ndimage_name, i_ch + 1, status.ToString(), sw.Milliseconds());
    }
    LOG_DEBUG("[{}][{}] Shutter turned off [{:.1f} ms]", ndimage_name, i_ch + 1,
              sw.Milliseconds());


    return property_snapshot;
}

ImageData
MultiChannelTask::GetFrame(int i_ch,
                           std::chrono::system_clock::time_point *timestamp)
{
    Channel channel = channels[i_ch];
    Status status = dcam->WaitFrameReady(1000);
    if (!status.ok()) {
        // This should not happen
        // Log and then continue anyways to see whether it is possible to get
        // frame data if there is no data, throw exception there
        LOG_WARN(
            "[{}][{}] WaitFrameReady returned false, which indicates ABORT",
            ndimage_name, i_ch);
    } else {
        LOG_DEBUG("[{}][{}] Frame ready [{:.1f} ms after exposure end]",
                  ndimage_name, i_ch + 1, sw_exposure_end.Milliseconds());
    }

    utils::StopWatch sw;
    StatusOr<ImageData> frame = dcam->GetFrame(i_ch, timestamp);
    if (!frame.ok()) {
        throw std::runtime_error(
            "GetFrame returned nullptr without throwing an exception");
    }
    LOG_DEBUG("[{}][{}] Get frame [{:.1f} ms]", ndimage_name, i_ch + 1,
              sw.Milliseconds());

    return frame.value();
}

Status MultiChannelTask::StopAcqusition()
{
    utils::StopWatch sw;
    StatusOr<bool> shutter_open = channel_control->IsCurrentShutterOpen();
    if (shutter_open.ok()) {
        if (*shutter_open) {
            LOG_WARN("[{}] Shutter is still open. Turning off...",
                     ndimage_name);

            sw.Reset();
            Status status = channel_control->CloseCurrentShutter();

            if (status.ok()) {
                LOG_INFO("[{}] Shutter turned off. [{:.1f} ms]", ndimage_name,
                         sw.Milliseconds());
            } else {
                LOG_ERROR("[{}] Failed to close shutter: {} [{:.1f} ms]",
                          ndimage_name, status.ToString(), sw.Milliseconds());
            }
        }
    } else {
        LOG_WARN("[{}] Get current shutter state: {}", ndimage_name,
                 shutter_open.status().ToString());
    }

    sw.Reset();
    dcam->StopAcquisition();
    LOG_DEBUG("[{}] Acquisition stopped [{:.1f} ms]", ndimage_name,
              sw.Milliseconds());
    return absl::OkStatus();
}

Status MultiChannelTask::Acquire(std::string ndimage_name,
                                 std::vector<Channel> channels, int i_z,
                                 int i_t, nlohmann::ordered_json metadata)
{
    if (channels.empty()) {
        throw std::invalid_argument("channel not set");
    }
    this->ndimage_name = ndimage_name;
    this->channels = channels;

    utils::StopWatch sw_task;
    LOG_INFO("[{}] Prepare acquisition", ndimage_name);

    //
    // Check and enable software trigger
    //
    Status status = EnableTrigger();
    if (!status.ok()) {
        return status;
    }

    //
    // Switch to channel 0
    //
    Channel channel = channels[0];
    channel_control->SwitchChannel(channel.preset_name, channel.exposure_ms,
                                   channel.illumination_intensity);

    //
    // Check and allocate camera buffer
    //
    status = PrepareBuffer();
    if (!status.ok()) {
        return status;
    }

    //
    // Create NDImage
    //
    DataType dtype = dcam->GetDataType();
    ColorType ctype = dcam->GetColorType();
    uint32_t width = dcam->GetWidth();
    uint32_t height = dcam->GetHeight();
    std::vector<NDImageChannel> nd_channels;
    for (const auto &channel : channels) {
        NDImageChannel nd_channel;
        nd_channel.name = channel.preset_name;
        nd_channel.width = width;
        nd_channel.height = height;
        nd_channel.dtype = dtype;
        nd_channel.ctype = ctype;
        nd_channels.push_back(nd_channel);
    }
    image_manager->NewNDImage(ndimage_name, nd_channels);

    //
    // Start acquisition
    //
    LOG_INFO("[{}] Starting acquisition", ndimage_name);
    SendEvent({
        .type = EventType::TaskStateChanged,
        .value = "Running",
    });
    status = StartAcqusition();
    if (!status.ok()) {
        return status;
    }

    //
    // Acquire images
    //
    utils::StopWatch sw_frame;
    utils::StopWatch sw_channel;
    try {
        for (int i_ch = 0; i_ch < channels.size(); i_ch++) {
            status = channel_control->WaitSwitchChannel();
            if (!status.ok()) {
                StopAcqusition();
                return status;
            }

            channel = channels[i_ch];
            LOG_INFO("[{}][{}/{}] Acquiring channel {}", ndimage_name, i_ch + 1,
                     channels.size(), channel.preset_name);
            SendEvent({
                .type = EventType::TaskMessage,
                .value = fmt::format("Acquiring channel {}/{}...", i_ch + 1,
                                     channels.size()),
            });

            sw_frame.Reset();

            auto property_snapshot = ExposeFrame(i_ch);
            if (i_ch + 1 < channels.size()) {
                sw_channel.Reset();
                Channel next_channel = channels[i_ch + 1];
                channel_control->SwitchChannel(
                    next_channel.preset_name, next_channel.exposure_ms,
                    next_channel.illumination_intensity);
            }

            std::chrono::system_clock::time_point timestamp;
            ImageData data = GetFrame(i_ch, &timestamp);

            nlohmann::ordered_json new_metadata;
            new_metadata["timestamp"] =
                utils::TimePoint(timestamp).FormatRFC3339_Local();
            ChannelPreset preset =
                channel_control->GetPreset(channel.preset_name);
            new_metadata["channel"] = {
                {"preset_name", channel.preset_name},
                {"exposure_ms", channel.exposure_ms},
            };
            if (!preset.illumination_property.empty()) {
                new_metadata["channel"]["illumination_intensity"] =
                    channel.illumination_intensity;
            }

            for (const auto &[k, v] : metadata.items()) {
                new_metadata[k] = v;
            }

            for (const auto &[k, v] : property_snapshot) {
                new_metadata["device_property"][k.ToString()] = v;
            }

            image_manager->AddImage(ndimage_name, i_ch, i_z, i_t, data,
                                   new_metadata);
            LOG_INFO("[{}][{}] Frame completed [{:.0f} ms]", ndimage_name,
                     i_ch + 1, sw_frame.Milliseconds());
        }
    } catch (std::exception &e) {
        StopAcqusition();
        throw;
    }

    StopAcqusition();
    double task_elapse_ms = sw_task.Milliseconds();
    LOG_INFO("[{}] Task completed: {:.0f} ms", ndimage_name, task_elapse_ms);

    SendEvent({
        .type = EventType::TaskStateChanged,
        .value = "Ready",
    });
    SendEvent({
        .type = EventType::TaskMessage,
        .value = fmt::format("Task {} completed [{:.0f} ms]", ndimage_name,
                             task_elapse_ms),
    });
    return absl::OkStatus();
}
