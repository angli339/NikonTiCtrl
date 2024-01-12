#include "tifffile.h"
#include "tiff_stream.h"

#include <fmt/format.h>
#include <stdexcept>

TiffDecoder::TiffDecoder(std::string buf)
{
    stream = std::istringstream(buf);

    tif = TIFFStreamOpenRead("istringstream", &stream);
    if (tif == NULL) {
        throw std::runtime_error("failed to open stream");
    }
}

TiffDecoder::~TiffDecoder()
{
    if (tif) {
        TIFFClose(tif);
        tif = nullptr;
    }
}

uint32_t TiffDecoder::Width()
{
    uint32_t value;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &value);
    return value;
}

uint32_t TiffDecoder::Height()
{
    uint32_t value;
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &value);
    return value;
}

uint16_t TiffDecoder::BitsPerSample()
{
    uint16_t value;
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &value);
    return value;
}

uint16_t TiffDecoder::SamplesPerPixel()
{
    uint16_t value;
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &value);
    return value;
}

std::optional<uint16_t> TiffDecoder::SampleFormat()
{
    std::optional<uint16_t> opt_value;
    uint16_t value;
    if (TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &value)) {
        opt_value = value;
    }
    return opt_value;
}

xt::xarray<uint16_t> TiffDecoder::ReadMono16()
{
    if (BitsPerSample() != 16) {
        throw std::runtime_error("not 16-bit");
    }
    if (SamplesPerPixel() != 1) {
        throw std::runtime_error("not 1 sample/pixel");
    }
    std::optional<uint16_t> sample_format = SampleFormat();
    if (sample_format.has_value() &&
        (sample_format.value() != SAMPLEFORMAT_UINT))
    {
        throw std::runtime_error("not uint");
    }

    uint32_t width = Width();
    uint32_t height = Height();

    uint32_t rows_per_strip;
    TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
    if (rows_per_strip != height) {
        throw std::runtime_error(
            "format not yet supported: expecting single strip");
    }

    uint16_t planar_config;
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planar_config);
    if (planar_config != PLANARCONFIG_CONTIG) {
        throw std::runtime_error(
            "format not supported: expecting PLANARCONFIG_CONTIG");
    }

    xt::xarray<uint16_t> data =
        xt::xarray<uint16_t>::from_shape({height, width});

    TIFFReadEncodedStrip(tif, 0, data.data(), data.size() * sizeof(uint16_t));

    return data;
}

void TiffEncoder::SetCompression(uint16_t compression)
{
    this->compression = compression;
}

void TiffEncoder::SetDescription(std::string description)
{
    this->description = description;
}

void TiffEncoder::SetArtist(std::string artist) { this->artist = artist; }

void TiffEncoder::SetSoftware(std::string software)
{
    this->software = software;
}

void TiffEncoder::SetCameraMake(std::string make) { this->camera_make = make; }

void TiffEncoder::SetCameraModel(std::string model)
{
    this->camera_model = model;
}

void TiffEncoder::SetCameraSerialNumber(std::string sn)
{
    this->camera_sn = sn;
}

void TiffEncoder::SetPixelSize(double x_um, double y_um)
{
    this->pixel_size_um = {x_um, y_um};
}

void TiffEncoder::SetExposureTime(double exposure_ms)
{
    this->exposure_ms = exposure_ms;
}

std::string TiffEncoder::EncodeMono16(xt::xarray<uint16_t> data)
{
    if (data.shape().size() != 2) {
        throw std::invalid_argument("expecting 2D array");
    }
    uint16_t image_height = data.shape(0);
    uint16_t image_width = data.shape(1);

    std::stringstream stream;

    TIFF *tif = TIFFStreamOpenWrite("stringstream", &stream);
    if (tif == NULL) {
        throw std::runtime_error("failed to open stream");
    }

    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, 0);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, image_width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, image_height);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, image_height);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, compression);

    // Mono16
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    TIFFSetField(tif, TIFFTAG_MINSAMPLEVALUE, 0);
    TIFFSetField(tif, TIFFTAG_MAXSAMPLEVALUE, 65535);

    if (pixel_size_um.has_value()) {
        double pixel_size_um_x = std::get<0>(pixel_size_um.value());
        double pixel_size_um_y = std::get<1>(pixel_size_um.value());

        double pixel_per_cm_x = 10 * 1000 / pixel_size_um_x;
        double pixel_per_cm_y = 10 * 1000 / pixel_size_um_y;

        TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
        TIFFSetField(tif, TIFFTAG_XRESOLUTION, pixel_per_cm_x);
        TIFFSetField(tif, TIFFTAG_YRESOLUTION, pixel_per_cm_y);
    }
    if (!description.empty()) {
        TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, description.c_str());
    }
    if (!artist.empty()) {
        TIFFSetField(tif, TIFFTAG_ARTIST, artist.c_str());
    }
    if (!software.empty()) {
        TIFFSetField(tif, TIFFTAG_SOFTWARE, software.c_str());
    }
    if (!camera_make.empty()) {
        TIFFSetField(tif, TIFFTAG_MAKE, camera_make.c_str());
    }
    if (!camera_model.empty()) {
        TIFFSetField(tif, TIFFTAG_MODEL, camera_model.c_str());
        TIFFSetField(tif, TIFFTAG_UNIQUECAMERAMODEL, camera_model.c_str());
    }
    if (!camera_sn.empty()) {
        TIFFSetField(tif, TIFFTAG_CAMERASERIALNUMBER, camera_sn.c_str());
    }

    // Placeholder for ExifIFD Offset
    uint64_t offsetExifIFD = 0;
    TIFFSetField(tif, TIFFTAG_EXIFIFD, offsetExifIFD);

    // Write Data
    TIFFWriteEncodedStrip(tif, 0, data.data(), data.size() * sizeof(uint16_t));

    // Write IFD
    TIFFWriteDirectory(tif);

    // Write EXIF IFD
    TIFFCreateEXIFDirectory(tif);
    TIFFSetField(tif, EXIFTAG_EXIFVERSION, exifVersion);
    if (exposure_ms.has_value()) {
        TIFFSetField(tif, EXIFTAG_EXPOSURETIME, exposure_ms.value() / 1000);
    }
    if (!camera_sn.empty()) {
        TIFFSetField(tif, EXIFTAG_BODYSERIALNUMBER, camera_sn.c_str());
    }
    TIFFWriteCustomDirectory(tif, &offsetExifIFD);

    // Go back and fill in ExifIFD Offset
    // This requires the stream to be readable
    TIFFSetDirectory(tif, 0);
    TIFFSetField(tif, TIFFTAG_EXIFIFD, offsetExifIFD);

    TIFFClose(tif);

    return stream.str();
}
