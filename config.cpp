#include "config.h"
#include "utils/string_utils.h"

#include <unordered_set>

std::unordered_map<std::string, double> configPixelSize = {
    {"20x", 0.3125},
    {"60xO", 0.1072},
    {"100xO", 0.0668},
};

std::unordered_map<std::string, std::unordered_map<std::string, Label>> configLabel = {
    {"NikonTi/NosePiece", {
         {"1", {.name = "20x", .description = "Plan Apo 20x NA 0.75 Dry"}},
         {"2", {.name = "40x", .description = "---"}},
         {"3", {.name = "60xO", .description = "Plan Apo VC 60x NA 1.40 Oil"}},
         {"4", {.name = "100xO", .description = "Plan Apo VC 100x NA 1.40 Oil"}},
         {"5", {.name = "---", .description = "---"}},
         {"6", {.name = "---", .description = "---"}},
    }},
    {"NikonTi/LightPath", {
        {"1", {.name = "E100", .description = "Eye100"}},
        {"2", {.name = "L100", .description = "Left100"}},
        {"3", {.name = "R100", .description = "Right100"}},
        {"4", {.name = "L80", .description = "Left80"}},
    }},
    {"NikonTi/FilterBlock1", {
        {"1", {.name = "---", .description = "---"}},
        {"2", {.name = "BCYR", .description = "Semrock FF444/520/590-Di01-25x36"}},
        {"3", {.name = "CFP", .description = "Semrock FF01-438/24 FF458-Di01"}},
        {"4", {.name = "GFP", .description = "Semrock FF01-472/30 FF495-Di02 FF01-520/35"}},
        {"5", {.name = "YFP", .description = "Semrock FF520-Di01"}},
        {"6", {.name = "RFP", .description = "Semrock FF593-Di02"}},
    }},
    {"PriorProScan/FilterWheel1", {
        {"1", {.name = "---", .description = "---"}},
        {"2", {.name = "RFP", .description = "FF01-562/40"}},
        {"3", {.name = "BFP", .description = "FF01-390/40"}},
        {"4", {.name = "YFP", .description = "FF01-500/24"}},
        {"5", {.name = "---", .description = "---"}},
        {"6", {.name = "---", .description = "---"}},
    }},
    {"PriorProScan/FilterWheel3", {
        {"1", {.name = "RFP", .description = "FF01-641/75"}},
        {"2", {.name = "BFP", .description = "FF01-452/45"}},
        {"3", {.name = "YFP", .description = "FF01-542/27"}},
        {"4", {.name = "CFP", .description = "FF01-483/32"}},
        {"5", {.name = "---", .description = "---"}},
        {"6", {.name = "Dark", .description = "Aluminum Disc"}},
        {"7", {.name = "---", .description = "---"}},
        {"8", {.name = "---", .description = "---"}},
        {"9", {.name = "---", .description = "---"}},
        {"10", {.name = "---", .description = "---"}},
    }},
};

