#include "channelcontrol_model.h"

#include "logging.h"

ChannelControlModel::ChannelControlModel(ChannelControl *channelControl,
                                         QObject *parent)
    : QObject(parent)
{
    this->channelControl = channelControl;

    for (const auto &preset_name : channelControl->ListPresetNames()) {
        ChannelPreset preset = channelControl->GetPreset(preset_name);

        Channel channel;
        channel.preset_name = preset_name;
        channel.exposure_ms = preset.default_exposure_ms;
        if (!preset.illumination_property.empty()) {
            channel.illumination_intensity =
                preset.default_illumination_intensity;
        }

        channel_names.push_back(preset_name.c_str());
        channel_enabled[preset_name.c_str()] = false;
        channels[preset_name.c_str()] = channel;
    }
}


QStringList ChannelControlModel::ListChannelNames() { return channel_names; }

Channel ChannelControlModel::GetChannel(QString name) { return channels[name]; }

void ChannelControlModel::SetChannelExposure(QString name, int exposure_ms)
{
    channels[name].exposure_ms = exposure_ms;
}

void ChannelControlModel::SetChannelIllumination(QString name,
                                                 int illumination_intensity)
{
    channels[name].illumination_intensity = illumination_intensity;
}

void ChannelControlModel::SwitchChannel(QString name)
{
    try {
        Channel channel = channels[name];
        channelControl->SwitchChannel(channel.preset_name, channel.exposure_ms,
                                      channel.illumination_intensity);
    } catch (std::exception &e) {
        LOG_ERROR(e.what());
    }
}

void ChannelControlModel::SetChannelEnabled(QString name, bool enabled)
{
    channel_enabled[name] = enabled;
}

bool ChannelControlModel::IsChannelEnabled(QString name)
{
    return channel_enabled[name];
}

std::vector<Channel> ChannelControlModel::GetEnabledChannels()
{
    std::vector<Channel> enabled_channels;
    for (const auto &channel_name : channel_names) {
        if (channel_enabled[channel_name]) {
            enabled_channels.push_back(channels[channel_name]);
        }
    }
    return enabled_channels;
}