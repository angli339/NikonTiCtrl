#ifndef DEVICE_PRIOR_PROSCAN_PROP_INFO_H
#define DEVICE_PRIOR_PROSCAN_PROP_INFO_H

#include <string>
#include <map>

namespace PriorProscan {

struct PropInfo {
    std::string name;
    std::string description;

    std::string getCommand;
    std::string setCommand;
    std::string setResponse;

    bool isVolatile;
};

extern std::map<std::string, std::string> error_code;
extern std::map<std::string, PropInfo> prop_info;

} // namespace PriorProscan

#endif
