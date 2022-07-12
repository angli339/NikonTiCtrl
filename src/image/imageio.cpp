#include "image/imageio.h"

#include <stdexcept>

#include <fmt/format.h>
#include <tiffio.h>

#include "version.h"

void ImageWrite(std::filesystem::path filepath, ImageData data,
                TiffMetadata tiff_meta)
{
    std::string encoded_metadata = tiff_meta.metadata.dump();
    tiff_meta.software_version = fmt::format("NikonTiControl {}", gitTagVersion);

    TIFF *tif = TIFFOpen(filepath.string().c_str(), "w");
    if (tif == NULL) {
        // TODO get text descrption of the error
        throw std::runtime_error(fmt::format(
            "failed to open tiff for write \"{}\"", filepath.string()));
    }

    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, 0);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, data.Width());
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, data.Height());
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, data.Height());
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_ZSTD);

    if (data.ColorType() == ColorType::Mono16) {
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        TIFFSetField(tif, TIFFTAG_MINSAMPLEVALUE, 0);
        TIFFSetField(tif, TIFFTAG_MAXSAMPLEVALUE, 65535);
    } else {
        throw std::runtime_error(
            fmt::format("color type {} is not implemented", data.ColorType()));
    }

    if (tiff_meta.pixel_size_um.has_value()) {
        double pixel_per_cm = 10 * 1000 / tiff_meta.pixel_size_um.value();
        TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
        TIFFSetField(tif, TIFFTAG_XRESOLUTION, pixel_per_cm);
        TIFFSetField(tif, TIFFTAG_YRESOLUTION, pixel_per_cm);
    }

    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, encoded_metadata.c_str());

    TIFFSetField(tif, TIFFTAG_SOFTWARE, tiff_meta.software_version.c_str());

    if (tiff_meta.camera_make.has_value()) {
        TIFFSetField(tif, TIFFTAG_MAKE, tiff_meta.camera_make.value().c_str());
    }

    if (tiff_meta.camera_model.has_value()) {
        TIFFSetField(tif, TIFFTAG_MODEL,
                     tiff_meta.camera_model.value().c_str());
        TIFFSetField(tif, TIFFTAG_UNIQUECAMERAMODEL,
                     tiff_meta.camera_model.value().c_str());
    }

    // Placeholder for ExifIFD Offset
    uint64_t offsetExifIFD = 0;
    TIFFSetField(tif, TIFFTAG_EXIFIFD, offsetExifIFD);

    // Write Data and IFD
    TIFFWriteEncodedStrip(tif, 0, data.Buf().get(), data.BufSize());
    TIFFWriteDirectory(tif);

    // Write EXIF IFD
    TIFFCreateEXIFDirectory(tif);
    uint8_t exifVersion[4] = {'0', '2', '2', '1'};
    TIFFSetField(tif, EXIFTAG_EXIFVERSION, exifVersion);
    // TIFFSetField(tif, EXIFTAG_EXPOSURETIME, exposure_time_s);
    TIFFWriteCustomDirectory(tif, &offsetExifIFD);

    // Fill in ExifIFD Offset
    TIFFSetDirectory(tif, 0);
    TIFFSetField(tif, TIFFTAG_EXIFIFD, offsetExifIFD);

    TIFFClose(tif);
}