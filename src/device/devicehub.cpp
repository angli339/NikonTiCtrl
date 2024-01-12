#include "device/devicehub.h"

#include <fmt/format.h>

#include "logging.h"

DeviceHub::~DeviceHub()
{
    if (ListConnectedDevice().size() > 0) {
        DisconnectAll();
    }
    for (auto &[dev_name, dev] : dev_map) {
        delete dev;
    }
}

void DeviceHub::AddDevice(std::string dev_name, Device *dev)
{
    if (dev_name == "") {
        throw std::invalid_argument("dev_name cannot be null");
    }
    if (dev == nullptr) {
        throw std::invalid_argument("dev cannot be null");
    }

    dev_map[dev_name] = dev;

    if (!event_subscriber_list.empty()) {
        for (const auto &channel : event_subscriber_list) {
            dev->SubscribeEvents(channel, [dev_name](Event &e) {
                e.device = dev_name;
                e.path = PropertyPath(dev_name, e.path.PropertyName());
            });
        }
    }
}

Device *DeviceHub::GetDevice(std::string dev_name)
{
    auto it = dev_map.find(dev_name);
    if (it == dev_map.end()) {
        return nullptr;
    }
    return it->second;
}

std::string DeviceHub::GetDeviceName(Device *dev)
{
    for (const auto &[name, dev_known] : dev_map) {
        if (dev == dev_known) {
            return name;
        }
    }
    return "";
}

void DeviceHub::AddCamera(std::string dev_name, Hamamatsu::DCam *dcam)
{
    AddDevice(dev_name, dcam);
    hamamatsu_dcam = dcam;
}

Hamamatsu::DCam *DeviceHub::GetHamamatsuDCam() { return hamamatsu_dcam; }

Status DeviceHub::ConnectAll()
{
    // Concurrently connect all devices
    std::map<std::string, std::future<Status>> task_future_map;
    for (const auto &[dev_name, dev] : dev_map) {
        if (dev->IsConnected()) {
            continue;
        }
        task_future_map[dev_name] =
            std::async(std::launch::async, &DeviceHub::runDeviceConnect, this,
                       dev_name, dev);
    }
    return mergeDeviceTaskStatus(task_future_map);
}

Status DeviceHub::DisconnectAll()
{
    // Concurrently disconnect all devices
    std::map<std::string, std::future<Status>> task_future_map;
    for (const auto &[dev_name, dev] : dev_map) {
        if (!dev->IsConnected()) {
            continue;
        }
        task_future_map[dev_name] =
            std::async(std::launch::async, &DeviceHub::runDeviceDisconnect,
                       this, dev_name, dev);
    }
    return mergeDeviceTaskStatus(task_future_map);
}

Status DeviceHub::runDeviceConnect(std::string dev_name, Device *dev)
{
    LOG_INFO("Connecting device {}...", dev_name);
    utils::StopWatch sw;
    Status status = dev->Connect();
    if (status.ok()) {
        LOG_INFO("Device {} connected [{:.0f} ms]", dev_name,
                 sw.Milliseconds());
    } else {
        LOG_ERROR("Error connecting device {}: {}", dev_name,
                  status.ToString());
    }
    return status;
}

Status DeviceHub::runDeviceDisconnect(std::string dev_name, Device *dev)
{
    LOG_INFO("Disconnecting device {}...", dev_name);
    utils::StopWatch sw;
    Status status = dev->Disconnect();
    if (status.ok()) {
        LOG_INFO("Device {} disconnected [{:.0f} ms]", dev_name,
                 sw.Milliseconds());
    } else {
        LOG_ERROR("Error disconnecting device {}: {}", dev_name,
                  status.ToString());
    }
    return status;
}

std::set<std::string> DeviceHub::ListDevice()
{
    std::set<std::string> dev_list;
    for (const auto &[dev_name, dev] : dev_map) {
        dev_list.insert(dev_name);
    }
    return dev_list;
}

std::set<std::string> DeviceHub::ListConnectedDevice()
{
    std::set<std::string> dev_list;
    for (const auto &[dev_name, dev] : dev_map) {
        if (dev->IsConnected()) {
            dev_list.insert(dev_name);
        }
    }
    return dev_list;
}

std::vector<PropertyPath> DeviceHub::ListProperty(PropertyPath path)
{
    std::vector<PropertyPath> resp_list;

    // Empty path
    if (path.empty()) {
        return {};
    }

    // Root
    if (path.IsRoot()) {
        for (const auto &[dev_name, dev] : dev_map) {
            resp_list.push_back(PropertyPath(dev_name, ""));
        }
        return resp_list;
    }

    // Device
    std::string dev_name = path.DeviceName();
    if (dev_name.empty()) {
        return {};
    }
    Device *dev = GetDevice(dev_name);
    if (dev == nullptr) {
        return {};
    }
    for (const auto &property_name : dev->ListProperty()) {
        resp_list.push_back(PropertyPath(dev_name, property_name));
    }
    return resp_list;
}

StatusOr<std::string> DeviceHub::GetProperty(PropertyPath path)
{
    Device *dev = GetDevice(path.DeviceName());
    if (dev == nullptr) {
        return absl::NotFoundError(
            fmt::format("device {} not found", path.DeviceName()));
    }
    return dev->GetProperty(path.PropertyName());
}

