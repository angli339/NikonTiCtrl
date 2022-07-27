#ifndef STRUCTARRAY_H
#define STRUCTARRAY_H

#include <string>
#include <vector>
#include <map>
#include <xtensor/xarray.hpp>
#include <variant>

enum class Dtype {
    float32,
    float64,
    uint8,
    uint16,
    uint32,
    uint64,
    int8,
    int16,
    int32,
    int64,
};

struct StructArrayFieldDef {
    std::string name;
    Dtype dtype;
};

struct StructArrayyField {
    std::string name;
    Dtype dtype;
    size_t offset;
};

class StructArray {
public:
    StructArray();
    StructArray(std::vector<std::string> names, Dtype dtype, size_t size);
    StructArray(std::vector<StructArrayFieldDef> dtype, size_t size);

    size_t Size() { return this->size; }
    size_t ItemSize() { return this->itemsize; }

    const std::vector<StructArrayFieldDef> &DataType() { return this->dtype; }
    const std::vector<StructArrayyField> &Fields() { return this->fields; }
    const std::vector<std::string> &Names() { return this-> names; }

    template <typename T>
    xt::xarray<T> &Field(std::string name);

private:
    void init(std::vector<StructArrayFieldDef> dtype, size_t size);
    
    std::vector<StructArrayFieldDef> dtype;
    size_t size = 0;
    
    std::vector<std::string> names;
    std::vector<StructArrayyField> fields;
    size_t itemsize = 0;

    std::map<std::string, std::variant<
        xt::xarray<float>,
        xt::xarray<double>,
        xt::xarray<uint8_t>,
        xt::xarray<uint16_t>,
        xt::xarray<uint32_t>,
        xt::xarray<uint64_t>,
        xt::xarray<int8_t>,
        xt::xarray<int16_t>,
        xt::xarray<int32_t>,
        xt::xarray<int64_t>
        >> data;
};

template <typename T>
xt::xarray<T> &StructArray::Field(std::string name)
{
    return std::get<xt::xarray<T>>(data[name]);
}

#endif
