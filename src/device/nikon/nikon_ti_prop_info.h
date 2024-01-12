#ifndef DEVICE_NIKON_TI_PROP_INFO_H
#define DEVICE_NIKON_TI_PROP_INFO_H

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace NikonTi {

struct APIValueConvertor {
    std::function<std::string(std::string)> valueFromAPI;
    std::function<std::string(std::string)> valueToAPI;
};

struct PropInfo {
    std::string description;
    std::string default_value;
    std::vector<std::string> options;

    std::string mm_label;
    std::string mm_property;
    bool readonly;
    APIValueConvertor *value_converter;
};

extern std::map<std::string, PropInfo> prop_info;

} // namespace NikonTi

#endif
