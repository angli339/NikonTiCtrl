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

HDF5File::~HDF5File()
{
    std::unique_lock<std::mutex> lk(io_mutex);
    H5Fclose(file_id);
}

bool HDF5File::exists(std::string name)
{
    std::unique_lock<std::mutex> lk(io_mutex);

    return exists_nolock(name);
}

bool HDF5File::exists_nolock(std::string name)
{
    if (name.empty()) {
        throw std::invalid_argument("empty name");
    }

    // recursively determine the existance of parent path
    size_t pos = name.find_last_of("/");
    if (!((pos == std::string::npos) || (pos == 0))) {
        std::string parent_path = name.substr(0, pos);
        if (!exists_nolock(parent_path)) {
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
    std::unique_lock<std::mutex> lk(io_mutex);

    herr_t status = H5Ldelete(file_id, name.c_str(), H5P_DEFAULT);
    if (status < 0) {
        throw std::runtime_error(fmt::format("H5Ldelete err={}", status));
    }
}

std::vector<std::string> HDF5File::list(std::string group_name)
{
    std::unique_lock<std::mutex> lk(io_mutex);

    hid_t group_id = H5Gopen2(file_id, group_name.c_str(), H5P_DEFAULT);
    if (group_id == H5I_INVALID_HID) {
        throw std::runtime_error("H5Gopen2 failed");
    }

    H5G_info_t ginfo;
    herr_t status = H5Gget_info(group_id, &ginfo);
    if (status < 0) {
        H5Gclose(group_id);
        throw std::runtime_error(fmt::format("H5Gget_info, err={}", status));
    }

    std::vector<std::string> obj_name_list;

    for (int i = 0; i < ginfo.nlinks; i++) {
        ssize_t size =
            H5Lget_name_by_idx(group_id, ".", H5_INDEX_NAME, H5_ITER_NATIVE, i,
                               NULL, 0, H5P_DEFAULT);
        if (size <= 0) {
            break;
        }
        char *name = (char *)malloc(size + 1);
        ssize_t n =
            H5Lget_name_by_idx(group_id, ".", H5_INDEX_NAME, H5_ITER_NATIVE, i,
                               name, size + 1, H5P_DEFAULT);
        if (n <= 0) {
            free(name);
            H5Gclose(group_id);
            throw std::exception("H5Lget_name_by_idx failed");
        }
        obj_name_list.push_back(std::string(name, size));
        free(name);
    }

    H5Gclose(group_id);
    return obj_name_list;
}

bool HDF5File::is_group(std::string name)
{
    std::unique_lock<std::mutex> lk(io_mutex);

    hid_t root_group = H5Gopen2(file_id, "/", H5P_DEFAULT);
    if (root_group == H5I_INVALID_HID) {
        throw std::runtime_error("cannot open root_group");
    }
    H5O_info1_t info;
    herr_t status = H5Oget_info_by_name2(root_group, name.c_str(), &info,
                                         H5O_INFO_BASIC, H5P_DEFAULT);
    if (status < 0) {
        H5Gclose(root_group);
        throw std::runtime_error(
            fmt::format("H5Oget_info_by_name2, err={}", status));
    }
    H5Gclose(root_group);

    if (info.type == H5O_TYPE_GROUP) {
        return true;
    } else {
        return false;
    }
}

void HDF5File::write(std::string name, StructArray arr)
{
    // Overwrite if exists
    if (exists(name)) {
        remove(name);
    }

    std::unique_lock<std::mutex> lk(io_mutex);

    // Create compound type
    hid_t type_id = H5Tcreate(H5T_COMPOUND, arr.ItemSize());
    for (const auto &field : arr.Fields()) {
        hid_t member_type_id;
        switch (field.dtype) {
        case Dtype::float32:
            member_type_id = H5T_NATIVE_FLOAT;
            break;
        case Dtype::float64:
            member_type_id = H5T_NATIVE_DOUBLE;
            break;
        case Dtype::uint8:
            member_type_id = H5T_NATIVE_UINT8;
            break;
        case Dtype::uint16:
            member_type_id = H5T_NATIVE_UINT16;
            break;
        case Dtype::uint32:
            member_type_id = H5T_NATIVE_UINT32;
            break;
        case Dtype::uint64:
            member_type_id = H5T_NATIVE_UINT64;
            break;
        case Dtype::int8:
            member_type_id = H5T_NATIVE_INT8;
            break;
        case Dtype::int16:
            member_type_id = H5T_NATIVE_INT16;
            break;
        case Dtype::int32:
            member_type_id = H5T_NATIVE_INT32;
            break;
        case Dtype::int64:
            member_type_id = H5T_NATIVE_INT64;
            break;
        }
        H5Tinsert(type_id, field.name.c_str(), field.offset, member_type_id);
    }
    if (H5Tget_size(type_id) != arr.ItemSize()) {
        throw std::runtime_error("H5T size does not match itemsize");
    }

    const hsize_t dims[] = {arr.Size()};
    hid_t space_id = H5Screate_simple(1, dims, NULL);
    if (space_id == H5I_INVALID_HID) {
        throw std::runtime_error("cannot create dataspace");
    }

    hid_t lcpl_id = H5Pcreate(H5P_LINK_CREATE);
    if (lcpl_id == H5I_INVALID_HID) {
        H5Sclose(space_id);
        throw std::runtime_error("cannot create link property");
    }

    herr_t status;
    status = H5Pset_create_intermediate_group(lcpl_id, 1);
    if (status < 0) {
        H5Pclose(lcpl_id);
        H5Sclose(space_id);
        throw std::runtime_error(
            fmt::format("cannot set link property, err={}", status));
    }

    hid_t ds_id = H5Dcreate2(file_id, name.c_str(), type_id, space_id, lcpl_id,
                             H5P_DEFAULT, H5P_DEFAULT);
    if (ds_id == H5I_INVALID_HID) {
        H5Pclose(lcpl_id);
        H5Sclose(space_id);
        throw std::runtime_error("cannot create dataset");
    }

    std::string buf = arr.ToBuf();
    status = H5Dwrite(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, &buf[0]);
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

void HDF5File::flush()
{
    std::unique_lock<std::mutex> lk(io_mutex);

    herr_t status = H5Fflush(file_id, H5F_SCOPE_LOCAL);
    if (status < 0) {
        throw std::runtime_error(fmt::format("H5Fflush err={}", status));
    }
}

StructArray HDF5File::read(std::string name)
{
    std::unique_lock<std::mutex> lk(io_mutex);

    hid_t ds_id = H5Dopen2(file_id, name.c_str(), H5P_DEFAULT);
    if (ds_id == H5I_INVALID_HID) {
        throw std::runtime_error("cannot read dataset");
    }

    hid_t space_id = H5Dget_space(ds_id);
    if (space_id == H5I_INVALID_HID) {
        throw std::runtime_error("cannot read data space");
    }

    int ndims = H5Sget_simple_extent_ndims(space_id);
    if (ndims != 1) {
        throw std::runtime_error("unexpected shape");
    }
    hsize_t length;
    if (H5Sget_simple_extent_dims(space_id, &length, NULL) != ndims) {
        throw std::runtime_error("cannot read data length");
    }

    hid_t type_id = H5Dget_type(ds_id);
    size_t type_size = H5Tget_size(type_id);

    // read data into buffer
    size_t buf_size = length * type_size;
    uint8_t *buf = (uint8_t *)malloc(buf_size);
    herr_t status = H5Dread(ds_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
    if (status < 0) {
        H5Dclose(ds_id);
        throw std::runtime_error("cannot read data");
    }
    H5Dclose(ds_id);

    // find out fields
    int n_fields = H5Tget_nmembers(type_id);
    std::vector<StructArrayFieldDef> fields;
    for (int i = 0; i < n_fields; i++) {
        std::string name = std::string(H5Tget_member_name(type_id, i));
        hid_t field_type_id = H5Tget_member_type(type_id, i);
        if (H5Tequal(field_type_id, H5T_NATIVE_FLOAT)) {
            fields.push_back({name, Dtype::float32});
        } else if (H5Tequal(field_type_id, H5T_NATIVE_DOUBLE)) {
            fields.push_back({name, Dtype::float64});
        } else if (H5Tequal(field_type_id, H5T_NATIVE_UINT8)) {
            fields.push_back({name, Dtype::uint8});
        } else if (H5Tequal(field_type_id, H5T_NATIVE_UINT16)) {
            fields.push_back({name, Dtype::uint16});
        } else if (H5Tequal(field_type_id, H5T_NATIVE_UINT32)) {
            fields.push_back({name, Dtype::uint32});
        } else if (H5Tequal(field_type_id, H5T_NATIVE_UINT64)) {
            fields.push_back({name, Dtype::uint64});
        } else if (H5Tequal(field_type_id, H5T_NATIVE_INT8)) {
            fields.push_back({name, Dtype::int8});
        } else if (H5Tequal(field_type_id, H5T_NATIVE_INT16)) {
            fields.push_back({name, Dtype::int16});
        } else if (H5Tequal(field_type_id, H5T_NATIVE_INT32)) {
            fields.push_back({name, Dtype::int32});
        } else if (H5Tequal(field_type_id, H5T_NATIVE_INT64)) {
            fields.push_back({name, Dtype::int64});
        } else {
            throw std::runtime_error("unexpected data type");
        }
    }
    StructArray arr(fields, length);
    if (arr.ItemSize() != type_size) {
        throw std::runtime_error(
            "size of array item does not match H5T type size");
    }
    arr.FromBuf(std::string((char *)buf, buf_size));

    return arr;
}