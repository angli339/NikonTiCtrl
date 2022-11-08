#include "channelcontrol.h"

#include "config.h"
#include "logging.h"

ChannelControl::ChannelControl(DeviceHub *dev)
{
    this->dev = dev;
    for (const auto &preset : config.system.presets) {
        preset_names.push_back(preset.name);
        preset_map[preset.name] = preset;

        for (const auto &[property, value] : preset.property_value) {
            required_properties.insert(property);
        }
        required_properties.insert(preset.shutter_property);
        required_properties.insert(preset.illumination_property);
    }
    for (const auto &property : required_properties) {
        required_devices.insert(property.DeviceName());
    }
}

std::vector<std::string> ChannelControl::ListPresetNames()
{
    return preset_names;
}

ChannelPreset ChannelControl::GetPreset(std::string preset_name)
{
    auto it = preset_map.find(preset_name);
    if (it == preset_map.end()) {
        throw std::invalid_argument("invalid preset");
    }
    return it->second;
}

Status ChannelControl::runSwitchChannel(ChannelPreset preset,
                                        double exposure_ms,
                                        double illumination_intensity)
{
    std::shared_lock<std::shared_mutex> lk_ch(channels_mutex);
    std::unique_lock<std::shared_mutex> lk_shutter(shutter_mutex);

    utils::StopWatch sw;

    auto snapshot = dev->GetPropertySnapshot(required_devices);
    auto channel_property_value =
        getChannelPropertyValue(preset, exposure_ms, illumination_intensity);
    auto diff = diffSnapshotPropertyValue(snapshot, channel_property_value);

    LOG_DEBUG("Switching to channel {}", preset.name);
    LOG_DEBUG("  Set Shutter=\"{}\"", preset.shutter_property);
    for (const auto &[property, value] : diff) {
        LOG_DEBUG("  Set {}=\"{}\"", property, value);
    }
    current_shutter = preset.shutter_property;
    Status status = dev->SetProperty(diff);
    if (!status.ok()) {
        return absl::UnavailableError(fmt::format(
            "switch to channel {}: {}", preset.name, status.ToString()));
    }

    status = dev->WaitPropertyFor(PropertyPathList(diff),
                                  std::chrono::milliseconds(5000));
    if (!status.ok()) {
        std::string message = fmt::format("timeout switching to channel {}: {}",
                                          preset.name, status.ToString());
        LOG_ERROR(message);
        SendEvent({
            .type = EventType::TaskMessage,
            .value = message,
        });
        return absl::DeadlineExceededError(message);
    }

    std::string message = fmt::format("Switched to channel {} [{:.0f} ms]",
                                      preset.name, sw.Milliseconds());
    LOG_INFO(message);
    SendEvent({
        .type = EventType::TaskChannelChanged,
        .value = preset.name,
    });
    SendEvent({
        .type = EventType::TaskMessage,
        .value = message,
    });
    return absl::OkStatus();
}

void ChannelControl::SwitchChannel(std::string preset_name, double exposure_ms,
                                   double illumination_intensity)
{
    if (switch_channel_future.valid()) {
        if (std::future_status::ready !=
            switch_channel_future.wait_for(std::chrono::seconds(0)))
        {
            throw std::runtime_error(
                "previous switch channel is not completed");
        }
        try {
            switch_channel_future.get();
        } catch (std::exception &e) {
            LOG_WARN("Ignoring error in previous switch channel: {}", e.what());
        }
    }

    ChannelPreset preset = GetPreset(preset_name);
    switch_channel_future =
        std::async(std::launch::async, &ChannelControl::runSwitchChannel, this,
                   preset, exposure_ms, illumination_intensity);
}

Status ChannelControl::WaitSwitchChannel()
{
    return switch_channel_future.get();
}

PropertyPath ChannelControl::GetCurrentShutter()
{
    std::shared_lock<std::shared_mutex> lk(shutter_mutex);
    return current_shutter;
}

StatusOr<bool> ChannelControl::IsCurrentShutterOpen()
{
    std::shared_lock<std::shared_mutex> lk(shutter_mutex);
    if (current_shutter.empty()) {
        return false;
    }
    StatusOr<std::string> value = dev->GetProperty(current_shutter);
    if (!value.ok()) {
        return value.status();
    }
    return value.value() == "On";
}

StatusOr<bool> ChannelControl::IsCurrentShutterClose()
{
    std::shared_lock<std::shared_mutex> lk(shutter_mutex);
    if (current_shutter.empty()) {
        return false;
    }
    StatusOr<std::string> value = dev->GetProperty(current_shutter);
    if (!value.ok()) {
        return value.status();
    }
    return value.value() == "Off";
}

Status ChannelControl::OpenCurrentShutter()
{
    std::shared_lock<std::shared_mutex> lk(shutter_mutex);
    if (current_shutter.empty()) {
        return absl::OkStatus();
    }
    return dev->SetProperty(current_shutter, "On");
}

Status ChannelControl::CloseCurrentShutter()
{
    std::shared_lock<std::shared_mutex> lk(shutter_mutex);
    if (current_shutter.empty()) {
        return absl::OkStatus();
    }
    return dev->SetProperty(current_shutter, "Off");
}

Status ChannelControl::WaitShutter()
{
    std::shared_lock<std::shared_mutex> lk(shutter_mutex);
    if (current_shutter.empty()) {
        return absl::OkStatus();
    }
    return dev->WaitPropertyFor({current_shutter},
                                std::chrono::milliseconds(300));
}

PropertyValueMap ChannelControl::getChannelPropertyValue(
    ChannelPreset preset, double exposure_ms, double illumination_intensity)
{
    auto channel_property_value = preset.property_value;
    if (!preset.illumination_property.empty()) {
        channel_property_value[preset.illumination_property] =
            fmt::format("{:.0f}", std::round(illumination_intensity));
    }
    double exposure_s = exposure_ms / 1000.0;
    channel_property_value["/Hamamatsu/EXPOSURE TIME"] =
        fmt::format("{:.6g}", exposure_s);

    return channel_property_value;
}

PropertyValueMap
ChannelControl::diffSnapshotPropertyValue(PropertyValueMap snapshot,
                                          PropertyValueMap propery_value)
{
    PropertyValueMap diff;
    for (const auto &[property, value] : propery_value) {
        auto it = snapshot.find(property);
        if (it == snapshot.end()) {
            diff[property] = value;
        } else if (it->second != value) {
            diff[property] = value;
        }
    }
    return diff;
}
