#include "config.h"

Config config;

void loadConfig(std::filesystem::path filename)
{
    config.unet_grpc_addr = "10.119.89.80:8500";

    config.data_root = "C:/Users/Ang/Data";

    config.user = {
        .name = "Ang Li",
        .email = "angli01@g.harvard.edu",
    };

    config.presets = {
        {
            .name = "BF",
            .property_value =
                {
                    {"/PriorProScan/LumenShutter", "Off"},
                    {"/NikonTi/FilterBlock1", "2"},
                    {"/PriorProScan/FilterWheel3", "10"},
                },
            .shutter_property = "/NikonTi/DiaShutter",
            .default_exposure_ms = 25,
        },
        {
            .name = "RFP",
            .property_value =
                {
                    {"/NikonTi/DiaShutter", "Off"},
                    {"/NikonTi/FilterBlock1", "2"},
                    {"/PriorProScan/FilterWheel1", "2"},
                    {"/PriorProScan/FilterWheel3", "1"},
                },
            .shutter_property = "/PriorProScan/LumenShutter",
            .illumination_property = "/PriorProScan/LumenOutputIntensity",
            .default_exposure_ms = 40,
            .default_illumination_intensity = 25,
        },
        {
            .name = "BFP",
            .property_value =
                {
                    {"/NikonTi/DiaShutter", "Off"},
                    {"/NikonTi/FilterBlock1", "2"},
                    {"/PriorProScan/FilterWheel1", "3"},
                    {"/PriorProScan/FilterWheel3", "2"},
                },
            .shutter_property = "/PriorProScan/LumenShutter",
            .illumination_property = "/PriorProScan/LumenOutputIntensity",
            .default_exposure_ms = 200,
            .default_illumination_intensity = 25,
        },
        {
            .name = "YFP",
            .property_value =
                {
                    {"/NikonTi/DiaShutter", "Off"},
                    {"/NikonTi/FilterBlock1", "2"},
                    {"/PriorProScan/FilterWheel1", "4"},
                    {"/PriorProScan/FilterWheel3", "3"},
                },
            .shutter_property = "/PriorProScan/LumenShutter",
            .illumination_property = "/PriorProScan/LumenOutputIntensity",
            .default_exposure_ms = 50,
            .default_illumination_intensity = 40,
        },
        {
            .name = "CFP",
            .property_value =
                {
                    {"/NikonTi/DiaShutter", "Off"},
                    {"/NikonTi/FilterBlock1", "3"},
                    {"/PriorProScan/FilterWheel1", "1"},
                    {"/PriorProScan/FilterWheel3", "4"},
                },
            .shutter_property = "/PriorProScan/LumenShutter",
            .illumination_property = "/PriorProScan/LumenOutputIntensity",
            .default_exposure_ms = 100,
            .default_illumination_intensity = 25,
        },
        {
            .name = "GFP",
            .property_value =
                {
                    {"/NikonTi/DiaShutter", "Off"},
                    {"/NikonTi/FilterBlock1", "1"},
                    {"/PriorProScan/FilterWheel1", "4"},
                    {"/PriorProScan/FilterWheel3", "5"},
                },
            .shutter_property = "/PriorProScan/LumenShutter",
            .illumination_property = "/PriorProScan/LumenOutputIntensity",
            .default_exposure_ms = 50,
            .default_illumination_intensity = 25,
        },
    };
}
