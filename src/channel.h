#ifndef CHANNEL_H
#define CHANNEL_H

#include <map>
#include <string>

#include "device/propertypath.h"

struct ChannelPreset {
    std::string name;
    PropertyValueMap property_value;
    PropertyPath shutter_property;
    PropertyPath illumination_property;
    double default_exposure_ms;
    double default_illumination_intensity;
};

struct Channel {
    std::string preset_name;
    double exposure_ms;
    double illumination_intensity;
};

void from_json(const nlohmann::json& j, ChannelPreset& p);

#endif
