#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include <unordered_map>

struct Label {
    std::string name;
    std::string description;
};

extern std::unordered_map<std::string, double> configPixelSize;
extern std::unordered_map<std::string, std::unordered_map<std::string, Label>> configLabel;
extern std::unordered_map<std::string, std::unordered_map<std::string, std::string>> configChannel;

std::string propertyLabelToPosition(std::string name, std::string value);
std::string propertyLabelFromPosition(std::string name, std::string value);

std::vector<std::string> getChannelList();
std::unordered_map<std::string, std::string> getChannelConfig_Filter(std::string channel);
std::unordered_map<std::string, std::string> getChannelConfig_Shutter(std::string channel);
std::string getChannelFromDeviceProperties(std::unordered_map<std::string, std::string> properties);

#endif // CONFIG_H
