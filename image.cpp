#include "image.h"
#include "utils/string_utils.h"

// #include <filesystem>
#include <fmt/format.h>
#include <tiffio.h>
#include "logging.h"
#include "cmath"

// #include "opencv2/core.hpp"
// #include "opencv2/imgproc.hpp"

Image::~Image() {
    if (buf != NULL) {
        free(buf);
    }
}

Image *Image::copy() {
    Image *im_copy = new Image;
    im_copy->pixelFormat = pixelFormat;
    im_copy->width = width;
    im_copy->height = height;
    im_copy->timestamp = timestamp;
    im_copy->bufSize = bufSize;
    im_copy->buf = (uint8_t *)malloc(bufSize);
    memcpy_s(im_copy->buf, bufSize, buf, bufSize);
    return im_copy;
}

std::string Image::encodeMetadata() {
    nlohmann::json j = {
        {"timestamp", timestamp},
        {"id", id},
        {"name", name},
        {"description", description},

        {"channel", channel},
        {"exposure_ms", exposure_ms},

        {"pixelSize_um", pixelSize_um},
        {"positionX_um", positionX_um},
        {"positionY_um", positionY_um},
        {"positionZ_um", positionZ_um},
    };

    if (excitationIntensity != 0) {
        j["excitationIntensity"] = excitationIntensity;
    }

    j["deviceProperty"] = deviceProperty;

    return j.dump();
}

void Image::saveFile(std::string folder) {
    std::string path = fmt::format("{}/{}.tif", folder, name);

    TIFF* tif = TIFFOpen(path.c_str(), "w");

    if (tif == NULL) {
        SPDLOG_ERROR("failed to open {} for write", path);
    }

    log_io("Image", "saveFile", "",
        "TIFFOpen()", path, "",
        ""
    );

    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, 0);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    
    if (pixelFormat == PixelMono16) {
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        TIFFSetField(tif, TIFFTAG_MINSAMPLEVALUE, 0);
        TIFFSetField(tif, TIFFTAG_MAXSAMPLEVALUE, 65535);
    } else {
        throw std::runtime_error("pixelFormat not yet supported");
    }

    if (pixelSize_um != 0) {
        double pixel_per_cm = 10 * 1000 / pixelSize_um;
        TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
        TIFFSetField(tif, TIFFTAG_XRESOLUTION, pixel_per_cm);
        TIFFSetField(tif, TIFFTAG_YRESOLUTION, pixel_per_cm);
    }

    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, encodeMetadata().c_str());
    TIFFSetField(tif, TIFFTAG_SOFTWARE, "Springer Lab Nikon Ti Control");
    TIFFSetField(tif, TIFFTAG_MAKE, "Hamamatsu");
    TIFFSetField(tif, TIFFTAG_MODEL, "Hamamatsu ORCA-R2");
    TIFFSetField(tif, TIFFTAG_UNIQUECAMERAMODEL, "Hamamatsu ORCA-R2");
    // TIFFSetField(tif, TIFFTAG_CAMERASERIALNUMBER, "SN123456");
    if (user_full_name != "") {
        if (user_email != "") {
            TIFFSetField(tif, TIFFTAG_ARTIST, fmt::format("{} <{}>", user_full_name, user_email).c_str());
        } else {
            TIFFSetField(tif, TIFFTAG_ARTIST, user_full_name.c_str());
        }
    }

    // Placeholder for ExifIFD Offset
    uint64_t offsetExifIFD = 0;
    TIFFSetField(tif, TIFFTAG_EXIFIFD, offsetExifIFD);

    // Write Data and IFD
    TIFFWriteEncodedStrip(tif, 0, buf, bufSize);
    TIFFWriteDirectory(tif);

    // Write EXIF IFD
    TIFFCreateEXIFDirectory(tif);
    uint8_t exifVersion[4] = {'0','2','2','1'};
    TIFFSetField(tif, EXIFTAG_EXIFVERSION, exifVersion);
    TIFFSetField(tif, EXIFTAG_EXPOSURETIME, 0.050);
    TIFFWriteCustomDirectory(tif, &offsetExifIFD);

    // Fill in ExifIFD Offset
    TIFFSetDirectory(tif, 0);
    TIFFSetField(tif, TIFFTAG_EXIFIFD, offsetExifIFD);

    TIFFClose(tif);
    
    SPDLOG_INFO("Image: image saved to {}", path.c_str());
}

std::vector<double> Image::getHistogram()
{
    int n_hist = 256;
    int bin_size = 256;

    uint32_t hist_int[n_hist] = {0};
    
    uint16_t *buf16 = (uint16_t *)buf;
    for (int i = 0; i < width * height; i++) {
        hist_int[buf16[i]/bin_size] += 1;
    }

    uint32_t count_max = 0;
    for (int i = 0; i < n_hist; i++) {
        if (hist_int[i] > count_max) {
            count_max = hist_int[i];
        }
    }

    std::vector<double> hist;
    hist.reserve(n_hist);
    for (int i = 0; i < n_hist; i++) {
        hist.push_back((double)hist_int[i] / (double)count_max);
    }
    return hist;

    
    // //
    // // Calculate histogram and normalize to [0, 1] with OpenCV
    // //
    // cv::Mat im = cv::Mat(height, width, CV_16U, buf);

    // cv::Mat cvHist;
    // int histChannels[] = {0};
    // int histBins[] = {256};
    // float range[] = {0, 65536};
    // const float* histRange = {range};
    // cv::calcHist(&im, 1, histChannels, cv::Mat(), cvHist, 1, histBins, &histRange, true, false);
    // cv::normalize(cvHist, cvHist, 1.0, 0.0, cv::NORM_MINMAX);

    // std::vector<double> hist;
    // hist.reserve(histBins[0]);
    
    // for (int i = 0; i < histBins[0]; i++) {
    //     hist.push_back(cvHist.at<float>(i));
    // }
    return hist;
}