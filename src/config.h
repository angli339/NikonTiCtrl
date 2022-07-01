#ifndef CONFIG_H
#define CONFIG_H

#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "channel.h"
#include "device/propertypath.h"

struct Label {
    std::string name;
    std::string description;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Label, name, description)

struct ConfigSystem {
    std::string unet_grpc_addr;
    std::map<std::string, double> pixel_size;
    std::map<PropertyPath, std::map<std::string, Label>> labels;
    std::vector<ChannelPreset> presets;
};

struct ConfigUser {
    std::string name;
    std::string email;
    std::string data_root;
};

struct Config {
    ConfigUser user;
    ConfigSystem system;
};

extern Config config;

std::filesystem::path getSystemConfigPath();
std::filesystem::path getUserConfigPath();

void loadSystemConfig(std::filesystem::path filename);
void loadUserConfig(std::filesystem::path filename);

#endif
