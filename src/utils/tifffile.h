#ifndef TIFFFILE_H
#define TIFFFILE_H

#include <optional>
#include <string>
#include <tiffio.h>
#include <xtensor/xarray.hpp>

class TiffDecoder {
public:
    TiffDecoder(std::string buf);
    ~TiffDecoder();

    uint32_t Width();
    uint32_t Height();
    uint16_t BitsPerSample();
    uint16_t SamplesPerPixel();
    std::optional<uint16_t> SampleFormat();

    xt::xarray<uint16_t> ReadMono16();

private:
    std::istringstream stream;
    TIFF *tif = nullptr;
};

class TiffEncoder {
public:
    void SetCompression(uint16_t compression);
    void SetDescription(std::string description);
    void SetArtist(std::string artist);
    void SetSoftware(std::string software);
    void SetCameraMake(std::string make);
    void SetCameraModel(std::string model);
    void SetCameraSerialNumber(std::string sn);
    void SetPixelSize(double x_um, double y_um);
    void SetExposureTime(double exposure_ms);

    std::string EncodeMono16(xt::xarray<uint16_t> data);

private:
    uint16_t compression = COMPRESSION_NONE;
    std::string description;
    std::string artist;
    std::string camera_make;
    std::string camera_model;
    std::string camera_sn;
    std::optional<std::tuple<double, double>> pixel_size_um;
    std::optional<double> exposure_ms;

    std::string software;
    uint8_t exifVersion[4] = {'0', '2', '2', '1'};
};

#endif
