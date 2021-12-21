#include "device/nikon/nikon_ti_prop_info.h"

#include <stdexcept>

#include <fmt/format.h>

namespace NikonTi {

APIValueConvertor converter_MMState_Position = {
    .valueFromAPI = [](std::string mm_value) -> std::string {
        uint8_t state = std::stoi(mm_value);
        uint8_t position = state + 1;
        return std::to_string(position);
    },
    .valueToAPI = [](std::string value) -> std::string {
        uint8_t position = std::stoi(value);
        uint8_t state = position - 1;
        return std::to_string(state);
    },
};

APIValueConvertor converter_MMInt_OnOff = {
    .valueFromAPI = [](std::string mm_value) -> std::string {
        if (mm_value == "1") {
            return "On";
        } else if (mm_value == "0") {
            return "Off";
        } else {
            throw std::invalid_argument(
                fmt::format("invalid value \"{}\"", mm_value));
        }
    },
    .valueToAPI = [](std::string value) -> std::string {
        if (value == "On") {
            return "1";
        } else if (value == "Off") {
            return "0";
        } else {
            throw std::invalid_argument(
                fmt::format("invalid value \"{}\"", value));
        }
    },
};

APIValueConvertor converter_PFSOffset = {
    .valueFromAPI = [](std::string mm_value) -> std::string {
        double pos = std::stod(mm_value);
        return fmt::format("{:.3f}", pos);
    },
    .valueToAPI = [](std::string value) -> std::string { return value; },
};

std::map<std::string, PropInfo> prop_info = {
    {"DeviceAddress",
     {
         .description = "Device Address",
         .mm_label = "TIScope",
         .mm_property = "DeviceAddress",
         .readonly = true,
     }},
    {"DeviceIndex",
     {
         .description = "Device Index",
         .mm_label = "TIScope",
         .mm_property = "DeviceIndex",
         .readonly = true,
     }},
    {"DriverVersion",
     {
         .description = "Driver Version",
         .mm_label = "TIScope",
         .mm_property = "DriverVersion",
         .readonly = true,
     }},
    {"FirmwareVersion",
     {
         .description = "Firmware Version",
         .mm_label = "TIScope",
         .mm_property = "FirmwareVersion",
         .readonly = true,
     }},
    {"SoftwareVersion",
     {
         .description = "Software Version",
         .mm_label = "TIScope",
         .mm_property = "SoftwareVersion",
         .readonly = true,
     }},
    {"FilterBlock1",
     {
         .description = "Filter Block 1 Position",
         .options = {"1", "2", "3", "4", "5", "6"},
         .mm_label = "TIFilterBlock1",
         .mm_property = "State",
         .readonly = false,
         .value_converter = &converter_MMState_Position,
     }},
    {"LightPath",
     {
         .description = "Light Path Position",
         .options = {"1", "2", "3", "4"},
         .mm_label = "TILightPath",
         .mm_property = "State",
         .readonly = false,
         .value_converter = &converter_MMState_Position,
     }},
    {"NosePiece",
     {
         .description = "Nose Piece Position. Options: [1, 2, 3, 4, 5, 6]",
         .options = {"1", "2", "3", "4", "5", "6"},
         .mm_label = "TINosePiece",
         .mm_property = "State",
         .readonly = false,
         .value_converter = &converter_MMState_Position,
     }},
    {"DiaShutter",
     {
         .description = "Dia Shutter",
         .options = {"On", "Off"},
         .mm_label = "TIDiaShutter",
         .mm_property = "State",
         .readonly = false,
         .value_converter = &converter_MMInt_OnOff,
     }},
    {"DiaLampComputerControl",
     {
         .description = "Dia Lamp Computer Control",
         .options = {"On", "Off"},
         .mm_label = "TIDiaLamp",
         .mm_property = "ComputerControl",
         .readonly = false,
     }},
    {"DiaLampIntensity",
     {
         .description = "Dia Lamp Intensity. Range: [0, 24]",
         .mm_label = "TIDiaLamp",
         .mm_property = "Intensity",
         .readonly = false,
     }},
    {"DiaLampOnOff",
     {
         .description = "Dia Lamp On/Off",
         .options = {"On", "Off"},
         .mm_label = "TIDiaLamp",
         .mm_property = "State",
         .readonly = false,
         .value_converter = &converter_MMInt_OnOff,
     }},
    {"ZDrivePosition",
     {
         .description = "Z Drive Position",
         .mm_label = "TIZDrive",
         .mm_property = "", // This property requires GetPosition(), and is not
                            // accessable from GetProperty().
         .readonly = false,
     }},
    {"ZDriveSpeed",
     {
         .description = "Z Drive Speed",
         .default_value = "1",
         .options = {"1", "2", "3", " 4", "5", "6", "7", "8", "9"},
         .mm_label = "TIZDrive",
         .mm_property = "Speed",
         .readonly = false,
     }},
    {"ZDriveTolerance",
     {
         .description = "Z Drive Tolerance",
         .default_value = "0",
         .options = {"0", "1", "2", "3", " 4", "5", "6", "7", "8", "9"},
         .mm_label = "TIZDrive",
         .mm_property = "Tolerance",
         .readonly = false,
     }},
    {"PFSOffset",
     {
         .description = "PFS Offset. Range: [0.0, 1000.0]",
         .mm_label = "TIPFSOffset",
         .mm_property = "Position",
         .readonly = false,
         .value_converter = &converter_PFSOffset,
     }},
    {"PFSStatus",
     {
         .description = "PFS Status",
         .options = {"Out of focus search range", "Focusing", "Locked"},
         .mm_label = "TIPFSStatus",
         .mm_property = "Status",
         .readonly = true,
     }},
    {"PFSState",
     {
         .description = "PFS State",
         .options = {"On", "Off"},
         .mm_label = "TIPFSStatus",
         .mm_property = "State",
         .readonly = false,
     }},
    {"PFSFullFocusTimeoutMs",
     {
         .description = "PFS Full Focus Timeout Ms. Range: unknown",
         .default_value = "5000",
         .mm_label = "TIPFSStatus",
         .mm_property = "FullFocusTimeoutMs",
         .readonly = false,
     }},
    {"PFSFullFocusWaitAfterLockMs",
     {
         .description = "PFS Full Focus Wait After Lock Ms. Range: unknown",
         .default_value = "0",
         .mm_label = "TIPFSStatus",
         .mm_property = "FullFocusWaitAfterLockMs",
         .readonly = false,
     }},
};

} // namespace NikonTi
