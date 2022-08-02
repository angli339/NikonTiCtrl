#ifndef HDF5FILE_H
#define HDF5FILE_H

#include <filesystem>
#include <fmt/format.h>
#include <hdf5.h>
#include <string>

#include "utils/structarray.h"

class HDF5File {
public:
    HDF5File(std::filesystem::path path);
    ~HDF5File();

    bool exists(std::string name);
    void remove(std::string name);
    std::vector<std::string> list(std::string group_name);
    bool is_group(std::string name);

    template <typename T> xt::xarray<T> read(std::string name);

    template <typename T>
    void write(std::string name, xt::xarray<T> arr, bool compress = false);

    StructArray read(std::string name);
    void write(std::string name, StructArray arr);
    void flush();

public:
    hid_t file_id;
};

template <typename T>
void HDF5File::write(std::string name, xt::xarray<T> arr, bool compress)
{
    // Overwrite if exists
    if (exists(name)) {
        remove(name);
    }

    hid_t type_id;
    hid_t mem_type_id;
    if constexpr (std::is_same_v<T, float>) {
        type_id = H5T_IEEE_F32LE;
        mem_type_id = H5T_NATIVE_FLOAT;
    } else if constexpr (std::is_same_v<T, double>) {
        type_id = H5T_IEEE_F64LE;
        mem_type_id = H5T_NATIVE_DOUBLE;
    } else if constexpr (std::is_same_v<T, uint8_t>) {
        type_id = H5T_STD_U8LE;
        mem_type_id = H5T_NATIVE_UINT8;
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        type_id = H5T_STD_U16LE;
        mem_type_id = H5T_NATIVE_UINT16;
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        type_id = H5T_STD_U32LE;
        mem_type_id = H5T_NATIVE_UINT32;
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        type_id = H5T_STD_U64LE;
        mem_type_id = H5T_NATIVE_UINT64;
    } else if constexpr (std::is_same_v<T, int8_t>) {
        type_id = H5T_STD_I8LE;
        mem_type_id = H5T_NATIVE_INT8;
    } else if constexpr (std::is_same_v<T, int16_t>) {
        type_id = H5T_STD_I16LE;
        mem_type_id = H5T_NATIVE_INT16;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        type_id = H5T_STD_I32LE;
        mem_type_id = H5T_NATIVE_INT32;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        type_id = H5T_STD_I64LE;
        mem_type_id = H5T_NATIVE_INT64;
    } else {
        // workaround as static_assert needs to depend on T
        // https://stackoverflow.com/a/69921429
        static_assert(!sizeof(T *), "unknown type");
    }

    hid_t space_id =
        H5Screate_simple(arr.shape().size(), arr.shape().data(), NULL);
    if (space_id == H5I_INVALID_HID) {
        throw std::runtime_error("cannot create space");
    }

    hid_t lcpl_id = H5Pcreate(H5P_LINK_CREATE);
    if (lcpl_id == H5I_INVALID_HID) {
        H5Sclose(space_id);
        throw std::runtime_error("cannot create link property");
    }

    herr_t status;

    hid_t dcpl_id = H5Pcreate(H5P_DATASET_CREATE);
    if (compress) {
        hsize_t c_ndim;
        hsize_t c_dims[2];
        if (arr.shape().size() == 1) {
            c_ndim = 1;
            c_dims[0] = arr.shape(0);
        } else {
            c_ndim = 2;
            c_dims[0] = arr.shape(0);
            c_dims[1] = arr.shape(1);
        }
        status = H5Pset_chunk(dcpl_id, c_ndim, c_dims);
        if (status < 0) {
            H5Pclose(dcpl_id);
            H5Pclose(lcpl_id);
            H5Sclose(space_id);
            throw std::runtime_error(
                fmt::format("cannot set chunk, err={}", status));
        }

        status = H5Pset_deflate(dcpl_id, 4);
        if (status < 0) {
            H5Pclose(dcpl_id);
            H5Pclose(lcpl_id);
            H5Sclose(space_id);
            throw std::runtime_error(
                fmt::format("cannot set deflate, err={}", status));
        }
    }

    status = H5Pset_create_intermediate_group(lcpl_id, 1);
    if (status < 0) {
        H5Pclose(lcpl_id);
        H5Sclose(space_id);
        throw std::runtime_error(
            fmt::format("cannot set property, err={}", status));
    }

    hid_t ds_id = H5Dcreate2(file_id, name.c_str(), type_id, space_id, lcpl_id,
                             dcpl_id, H5P_DEFAULT);
    if (ds_id == H5I_INVALID_HID) {
        H5Pclose(lcpl_id);
        H5Sclose(space_id);
        throw std::runtime_error("cannot create dataset");
    }

    status =
        H5Dwrite(ds_id, mem_type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    if (status < 0) {
        H5Dclose(ds_id);
        H5Pclose(lcpl_id);
        H5Sclose(space_id);
        throw std::runtime_error(
            fmt::format("cannot write dataset, err={}", status));
    }

    H5Dclose(ds_id);
    H5Pclose(lcpl_id);
    H5Sclose(space_id);
}

template <typename T> xt::xarray<T> HDF5File::read(std::string name)
{
    hid_t ds_id = H5Dopen2(file_id, name.c_str(), H5P_DEFAULT);
    if (ds_id == H5I_INVALID_HID) {
        throw std::runtime_error("cannot read dataset");
    }

    hid_t space_id = H5Dget_space(ds_id);
    if (space_id == H5I_INVALID_HID) {
        throw std::runtime_error("cannot read data space");
    }

    int ndims = H5Sget_simple_extent_ndims(space_id);
    hsize_t *dims = (hsize_t *)malloc(ndims * sizeof(hsize_t));
    if (H5Sget_simple_extent_dims(space_id, dims, NULL) != ndims) {
        throw std::runtime_error("cannot read data shape");
    }

    std::vector<size_t> shape;
    shape.resize(ndims);
    for (int i = 0; i < ndims; i++) {
        shape[i] = dims[i];
    }
    xt::xarray<T> arr = xt::xarray<T>::from_shape(shape);

    herr_t status;
    hid_t type_id = H5Dget_type(ds_id);
    if (H5Tequal(type_id, H5T_NATIVE_FLOAT) && std::is_same_v<T, float>) {
        status =
            H5Dread(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    } else if (H5Tequal(type_id, H5T_NATIVE_DOUBLE) &&
               std::is_same_v<T, double>) {
        status =
            H5Dread(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    } else if (H5Tequal(type_id, H5T_NATIVE_UINT8) &&
               std::is_same_v<T, uint8_t>) {
        status =
            H5Dread(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    } else if (H5Tequal(type_id, H5T_NATIVE_UINT16) &&
               std::is_same_v<T, uint16_t>) {
        status =
            H5Dread(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    } else if (H5Tequal(type_id, H5T_NATIVE_UINT32) &&
               std::is_same_v<T, uint32_t>) {
        status =
            H5Dread(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    } else if (H5Tequal(type_id, H5T_NATIVE_UINT64) &&
               std::is_same_v<T, uint64_t>) {
        status =
            H5Dread(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    } else if (H5Tequal(type_id, H5T_NATIVE_INT8) && std::is_same_v<T, int8_t>)
    {
        status =
            H5Dread(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    } else if (H5Tequal(type_id, H5T_NATIVE_INT16) &&
               std::is_same_v<T, int16_t>) {
        status =
            H5Dread(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    } else if (H5Tequal(type_id, H5T_NATIVE_INT32) &&
               std::is_same_v<T, int32_t>) {
        status =
            H5Dread(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    } else if (H5Tequal(type_id, H5T_NATIVE_INT64) &&
               std::is_same_v<T, int64_t>) {
        status =
            H5Dread(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    } else {
        H5Dclose(ds_id);
        throw std::runtime_error("unexpected data type");
    }

    if (status < 0) {
        H5Dclose(ds_id);
        throw std::runtime_error(
            fmt::format("cannot read dataset, err={}", status));
    }

    H5Dclose(ds_id);
    return arr;
}

#endif
