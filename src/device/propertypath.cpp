#include "device/propertypath.h"

PropertyPath::PropertyPath(std::string path)
{
    // size = 0
    if (path.empty()) {
        return;
    }

    if (path.size() == 1) {
        // size = 1
        if (path == "/") {
            root = true;
            return;
        } else {
            property_name_ = path;
            return;
        }
    } else {
        // size > 1
        if (path[0] == '/') {
            std::size_t pos = path.find_first_of('/', 1);
            if (pos == std::string::npos) {
                // "/dev_name"
                dev_name_ = path.substr(1, pos - 1);
                return;
            } else {
                // "/dev_name/property_name"
                // "/dev_name/property_name/sub_property_name"
                dev_name_ = path.substr(1, pos - 1);
                property_name_ = path.substr(pos + 1, std::string::npos);
                return;
            }
        } else {
            // "property_name"
            // "property_name/sub_property_name"
            property_name_ = path;
            return;
        }
    }
}

std::string PropertyPath::ToString() const
{
    if (root) {
        return "/";
    } else {
        if (dev_name_.empty() && property_name_.empty()) {
            return "";
        }
        if (dev_name_.empty()) {
            return property_name_;
        }
        if (property_name_.empty()) {
            return "/" + dev_name_;
        }
        return "/" + dev_name_ + "/" + property_name_;
    }
}

std::vector<PropertyPath> PropertyPathList(PropertyValueMap path_value_map)
{
    std::vector<PropertyPath> path_list;
    for (const auto &[path, value] : path_value_map) {
        path_list.push_back(path);
    }
    return path_list;
}

void from_json(const nlohmann::json &j, PropertyValueMap &pv)
{
    std::map<std::string, std::string> m;
    j.get_to(m);

    for (const auto &[p, v] : m) {
        pv[p] = v;
    }
}