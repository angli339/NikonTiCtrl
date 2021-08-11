#ifndef CONFIG_H
#define CONFIG_H

#include <filesystem>
#include <string>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

struct Label {
    std::string name;
    std::string description;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Label, name, description)
};

struct ConfigUser {
    std::string name;
    std::string email;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConfigUser, name, email)
};

//
// Global variables for the configurations (as a temporary solution)
//
extern ConfigUser configUser;
extern std::map<std::string, double> configPixelSize;
extern std::map<std::string, std::map<std::string, Label>> configLabel;
extern std::map<std::string, std::map<std::string, std::string>> configChannel;

void loadConfig(fs::path filename);
void loadUserConfig(fs::path filename);
std::string propertyLabelToPosition(std::string name, std::string value);
std::string propertyLabelFromPosition(std::string name, std::string value);

std::vector<std::string> getChannelList();
std::map<std::string, std::string> getChannelConfig_Filter(std::string channel);
std::map<std::string, std::string> getChannelConfig_Shutter(std::string channel);
std::string getChannelFromDeviceProperties(std::map<std::string, std::string> properties);

#endif // CONFIG_H
