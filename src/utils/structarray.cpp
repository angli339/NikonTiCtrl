#include "structarray.h"

StructArray::StructArray(std::vector<std::string> names, Dtype dtype, size_t size)
{
    std::vector<StructArrayFieldDef> dtype_fields;
    for (const auto & name : names) {
        dtype_fields.push_back({name, dtype});
    }
    init(dtype_fields, size);
}

StructArray::StructArray(std::vector<StructArrayFieldDef> dtype, size_t size)
{
    init(dtype, size);
}

void StructArray::init(std::vector<StructArrayFieldDef> dtype, size_t size)
{
    this->dtype = dtype;
    this->size = size;
    
    this->names.reserve(dtype.size());
    size_t offset = 0;
    for (const auto & field : dtype) {
        this->names.push_back(field.name);
        this->fields.push_back({field.name, field.dtype, offset});
        switch (field.dtype) {
        case Dtype::float32:
            data[field.name] = xt::xarray<float>(xt::zeros<float>({size}));
            offset += 4;
            break;
        case Dtype::float64:
            data[field.name] = xt::xarray<double>(xt::zeros<double>({size}));
            offset += 8;
            break;
        case Dtype::uint8:
            data[field.name] = xt::xarray<uint8_t>(xt::zeros<uint8_t>({size}));
            offset += 1;
            break;
        case Dtype::uint16:
            data[field.name] = xt::xarray<uint16_t>(xt::zeros<uint16_t>({size}));
            offset += 2;
            break;
        case Dtype::uint32:
            data[field.name] = xt::xarray<uint32_t>(xt::zeros<uint32_t>({size}));
            offset += 4;
            break;
        case Dtype::uint64:
            data[field.name] = xt::xarray<uint64_t>(xt::zeros<uint64_t>({size}));
            offset += 8;
            break;
        case Dtype::int8:
            data[field.name] = xt::xarray<int8_t>(xt::zeros<int8_t>({size}));
            offset += 1;
            break;
        case Dtype::int16:
            data[field.name] = xt::xarray<int16_t>(xt::zeros<int16_t>({size}));
            offset += 2;
            break;
        case Dtype::int32:
            data[field.name] = xt::xarray<int32_t>(xt::zeros<int32_t>({size}));
            offset += 4;
            break;
        case Dtype::int64:
            data[field.name] = xt::xarray<int64_t>(xt::zeros<int64_t>({size}));
            offset += 8;
            break;
        default:
            throw std::invalid_argument("unknown type");
        }
    }
    this->itemsize = offset;
}
