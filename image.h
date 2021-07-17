#ifndef IMAGE_H
#define IMAGE_H

#include <cstdint>
#include <ghc/filesystem.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace fs = ghc::filesystem;

enum PixelFormat {
    PixelMono8,
    PixelMono12,
    PixelMono16,

    PixelBayerRG8,
    PixelBayerRG16,

    PixelRGB8,
    PixelRGB16,
};

struct Image {
    // Data
    PixelFormat pixelFormat;
    uint16_t width;
    uint16_t height;
    uint32_t bufSize;
    uint8_t *buf;
    std::string path;

    // Metadata
    std::string timestamp;
    std::string id;
    std::string name;
    std::string description;

    std::string channel;
    double excitationIntensity;
    double exposure_ms;
    double blackLevel;
    double ampConvertionFactor;
    
    double pixelSize_um;
    double positionX_um;
    double positionY_um;
    double positionZ_um;

    std::string user_full_name;
    std::string user_email;

    ~Image();
    std::map<std::string, std::string> deviceProperty;
    Image *copy();

    // uint8_t *getBuf();
    // I/O
    std::string encodeMetadata();
    void saveFile(fs::path image_dir);
    
    // Image analysis
    std::vector<double> getHistogram();
};

#endif // IMAGE_H
