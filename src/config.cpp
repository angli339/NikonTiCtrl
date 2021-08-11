#include "config.h"

#include <fstream>
#include <stdexcept>
#include <unordered_set>

#include "utils/string_utils.h"

ConfigUser configUser;
std::map<std::string, double> configPixelSize;
std::map<std::string, std::map<std::string, Label>> configLabel;
std::map<std::string, std::map<std::string, std::string>> configChannel;
std::unordered_set<std::string> configShutterList = {"NikonTi/DiaShutter", "PriorProScan/LumenShutter"};

using json = nlohmann::json;

void loadConfig(fs::path filename)
{
     std::ifstream ifs(filename.string().c_str());
     json j = json::parse(ifs);

     configPixelSize = j.at("pixel_size").get<std::map<std::string, double>>();
     configLabel = j.at("label").get<std::map<std::string, std::map<std::string, Label>>>();
     configChannel = j.at("channel").get<std::map<std::string, std::map<std::string, std::string>>>();
}

void loadUserConfig(fs::path filename)
{
     std::ifstream ifs(filename.string().c_str());
     json j = json::parse(ifs);

     configUser = j.get<ConfigUser>();
}

std::string propertyLabelToPosition(std::string name, std::string value)
{
     auto it = configLabel.find(name);
     if (it == configLabel.end()) {
          return "";
     }
     auto configLabel_Property = it->second;

     for (const auto& [position, label] : configLabel_Property) {
          if (label.name == value) {
               return position;
          }
     }
     return "";
}

std::string propertyLabelFromPosition(std::string name, std::string value)
{
     auto it1 = configLabel.find(name);
     if (it1 == configLabel.end()) {
          return "";
     }
     auto configLabel_Property = it1->second;

     auto it2 = configLabel_Property.find(value);
     if (it1 == configLabel.end()) {
          return "";
     }
     auto label = it2->second;

     return label.name;
}

std::map<std::string, std::string> getChannelConfig_Filter(std::string channel)
{
     std::map<std::string, std::string> channelFilter;

     auto it = configChannel.find(channel);
     if (it == configChannel.end()) {
          throw std::invalid_argument("channel name not found in config");
     }
     auto channelConfig = it->second;

     for (const auto& kv : channelConfig) {
          std::string name = kv.first;
          std::string value = kv.second;

          // skip shutters
          if (configShutterList.find(name) != configShutterList.end()) {
               continue;
          }
          // Convert label to position
          if (util::hasSuffix(name, "/Label")) {
               name = util::trimSuffix(name, "/Label");
               value = propertyLabelToPosition(name, value);
          }
          channelFilter[name] = value;
     }
     return channelFilter;
}

std::map<std::string, std::string> getChannelConfig_Shutter(std::string channel)
{
     std::map<std::string, std::string> channelShutter;

     auto it = configChannel.find(channel);
     if (it == configChannel.end()) {
          throw std::invalid_argument("channel name not found in config");
     }
     auto channelConfig = it->second;

     for (const auto& kv : channelConfig) {
          std::string name = kv.first;
          std::string value = kv.second;

          // only keep shutters
          if (configShutterList.find(name) != configShutterList.end()) {
               channelShutter[name] = value;
          }
     }
     return channelShutter;
}

std::vector<std::string> getChannelList()
{
     std::vector<std::string> list;

     for (const auto& kv : configChannel) {
          auto channelName = kv.first;
          list.push_back(channelName);
     }
     return list;
}

std::string getChannelFromDeviceProperties(std::map<std::string, std::string> properties)
{
     for (const auto& channelName : getChannelList()) {
          auto channelConfig = getChannelConfig_Filter(channelName);

          bool match = true;

          for (const auto& [name, value] : channelConfig) {
               auto it = properties.find(name);
               if (it == properties.end()) {
                    match = false;
                    break;
               }
               if (it->second != value) {
                    match = false;
                    break;
               }
          }

          if (match) {
               return channelName;
          }
     }
     return "";
}