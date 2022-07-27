#ifndef HDF5FILE_H
#define HDF5FILE_H

#include <string>
#include <hdf5.h>
#include <filesystem>
#include <fmt/format.h>

#include "utils/structarray.h"

class HDF5File {
public:
    HDF5File(std::filesystem::path path);
    ~HDF5File();

    bool exists(std::string name);
    void remove(std::string name);


    template <typename T>
    void write(std::string name, xt::xarray<T> arr, bool compress=false);

    void write(std::string name, StructArray arr);
    void flush();

public:
    hid_t file_id;
};

template <typename T>
void HDF5File::write(std::string name, xt::xarray<T> arr, bool compress) {
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
        static_assert(!sizeof(T*), "unknown type");
    }

    hid_t space_id = H5Screate_simple(arr.shape().size(), arr.shape().data(), NULL);
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
            throw std::runtime_error(fmt::format("cannot set chunk, err={}", status));
        }

        status = H5Pset_deflate(dcpl_id, 4);
        if (status < 0) {
            H5Pclose(dcpl_id);
            H5Pclose(lcpl_id);
            H5Sclose(space_id);
            throw std::runtime_error(fmt::format("cannot set deflate, err={}", status));
        }
    }

    status = H5Pset_create_intermediate_group(lcpl_id, 1);
    if (status < 0) {
        H5Pclose(lcpl_id);
        H5Sclose(space_id);
        throw std::runtime_error(fmt::format("cannot set property, err={}", status));
    }

    hid_t ds_id = H5Dcreate2(file_id, name.c_str(), type_id, space_id, lcpl_id, dcpl_id, H5P_DEFAULT);
    if (ds_id == H5I_INVALID_HID) {
        H5Pclose(lcpl_id);
        H5Sclose(space_id);
        throw std::runtime_error("cannot create dataset");
    }
    
    status = H5Dwrite(ds_id, mem_type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
    if (status < 0) {
        H5Dclose(ds_id);
        H5Pclose(lcpl_id);
        H5Sclose(space_id);
        throw std::runtime_error(fmt::format("cannot write dataset, err={}", status));
    }

    H5Dclose(ds_id);
    H5Pclose(lcpl_id);
    H5Sclose(space_id);
}

#endif
