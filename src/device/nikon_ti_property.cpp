#include "nikon_ti_property.h"

#include <stdexcept>

MMValueConvertor converter_MMState_Position = {
    .valueFromMMValue = [](std::string mmValue) -> std::string {
        uint8_t state = std::stoi(mmValue);
        uint8_t position = state + 1;
        return std::to_string(position);
    },
    .valueToMMValue = [](std::string value) -> std::string {
        uint8_t position = std::stoi(value);
        uint8_t state = position - 1;
        return std::to_string(state);
    },
};

MMValueConvertor converter_MMInt_OnOff = {
    .valueFromMMValue = [](std::string mmValue) -> std::string {
        if (mmValue == "1") {
            return "On";
        } else if (mmValue == "0") {
            return "Off";
        } else {
            throw std::invalid_argument("invalid value: '" + mmValue + "'");
        }
    },
    .valueToMMValue = [](std::string value) -> std::string {
        if (value == "On") {
            return "1";
        } else if (value == "Off") {
            return "0";
        } else {
            throw std::invalid_argument("invalid value: '" + value + "'");
        }
    },
};

std::unordered_map<std::string, PropInfoNikonTi> propInfoNikonTi = {
    {"DeviceAddress", {
        .description = "Device Address",
        .mmLabel = "TIScope",
        .mmProperty = "DeviceAddress",
        .readonly = true,
        .mmValueConverter = nullptr,
    }},
    {"DeviceIndex", {
        .description = "Device Index",
        .mmLabel = "TIScope",
        .mmProperty = "DeviceIndex",
        .readonly = true,
        .mmValueConverter = nullptr,
    }},
    {"DriverVersion", {
        .description = "Driver Version",
        .mmLabel = "TIScope",
        .mmProperty = "DriverVersion",
        .readonly = true,
        .mmValueConverter = nullptr,
    }},
    {"FirmwareVersion", {
        .description = "Firmware Version",
        .mmLabel = "TIScope",
        .mmProperty = "FirmwareVersion",
        .readonly = true,
        .mmValueConverter = nullptr,
    }},
    {"SoftwareVersion", {
        .description = "Software Version",
        .mmLabel = "TIScope",
        .mmProperty = "SoftwareVersion",
        .readonly = true,
        .mmValueConverter = nullptr,
    }},
    {"FilterBlock1", {
        .description = "Filter Block 1 Position. Options: [1, 2, 3, 4, 5, 6]",
        .mmLabel = "TIFilterBlock1",
        .mmProperty = "State",
        .readonly = false,
        .mmValueConverter = &converter_MMState_Position,
    }},
    {"LightPath", {
        .description = "Light Path Position. Options: [1, 2, 3, 4]",
        .mmLabel = "TILightPath",
        .mmProperty = "State",
        .readonly = false,
        .mmValueConverter = &converter_MMState_Position,
    }},
    {"NosePiece", {
        .description = "Nose Piece Position. Options: [1, 2, 3, 4, 5, 6]",
        .mmLabel = "TINosePiece",
        .mmProperty = "State",
        .readonly = false,
        .mmValueConverter = &converter_MMState_Position,
    }},
    {"DiaShutter", {
        .description = "Dia Shutter. Options: [On, Off]",
        .mmLabel = "TIDiaShutter",
        .mmProperty = "State",
        .readonly = false,
        .mmValueConverter = &converter_MMInt_OnOff,
    }},
    {"DiaLampComputerControl", {
        .description = "Dia Lamp Computer Control. Options: [On, Off]",
        .mmLabel = "TIDiaLamp",
        .mmProperty = "ComputerControl",
        .readonly = false,
        .mmValueConverter = nullptr,
    }},
    {"DiaLampIntensity", {
        .description = "Dia Lamp Intensity. Range: [0, 24]",
        .mmLabel = "TIDiaLamp",
        .mmProperty = "Intensity",
        .readonly = false,
        .mmValueConverter = nullptr,
    }},
    {"DiaLampOnOff", {
        .description = "Dia Lamp On/Off. Options: [On, Off]",
        .mmLabel = "TIDiaLamp",
        .mmProperty = "State",
        .readonly = false,
        .mmValueConverter = &converter_MMInt_OnOff,
    }},
    {"ZDrivePosition", {
        .description = "Z Drive Position",
        .mmLabel = "TIZDrive",
        .mmProperty = "", // This property requires GetPosition(), and is not accessable from GetProperty().
        .readonly = false,
        .mmValueConverter = nullptr,
    }},
    {"ZDriveSpeed", {
        .description = "Z Drive Speed. Options: [1, 2, 3, ..., 9]. Default: 1",
        .mmLabel = "TIZDrive",
        .mmProperty = "Speed",
        .readonly = false,
        .mmValueConverter = nullptr,
    }},
    {"ZDriveTolerance", {
        .description = "Z Drive Tolerance. Options: [0, 1, 2, ..., 9]. Default: 0",
        .mmLabel = "TIZDrive",
        .mmProperty = "Tolerance",
        .readonly = false,
        .mmValueConverter = nullptr,
    }},
    {"PFSOffset", {
        .description = "PFS Offset. Range: [0.0, 1000.0]",
        .mmLabel = "TIPFSOffset",
        .mmProperty = "Position",
        .readonly = false,
        .mmValueConverter = nullptr,
    }},
    {"PFSStatus", {
        .description = "PFS Status. Options: ['Out of focus search range', 'Focusing', 'Locked']. Read-only",
        .mmLabel = "TIPFSStatus",
        .mmProperty = "Status",
        .readonly = true,
        .mmValueConverter = nullptr,
    }},
    {"PFSState", {
        .description = "PFS State. Options: [On, Off]",
        .mmLabel = "TIPFSStatus",
        .mmProperty = "State",
        .readonly = false,
        .mmValueConverter = nullptr,
    }},
    {"PFSFullFocusTimeoutMs", {
        .description = "PFS Full Focus Timeout Ms. Range: unknown. Default: 5000",
        .mmLabel = "TIPFSStatus",
        .mmProperty = "FullFocusTimeoutMs",
        .readonly = false,
        .mmValueConverter = nullptr,
    }},
    {"PFSFullFocusWaitAfterLockMs", {
        .description = "PFS Full Focus Wait After Lock Ms. Range: unknown. Default: 0",
        .mmLabel = "TIPFSStatus",
        .mmProperty = "FullFocusWaitAfterLockMs",
        .readonly = false,
        .mmValueConverter = nullptr,
    }},
};
