#include "hdf5file.h"

#include <fmt/format.h>

HDF5File::HDF5File(std::filesystem::path path)
{
    if (!std::filesystem::exists(path)) {
        file_id = H5Fcreate(path.string().c_str(), H5F_ACC_EXCL, H5P_DEFAULT,
                            H5P_DEFAULT);
        if (file_id == H5I_INVALID_HID) {
            throw std::runtime_error("H5Fcreate failed");
        }
    } else {
        file_id = H5Fopen(path.string().c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
        if (file_id == H5I_INVALID_HID) {
            throw std::runtime_error("H5Fopen failed");
        }
    }
}

HDF5File::~HDF5File() { H5Fclose(file_id); }

bool HDF5File::exists(std::string name)
{
    if (name.empty()) {
        throw std::invalid_argument("empty name");
    }

    // recursively determine the existance of parent path
    size_t pos = name.find_last_of("/");
    if (!((pos == std::string::npos) || (pos == 0))) {
        std::string parent_path = name.substr(0, pos);
        if (!exists(parent_path)) {
            return false;
        }
    }

    htri_t status = H5Lexists(file_id, name.c_str(), H5P_DEFAULT);
    if (status > 0) {
        return true;
    } else if (status == 0) {
        return false;
    } else {
        throw std::runtime_error(fmt::format("H5Lexists err={}", status));
    }
}

void HDF5File::remove(std::string name)
{
    herr_t status = H5Ldelete(file_id, name.c_str(), H5P_DEFAULT);
    if (status < 0) {
        throw std::runtime_error(fmt::format("H5Ldelete err={}", status));
    }
}

void HDF5File::write(std::string name, StructArray arr)
{
    // Overwrite if exists
    if (exists(name)) {
        remove(name);
    }

    // Prepare buffer
    size_t buf_size = arr.ItemSize() * arr.Size();
    uint8_t *buf = (uint8_t *)malloc(buf_size);

    // Create compound type and fill buffer with data
    hid_t type_id = H5Tcreate(H5T_COMPOUND, arr.ItemSize());
    for (const auto &field : arr.Fields()) {
        hid_t member_type_id;
        switch (field.dtype) {
        case Dtype::float32:
            member_type_id = H5T_NATIVE_FLOAT;
            for (int i = 0; i < arr.Size(); i++) {
                *(float *)(buf + i * arr.ItemSize() + field.offset) =
                    arr.Field<float>(field.name)[i];
            }
            break;
        case Dtype::float64:
            member_type_id = H5T_NATIVE_DOUBLE;
            for (int i = 0; i < arr.Size(); i++) {
                *(double *)(buf + i * arr.ItemSize() + field.offset) =
                    arr.Field<double>(field.name)[i];
            }
            break;
        case Dtype::uint8:
            member_type_id = H5T_NATIVE_UINT8;
            for (int i = 0; i < arr.Size(); i++) {
                *(uint8_t *)(buf + i * arr.ItemSize() + field.offset) =
                    arr.Field<uint8_t>(field.name)[i];
            }
            break;
        case Dtype::uint16:
            member_type_id = H5T_NATIVE_UINT16;
            for (int i = 0; i < arr.Size(); i++) {
                *(uint16_t *)(buf + i * arr.ItemSize() + field.offset) =
                    arr.Field<uint16_t>(field.name)[i];
            }
            break;
        case Dtype::uint32:
            member_type_id = H5T_NATIVE_UINT32;
            for (int i = 0; i < arr.Size(); i++) {
                *(uint32_t *)(buf + i * arr.ItemSize() + field.offset) =
                    arr.Field<uint32_t>(field.name)[i];
            }
            break;
        case Dtype::uint64:
            member_type_id = H5T_NATIVE_UINT64;
            for (int i = 0; i < arr.Size(); i++) {
                *(uint64_t *)(buf + i * arr.ItemSize() + field.offset) =
                    arr.Field<uint64_t>(field.name)[i];
            }
            break;
        case Dtype::int8:
            member_type_id = H5T_NATIVE_INT8;
            for (int i = 0; i < arr.Size(); i++) {
                *(int8_t *)(buf + i * arr.ItemSize() + field.offset) =
                    arr.Field<int8_t>(field.name)[i];
            }
            break;
        case Dtype::int16:
            member_type_id = H5T_NATIVE_INT16;
            for (int i = 0; i < arr.Size(); i++) {
                *(int16_t *)(buf + i * arr.ItemSize() + field.offset) =
                    arr.Field<int16_t>(field.name)[i];
            }
            break;
        case Dtype::int32:
            member_type_id = H5T_NATIVE_INT32;
            for (int i = 0; i < arr.Size(); i++) {
                *(int32_t *)(buf + i * arr.ItemSize() + field.offset) =
                    arr.Field<int32_t>(field.name)[i];
            }
            break;
        case Dtype::int64:
            member_type_id = H5T_NATIVE_INT64;
            for (int i = 0; i < arr.Size(); i++) {
                *(int64_t *)(buf + i * arr.ItemSize() + field.offset) =
                    arr.Field<int64_t>(field.name)[i];
            }
            break;
        }
        H5Tinsert(type_id, field.name.c_str(), field.offset, member_type_id);
    }

    const hsize_t dims[] = {arr.Size()};
    hid_t space_id = H5Screate_simple(1, dims, NULL);
    if (space_id == H5I_INVALID_HID) {
        free(buf);
        throw std::runtime_error("cannot create dataspace");
    }

    hid_t lcpl_id = H5Pcreate(H5P_LINK_CREATE);
    if (lcpl_id == H5I_INVALID_HID) {
        free(buf);
        H5Sclose(space_id);
        throw std::runtime_error("cannot create link property");
    }

    herr_t status;
    status = H5Pset_create_intermediate_group(lcpl_id, 1);
    if (status < 0) {
        free(buf);
        H5Pclose(lcpl_id);
        H5Sclose(space_id);
        throw std::runtime_error(
            fmt::format("cannot set link property, err={}", status));
    }

    hid_t ds_id = H5Dcreate2(file_id, name.c_str(), type_id, space_id, lcpl_id,
                             H5P_DEFAULT, H5P_DEFAULT);
    if (ds_id == H5I_INVALID_HID) {
        free(buf);
        H5Pclose(lcpl_id);
        H5Sclose(space_id);
        throw std::runtime_error("cannot create dataset");
    }

    status = H5Dwrite(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
    if (status < 0) {
        free(buf);
        H5Dclose(ds_id);
        H5Pclose(lcpl_id);
        H5Sclose(space_id);
        throw std::runtime_error(
            fmt::format("cannot write dataset, err={}", status));
    }

    free(buf);
    H5Dclose(ds_id);
    H5Pclose(lcpl_id);
    H5Sclose(space_id);
}

void HDF5File::flush()
{
    herr_t status = H5Fflush(file_id, H5F_SCOPE_LOCAL);
    if (status < 0) {
        throw std::runtime_error(fmt::format("H5Fflush err={}", status));
    }
}