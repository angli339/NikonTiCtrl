#ifndef DEVICE_PROPERTYPATH_H
#define DEVICE_PROPERTYPATH_H

#include <map>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
using json = nlohmann::ordered_json;

class PropertyPath {
public:
    PropertyPath() {}
    PropertyPath(std::string dev_name, std::string property_name)
        : dev_name_(dev_name), property_name_(property_name)
    {
    }
    PropertyPath(std::string path);
    PropertyPath(const char *path) : PropertyPath(std::string(path)) {}

    bool empty() const
    {
        return (!root) && (dev_name_.empty()) && (property_name_.empty());
    }

    bool IsRoot() const { return root; }
    bool IsDevice() const
    {
        return (!dev_name_.empty()) && (property_name_.empty());
    }
    std::string DeviceName() const { return dev_name_; }
    std::string PropertyName() const { return property_name_; }
    std::string ToString() const;

private:
    bool root = false;
    std::string dev_name_;
    std::string property_name_;
};

inline bool operator<(const PropertyPath &lhs, const PropertyPath &rhs)
{
    return lhs.ToString() < rhs.ToString();
}

template <> struct fmt::formatter<PropertyPath> : formatter<string_view> {
    template <typename FormatContext>
    auto format(PropertyPath p, FormatContext &ctx)
    {
        return formatter<string_view>::format(p.ToString(), ctx);
    }
};

typedef std::map<PropertyPath, std::string> PropertyValueMap;

void from_json(const nlohmann::json &j, PropertyValueMap &pv);

// Helper functions
std::vector<PropertyPath> PropertyPathList(PropertyValueMap path_value_map);

#endif
