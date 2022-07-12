#ifndef IMAGE_H
#define IMAGE_H

#include <filesystem>
#include <optional>

#include <nlohmann/json.hpp>

#include "device/propertypath.h"
#include "image/imagedata.h"

struct TiffMetadata {
    nlohmann::ordered_json metadata;

    std::optional<double> pixel_size_um;

    std::optional<std::string> camera_make;
    std::optional<std::string> camera_model;
    std::optional<std::string> camera_sn;

    std::string software_version;
};

void ImageWrite(std::filesystem::path filepath, ImageData data,
                TiffMetadata tiff_meta);

#endif
