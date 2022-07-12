#ifndef DEVICEHUB_H
#define DEVICEHUB_H

#include <future>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "device/device.h"
#include "device/propertypath.h"
#include "eventstream.h"

#include "device/hamamatsu/hamamatsu_dcam.h"

class DeviceHub {
public:
    ~DeviceHub();

    void AddDevice(std::string dev_name, Device *dev);
    Device *GetDevice(std::string dev_name);
    std::string GetDeviceName(Device *dev);

    void AddCamera(std::string dev_name, Hamamatsu::DCam *dcam);
    Hamamatsu::DCam *GetHamamatsuDCam();

    Status ConnectAll();
    Status DisconnectAll();

    std::set<std::string> ListDevice();
    std::set<std::string> ListConnectedDevice();
    std::vector<PropertyPath> ListProperty(PropertyPath path);

    StatusOr<std::string> GetProperty(PropertyPath path);
    Status SetProperty(PropertyPath path, std::string value);
    Status SetProperty(PropertyValueMap path_value_map);
    Status WaitPropertyFor(std::vector<PropertyPath> path_list,
                           std::chrono::milliseconds timeout);
    Status WaitPropertyUntil(std::vector<PropertyPath> path_list,
                             std::chrono::steady_clock::time_point timepoint);

    PropertyValueMap GetPropertySnapshot();
    PropertyValueMap
    GetPropertySnapshot(std::set<std::string> dev_name_set);

    void SubscribeEvents(EventStream *channel);

private:
    std::map<std::string, Device *> dev_map;
    Hamamatsu::DCam *hamamatsu_dcam = nullptr;

    std::vector<EventStream *> event_subscriber_list;

    Status runDeviceConnect(std::string dev_name, Device *dev);
    Status runDeviceDisconnect(std::string dev_name, Device *dev);
    Status mergeDeviceTaskStatus(
        std::map<std::string, std::future<Status>> &dev_task_future_map);
};


#endif
