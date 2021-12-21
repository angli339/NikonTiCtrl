#include "device/device.h"

#include <fmt/format.h>

std::string Device::PropertyDescription(std::string property)
{
    PropertyNode *node = Node(property);
    if (node == nullptr) {
        throw std::invalid_argument("property not found");
    }
    return node->Description();
}

bool Device::HasProperty(std::string property)
{
    PropertyNode *node = Node(property);
    if (node == nullptr) {
        return false;
    }
    return node->Valid();
}

std::vector<std::string> Device::ListProperty()
{
    std::vector<std::string> property_list;
    for (auto &[name, node] : NodeMap()) {
        if (node->Valid()) {
            property_list.push_back(name);
        }
    }
    return property_list;
}

std::map<std::string, std::string> Device::GetPropertySnapshot()
{
    std::map<std::string, std::string> property_value_map;
    for (const auto &[name, node] : NodeMap()) {
        auto snapshot = node->GetSnapshot();
        if (snapshot.has_value()) {
            property_value_map[name] = snapshot.value();
        }
    }
    return property_value_map;
}

StatusOr<std::string> Device::GetProperty(std::string property)
{
    if (!IsConnected()) {
        return absl::FailedPreconditionError("device not connected");
    }

    PropertyNode *node = Node(property);
    if ((node == nullptr) || !node->Valid()) {
        return absl::NotFoundError(fmt::format("property {} not found", property));
    }
    return node->GetValue();
}

Status Device::SetProperty(std::string property, std::string value)
{
    if (!IsConnected()) {
        return absl::FailedPreconditionError("device not connected");
    }

    PropertyNode *node = Node(property);
    if ((node == nullptr) || !node->Valid()) {
        return absl::NotFoundError(fmt::format("property {} not found", property));
    }
    return node->SetValue(value);
}

Status Device::SetProperty(std::map<std::string, std::string> property_value_map)
{
    for (const auto &[property, value] : property_value_map) {
        if (!HasProperty(property)) {
            return absl::NotFoundError(fmt::format("property {} not found", property));
        }
    }
    for (const auto &[property, value] : property_value_map) {
        PropertyNode *node = Node(property);
        if (node == nullptr) {
            return absl::NotFoundError(fmt::format("property {} not found", property));
        }
        Status status = node->SetValue(value);
        if (!status.ok()) {
            return absl::AbortedError(fmt::format("set property {}: {}", property, status.ToString()));
        }
    }
    return absl::OkStatus();
}

Status Device::WaitPropertyFor(std::vector<std::string> property_list, std::chrono::milliseconds timeout)
{
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + timeout;
    return WaitPropertyUntil(property_list, deadline);
}

Status Device::WaitPropertyUntil(std::vector<std::string> property_list, std::chrono::steady_clock::time_point deadline)
{
    for (const auto &property : property_list) {
        if (!HasProperty(property)) {
            return absl::NotFoundError(fmt::format("property {} not found", property));
        }
    }
    for (const auto &property : property_list) {
        PropertyNode *node = Node(property);
        if (node == nullptr) {
            return absl::NotFoundError(fmt::format("property {} not found", property));
        }
        Status status = node->WaitUntil(deadline);
        if (!status.ok()) {
            if (absl::IsDeadlineExceeded(status)) {
                return status;
            }
            return absl::AbortedError(fmt::format("wait property {}: {}", property, status.ToString()));
        }
    }
    return absl::OkStatus();
}
