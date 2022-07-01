#ifndef CHANNELCONTROL_H
#define CHANNELCONTROL_H

#include <future>
#include <map>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

#include "channel.h"
#include "device/devicehub.h"
#include "device/propertypath.h"
#include "eventstream.h"

class ChannelControl : public EventSender {
public:
    ChannelControl(DeviceHub *hub);

    std::vector<std::string> ListPresetNames();
    ChannelPreset GetPreset(std::string preset_name);
    void SwitchChannel(std::string preset_name, double exposure_ms,
                       double illumination_intensity);
    Status WaitSwitchChannel();

    PropertyPath GetCurrentShutter();
    StatusOr<bool> IsCurrentShutterOpen();
    StatusOr<bool> IsCurrentShutterClose();
    Status OpenCurrentShutter();
    Status CloseCurrentShutter();
    Status WaitShutter();

private:
    DeviceHub *hub;

    std::vector<std::string> preset_names;
    std::map<std::string, ChannelPreset> preset_map;
    std::set<std::string> required_devices;
    std::set<PropertyPath> required_properties;

    std::shared_mutex channels_mutex;
    std::shared_mutex shutter_mutex;
    PropertyPath current_shutter;

    Status runSwitchChannel(ChannelPreset preset, double exposure_ms,
                            double illumination_intensity);
    std::future<Status> switch_channel_future;

    // helper functions:
    PropertyValueMap
    getChannelPropertyValue(ChannelPreset preset, double exposure_ms,
                            double illumination_intensity);

    PropertyValueMap diffSnapshotPropertyValue(
        PropertyValueMap snapshot,
        PropertyValueMap propery_value);
};

#endif
