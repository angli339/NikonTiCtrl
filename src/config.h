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

struct ConfigUser {
    std::string name;
    std::string email;
};

struct Config {
    ConfigUser user;
    std::string data_root;
    std::string unet_grpc_addr;
    std::map<PropertyPath, std::map<std::string, Label>> labels;
    std::vector<ChannelPreset> presets;
};

extern Config config;

void loadConfig(std::filesystem::path filename);
void loadUserConfig(std::filesystem::path filename);

#endif
