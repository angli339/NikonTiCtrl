#include "data/imagedata.h"

#include <stdexcept>

ImageData::ImageData(uint32_t height, uint32_t width, ::DataType dtype,
                     ::ColorType ctype)
{
    this->height = height;
    this->width = width;
    this->dtype = dtype;
    this->ctype = ctype;

    uint8_t elem_size = ElemSize();
    if (elem_size == 0) {
        throw std::invalid_argument("invalid pixel_format");
    }
    buf_size = height * width * elem_size;
    buf = std::make_shared<uint8_t[]>(buf_size);
}

uint8_t ImageData::ElemSize()
{
    switch (dtype) {
    case DataType::Bool8:
    case DataType::Uint8:
        return 1;
    case DataType::Uint16:
    case DataType::Int16:
        return 2;
    case DataType::Int32:
    case DataType::Float32:
        return 4;
    case DataType::Float64:
        return 8;
    default:
        return 0;
    }
}

void ImageData::CopyFrom(const std::string &val)
{
    if (val.size() != buf_size) {
        throw std::invalid_argument("buf size mismatch");
    }
    memcpy(buf.get(), val.data(), buf_size);
}

void ImageData::CopyFrom(const float *val, const size_t n_val)
{
    if (dtype != DataType::Float32) {
        throw std::invalid_argument("type mismatch");
    }
    if (height * width != n_val) {
        throw std::invalid_argument("soze mismatch");
    }
    memcpy(buf.get(), val, buf_size);
}

void ImageData::CopyFrom(cv::Mat mat)
{
    if (mat.elemSize() != ElemSize()) {
        throw std::invalid_argument("ElemSize mismatch");
    }
    if (mat.total() != size()) {
        throw std::invalid_argument("size mismatch");
    }
    memcpy(buf.get(), mat.data, buf_size);
}

ImageData ImageData::AsFloat32()
{
    if (dtype == DataType::Float32) {
        return *this;
    }

    ImageData im_out = ImageData(height, width, DataType::Float32, ctype);

    switch (dtype) {
    case DataType::Float64:
        for (int i = 0; i < size(); i++) {
            double *in = reinterpret_cast<double *>(buf.get());
            float *out = reinterpret_cast<float *>(im_out.Buf().get());
            out[i] = float(in[i]);
        }
        return im_out;
    case DataType::Uint8:
        for (int i = 0; i < size(); i++) {
            uint8_t *in = reinterpret_cast<uint8_t *>(buf.get());
            float *out = reinterpret_cast<float *>(im_out.Buf().get());
            out[i] = float(in[i]) / float(255);
        }
        return im_out;
    case DataType::Uint16:
        float vmax;
        switch (ctype) {
        case ColorType::Mono10:
            vmax = float((1 << 10) - 1);
            break;
        case ColorType::Mono12:
            vmax = float((1 << 12) - 1);
            break;
        case ColorType::Mono14:
            vmax = float((1 << 14) - 1);
            break;
        case ColorType::Mono16:
        case ColorType::BayerRG16:
        case ColorType::Unknown:
            vmax = float((1 << 16) - 1);
            break;
        default:
            throw std::invalid_argument("invalid ColorType");
        }

        for (int i = 0; i < size(); i++) {
            uint16_t *in = reinterpret_cast<uint16_t *>(buf.get());
            float *out = reinterpret_cast<float *>(im_out.Buf().get());
            out[i] = float(in[i]) / vmax;
        }
        return im_out;
    default:
        throw std::invalid_argument("data type not supported");
    }
}

cv::Mat ImageData::AsMat()
{
    switch (dtype) {
    case DataType::Bool8:
    case DataType::Uint8:
        return cv::Mat(height, width, CV_8U, buf.get());
    case DataType::Uint16:
        return cv::Mat(height, width, CV_16U, buf.get());
    case DataType::Int16:
        return cv::Mat(height, width, CV_16S, buf.get());
    case DataType::Int32:
        return cv::Mat(height, width, CV_32S, buf.get());
    case DataType::Float32:
        return cv::Mat(height, width, CV_32F, buf.get());
    case DataType::Float64:
        return cv::Mat(height, width, CV_64F, buf.get());
    default:
        return cv::Mat();
    }
}
