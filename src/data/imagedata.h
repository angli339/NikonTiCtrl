#ifndef IMAGEDATA_H
#define IMAGEDATA_H

#include <cstdint>
#include <memory>
#include <vector>

#include <opencv2/core/mat.hpp>

enum class DataType
{
    Unknown,
    Bool8,
    Uint8,
    Uint16,
    Int16,
    Int32,
    Float32,
    Float64
};

enum class ColorType
{
    Unknown,
    Mono8,
    Mono10,
    Mono12,
    Mono14,
    Mono16,
    BayerRG8,
    BayerRG16,
};

class ImageData {
public:
    ImageData() {}
    ImageData(uint32_t height, uint32_t width, DataType dtype, ColorType ctype);

    bool empty()
    {
        return buf == nullptr;
    }
    uint32_t size()
    {
        return height * width;
    }

    DataType DataType()
    {
        return dtype;
    }
    ColorType ColorType()
    {
        return ctype;
    }
    uint8_t ElemSize();

    std::pair<uint32_t, uint32_t> Shape()
    {
        return {height, width};
    }
    uint32_t Height()
    {
        return height;
    }
    uint32_t Width()
    {
        return width;
    }
    size_t BufSize()
    {
        return buf_size;
    }
    std::shared_ptr<void> Buf()
    {
        return buf;
    }
    void CopyFrom(const std::string &val);
    void CopyFrom(cv::Mat mat);
    void CopyFrom(const float *val, const size_t n_val);

    cv::Mat AsMat();
    ImageData AsFloat32();

private:
    uint32_t height;
    uint32_t width;
    ::DataType dtype;
    ::ColorType ctype;

    size_t buf_size;
    std::shared_ptr<void> buf;
};

#endif
