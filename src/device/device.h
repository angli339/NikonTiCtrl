#ifndef DEVICE_H
#define DEVICE_H

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <absl/status/status.h>
#include <absl/status/statusor.h>
using absl::Status;
using absl::StatusOr;

#include "eventstream.h"

class Device;
class PropertyNode;

class Device : public EventSender {
public:
    virtual Status Connect() = 0;
    virtual Status Disconnect() = 0;
    virtual bool IsConnected() = 0;

    virtual PropertyNode *Node(std::string name) = 0;
    virtual std::map<std::string, PropertyNode *> NodeMap() = 0;

    virtual bool HasProperty(std::string property);
    virtual std::vector<std::string> ListProperty();
    virtual std::string PropertyDescription(std::string property);

    virtual StatusOr<std::string> GetProperty(std::string property);
    virtual Status SetProperty(std::string property, std::string value);
    virtual Status
    SetProperty(std::map<std::string, std::string> property_value_map);
    virtual Status WaitPropertyFor(std::vector<std::string> property_list,
                                   std::chrono::milliseconds timeout);
    virtual Status
    WaitPropertyUntil(std::vector<std::string> property_list,
                      std::chrono::steady_clock::time_point timepoint);

    virtual std::map<std::string, std::string> GetPropertySnapshot();
};

class PropertyNode {
public:
    virtual std::string Name() = 0;
    virtual std::string Description() = 0;
    virtual bool Valid() = 0;
    virtual bool Readable() = 0;
    virtual bool Writeable() = 0;
    virtual std::vector<std::string> Options() = 0;

    virtual StatusOr<std::string> GetValue() = 0;
    virtual Status SetValue(std::string value) = 0;
    virtual Status WaitFor(std::chrono::milliseconds timeout) = 0;
    virtual Status
    WaitUntil(std::chrono::steady_clock::time_point timepoint) = 0;

    virtual std::optional<std::string> GetSnapshot() = 0;
};

#endif