std::unordered_map<std::string, std::unordered_map<std::string, std::string>> configChannel = {
    {"BF", {
         {"NikonTi/FilterBlock1/Label", "BCYR"},
         {"NikonTi/DiaShutter", "On"},
         {"NikonTi/LightPath/Label", "L100"},
         {"PriorProScan/LumenShutter", "Off"},
         {"PriorProScan/FilterWheel3", "5"},
    }},
    {"RFP", {
         {"NikonTi/FilterBlock1/Label", "BCYR"},
         {"NikonTi/DiaShutter", "Off"},
         {"PriorProScan/LumenShutter", "On"},
         {"PriorProScan/LumenOutputIntensity", "15"},
         {"PriorProScan/FilterWheel1/Label", "RFP"},
         {"PriorProScan/FilterWheel3/Label", "RFP"},
    }},
    {"BFP", {
         {"NikonTi/FilterBlock1/Label", "BCYR"},
         {"NikonTi/DiaShutter", "Off"},
         {"PriorProScan/LumenShutter", "On"},
         {"PriorProScan/LumenOutputIntensity", "25"},
         {"PriorProScan/FilterWheel1/Label", "BFP"},
         {"PriorProScan/FilterWheel3/Label", "BFP"},
    }},
    {"YFP", {
         {"NikonTi/FilterBlock1/Label", "BCYR"},
         {"NikonTi/DiaShutter", "Off"},
         {"PriorProScan/LumenShutter", "On"},
         {"PriorProScan/LumenOutputIntensity", "100"},
         {"PriorProScan/FilterWheel1/Label", "YFP"},
         {"PriorProScan/FilterWheel3/Label", "YFP"},
    }},
    {"CFP-BF", {
         {"NikonTi/FilterBlock1/Label", "CFP"},
         {"NikonTi/DiaShutter", "On"},
         {"PriorProScan/LumenShutter", "Off"},
         {"PriorProScan/FilterWheel3", "5"},
    }},
    
    {"CFP", {
         {"NikonTi/FilterBlock1/Label", "CFP"},
         {"NikonTi/DiaShutter", "Off"},
         {"PriorProScan/LumenShutter", "On"},
         {"PriorProScan/LumenOutputIntensity", "10"},
         {"PriorProScan/FilterWheel1", "1"},
         {"PriorProScan/FilterWheel3/Label", "CFP"},
    }},
    {"CFP-YFP FRET", {
         {"NikonTi/FilterBlock1/Label", "CFP"},
         {"NikonTi/DiaShutter", "Off"},
         {"PriorProScan/LumenShutter", "On"},
         {"PriorProScan/LumenOutputIntensity", "10"},
         {"PriorProScan/FilterWheel1", "1"},
         {"PriorProScan/FilterWheel3/Label", "YFP"},
    }},
    {"CFP_YFP_FRET", {
         {"NikonTi/FilterBlock1/Label", "CFP"},
         {"NikonTi/DiaShutter", "Off"},
         {"PriorProScan/LumenShutter", "On"},
         {"PriorProScan/LumenOutputIntensity", "10"},
         {"PriorProScan/FilterWheel1", "1"},
         {"PriorProScan/FilterWheel3/Label", "YFP"},
    }},
    {"GFP", {
         {"NikonTi/FilterBlock1/Label", "GFP"},
         {"NikonTi/DiaShutter", "Off"},
         {"PriorProScan/LumenShutter", "On"},
         {"PriorProScan/LumenOutputIntensity", "100"},
         {"PriorProScan/FilterWheel1", "1"},
         {"PriorProScan/FilterWheel3", "5"},
    }},
    {"DarkFrame", {
         {"NikonTi/DiaShutter", "Off"},
         {"PriorProScan/LumenShutter", "Off"},
         {"PriorProScan/FilterWheel3/Label", "Dark"},
    }},

    {"LED_CFP", {
         {"NikonTi/FilterBlock1/Label", "CFP"},
         {"NikonTi/DiaShutter", "Off"},
         {"PriorProScan/FilterWheel3/Label", "CFP"},
    }},
    {"LED_CFP-YFP_FRET", {
         {"NikonTi/FilterBlock1/Label", "CFP"},
         {"NikonTi/DiaShutter", "Off"},
         {"PriorProScan/FilterWheel3/Label", "YFP"},
    }},
};

std::unordered_set<std::string> configShutterList = {"NikonTi/DiaShutter", "PriorProScan/LumenShutter"};

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

std::unordered_map<std::string, std::string> getChannelConfig_Filter(std::string channel)
{
     std::unordered_map<std::string, std::string> channelFilter;

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

std::unordered_map<std::string, std::string> getChannelConfig_Shutter(std::string channel)
{
     std::unordered_map<std::string, std::string> channelShutter;

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

std::string getChannelFromDeviceProperties(std::unordered_map<std::string, std::string> properties)
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