Status DeviceHub::SetProperty(PropertyPath path, std::string value)
{
    Device *dev = GetDevice(path.DeviceName());
    if (dev == nullptr) {
        if (!path.DeviceName().empty()) {
            return absl::NotFoundError(
                fmt::format("device \"{}\" not found", path.DeviceName()));
        } else {
            return absl::NotFoundError(
                fmt::format("path \"{}\" not found", path.ToString()));
        }
    }
    return dev->SetProperty(path.PropertyName(), value);
}

Status DeviceHub::SetProperty(PropertyValueMap path_value_map)
{
    // Check all paths for existence and sort them by device
    std::map<std::string, std::map<std::string, std::string>>
        property_value_map_by_dev;
    for (const auto &[path, value] : path_value_map) {
        std::string dev_name = path.DeviceName();
        Device *dev = GetDevice(dev_name);
        if (dev == nullptr) {
            if (!dev_name.empty()) {
                return absl::NotFoundError(
                    fmt::format("device \"{}\" not found", dev_name));
            } else {
                return absl::NotFoundError(
                    fmt::format("path \"{}\" not found", path.ToString()));
            }
        }
        if (!dev->HasProperty(path.PropertyName())) {
            return absl::NotFoundError(
                fmt::format("property \"{}\" not found", path.ToString()));
        }
        property_value_map_by_dev[dev_name][path.PropertyName()] = value;
    }

    // Concurrently set properties
    std::map<std::string, std::future<Status>> task_future_map;
    for (auto &[dev_name, property_value_map] : property_value_map_by_dev) {
        Device *dev = GetDevice(dev_name);
        task_future_map[dev_name] = std::async(
            std::launch::async,
            static_cast<Status (Device::*)(std::map<std::string, std::string>)>(
                &Device::SetProperty),
            dev, property_value_map);
    }
    return mergeDeviceTaskStatus(task_future_map);
}

Status DeviceHub::WaitPropertyFor(std::vector<PropertyPath> path_list,
                                  std::chrono::milliseconds timeout)
{
    return WaitPropertyUntil(path_list,
                             std::chrono::steady_clock::now() + timeout);
}

Status
DeviceHub::WaitPropertyUntil(std::vector<PropertyPath> path_list,
                             std::chrono::steady_clock::time_point timepoint)
{
    // Check all paths for existence and sort them by device
    std::map<Device *, std::vector<std::string>> property_list_by_dev;
    for (const auto &path : path_list) {
        Device *dev = GetDevice(path.DeviceName());
        if (dev == nullptr) {
            if (!path.DeviceName().empty()) {
                return absl::NotFoundError(
                    fmt::format("device \"{}\" not found", path.DeviceName()));
            } else {
                return absl::NotFoundError(
                    fmt::format("path \"{}\" not found", path.ToString()));
            }
        }
        if (!dev->HasProperty(path.PropertyName())) {
            return absl::NotFoundError(
                fmt::format("property \"{}\" not found", path.ToString()));
        }
        property_list_by_dev[dev].push_back(path.PropertyName());
    }

    for (const auto &[dev, property_list] : property_list_by_dev) {
        Status status = dev->WaitPropertyUntil(property_list, timepoint);
        if (!status.ok()) {
            return status;
        }
    }
    return absl::OkStatus();
}

PropertyValueMap DeviceHub::GetPropertySnapshot()
{
    PropertyValueMap path_value_map;

    for (const auto &[dev_name, dev] : dev_map) {
        if (!dev->IsConnected()) {
            continue;
        }
        for (const auto &[prop_name, value] : dev->GetPropertySnapshot()) {
            path_value_map[PropertyPath(dev_name, prop_name)] = value;
        }
    }
    return path_value_map;
}

PropertyValueMap
DeviceHub::GetPropertySnapshot(std::set<std::string> dev_name_set)
{
    PropertyValueMap path_value_map;

    for (const auto &[dev_name, dev] : dev_map) {
        if (!dev_name_set.contains(dev_name)) {
            continue;
        }
        if (!dev->IsConnected()) {
            continue;
        }
        for (const auto &[prop_name, value] : dev->GetPropertySnapshot()) {
            path_value_map[PropertyPath(dev_name, prop_name)] = value;
        }
    }
    return path_value_map;
}

void DeviceHub::SubscribeEvents(EventStream *channel)
{
    for (const auto &[dev_name, dev] : dev_map) {
        dev->SubscribeEvents(channel, [dev_name](Event &e) {
            e.device = dev_name;
            e.path = PropertyPath(dev_name, e.path.PropertyName());
        });
    }
    event_subscriber_list.push_back(channel);
}

Status DeviceHub::mergeDeviceTaskStatus(
    std::map<std::string, std::future<Status>> &dev_task_future_map)
{
    // Wait and get status
    std::map<std::string, Status> error_map;
    for (auto &[dev_name, task_future] : dev_task_future_map) {
        Status status = task_future.get();
        if (!status.ok()) {
            error_map[dev_name] = status;
        }
    }

    // Return status
    if (error_map.empty()) {
        return absl::OkStatus();
    }
    if (error_map.size() == 1) {
        return error_map.begin()->second;
    }
    std::vector<std::string> error_msg_list;
    for (const auto &[dev_name, err] : error_map) {
        error_msg_list.push_back(
            fmt::format("{}({})", dev_name, err.ToString()));
    }
    std::string merged_error_msg =
        fmt::format("{} devices failed: {}", error_msg_list.size(),
                    fmt::join(error_msg_list, ", "));
    return absl::AbortedError(merged_error_msg);
}
