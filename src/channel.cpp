#include "channel.h"

void from_json(const nlohmann::json &j, ChannelPreset &p)
{
    j.at("name").get_to(p.name);
    j.at("property_value").get_to(p.property_value);

    if (j.contains("shutter_property")) {
        p.shutter_property = j.at("shutter_property").get<std::string>();
    }

    if (j.contains("illumination_property")) {
        p.illumination_property =
            j.at("illumination_property").get<std::string>();
    }

    if (j.contains("default_exposure_ms")) {
        j.at("default_exposure_ms").get_to(p.default_exposure_ms);
    }

    if (j.contains("default_illumination_intensity")) {
        j.at("default_illumination_intensity")
            .get_to(p.default_illumination_intensity);
    }
}