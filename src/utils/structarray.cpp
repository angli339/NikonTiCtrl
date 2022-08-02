#include "structarray.h"

StructArray::StructArray(std::vector<std::string> names, Dtype dtype,
                         size_t size)
{
    std::vector<StructArrayFieldDef> dtype_fields;
    for (const auto &name : names) {
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
    for (const auto &field : dtype) {
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
            data[field.name] =
                xt::xarray<uint16_t>(xt::zeros<uint16_t>({size}));
            offset += 2;
            break;
        case Dtype::uint32:
            data[field.name] =
                xt::xarray<uint32_t>(xt::zeros<uint32_t>({size}));
            offset += 4;
            break;
        case Dtype::uint64:
            data[field.name] =
                xt::xarray<uint64_t>(xt::zeros<uint64_t>({size}));
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

std::string StructArray::ToBuf()
{
    size_t buf_size = ItemSize() * Size();
    uint8_t *buf = (uint8_t *)malloc(buf_size);
    for (const auto &field : Fields()) {
        size_t offset = field.offset;
        if (field.dtype == Dtype::float32) {
            xt::xarray<float> field_arr = Field<float>(field.name);
            for (int i = 0; i < Size(); i++) {
                *(float *)(buf + offset) = field_arr[i];
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::float64) {
            xt::xarray<double> field_arr = Field<double>(field.name);
            for (int i = 0; i < Size(); i++) {
                *(double *)(buf + offset) = field_arr[i];
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::uint8) {
            xt::xarray<uint8_t> field_arr = Field<uint8_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                *(uint8_t *)(buf + offset) = field_arr[i];
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::uint16) {
            xt::xarray<uint16_t> field_arr = Field<uint16_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                *(uint16_t *)(buf + offset) = field_arr[i];
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::uint32) {
            xt::xarray<uint32_t> field_arr = Field<uint32_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                *(uint32_t *)(buf + offset) = field_arr[i];
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::uint64) {
            xt::xarray<uint64_t> field_arr = Field<uint64_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                *(uint64_t *)(buf + offset) = field_arr[i];
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::int8) {
            xt::xarray<int8_t> field_arr = Field<int8_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                *(int8_t *)(buf + offset) = field_arr[i];
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::int16) {
            xt::xarray<int16_t> field_arr = Field<int16_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                *(int16_t *)(buf + offset) = field_arr[i];
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::int32) {
            xt::xarray<int32_t> field_arr = Field<int32_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                *(int32_t *)(buf + offset) = field_arr[i];
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::int64) {
            xt::xarray<int64_t> field_arr = Field<int64_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                *(int64_t *)(buf + offset) = field_arr[i];
                offset += ItemSize();
            }
        } else {
            throw std::invalid_argument("unknown type");
        }
    }

    return std::string((char *)buf, buf_size);
}

void StructArray::FromBuf(std::string buf_s)
{
    if (buf_s.size() != ItemSize() * Size()) {
        throw std::invalid_argument("unexpected buf size");
    }

    uint8_t *buf = (uint8_t *)&buf_s[0];
    for (const auto &field : Fields()) {
        size_t offset = field.offset;
        if (field.dtype == Dtype::float32) {
            xt::xarray<float> &field_arr = Field<float>(field.name);
            for (int i = 0; i < Size(); i++) {
                field_arr[i] = *(float *)(buf + offset);
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::float64) {
            xt::xarray<double> &field_arr = Field<double>(field.name);
            for (int i = 0; i < Size(); i++) {
                field_arr[i] = *(double *)(buf + offset);
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::uint8) {
            xt::xarray<uint8_t> &field_arr = Field<uint8_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                field_arr[i] = *(uint8_t *)(buf + offset);
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::uint16) {
            xt::xarray<uint16_t> &field_arr = Field<uint16_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                field_arr[i] = *(uint16_t *)(buf + offset);
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::uint32) {
            xt::xarray<uint32_t> &field_arr = Field<uint32_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                field_arr[i] = *(uint32_t *)(buf + offset);
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::uint64) {
            xt::xarray<uint64_t> &field_arr = Field<uint64_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                field_arr[i] = *(uint64_t *)(buf + offset);
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::int8) {
            xt::xarray<int8_t> &field_arr = Field<int8_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                field_arr[i] = *(int8_t *)(buf + offset);
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::int16) {
            xt::xarray<int16_t> &field_arr = Field<int16_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                field_arr[i] = *(int16_t *)(buf + offset);
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::int32) {
            xt::xarray<int32_t> &field_arr = Field<int32_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                field_arr[i] = *(int32_t *)(buf + offset);
                offset += ItemSize();
            }
        } else if (field.dtype == Dtype::int64) {
            xt::xarray<int64_t> &field_arr = Field<int64_t>(field.name);
            for (int i = 0; i < Size(); i++) {
                field_arr[i] = *(int64_t *)(buf + offset);
                offset += ItemSize();
            }
        } else {
            throw std::invalid_argument("unknown type");
        }
    }
}