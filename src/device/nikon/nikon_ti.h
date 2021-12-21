#ifndef DEVICE_NIKON_TI_H
#define DEVICE_NIKON_TI_H

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
#include <map>

#include <absl/status/status.h>
#include <absl/status/statusor.h>

#include "device/device.h"

namespace NikonTi {

using absl::Status;
using absl::StatusOr;

class Microscope;
class PropertyNode;

// mm_api.h
typedef void *MM_Session;
// nikon_ti_prop_info.h
struct APIValueConvertor;

class Microscope: public Device {
    friend class PropertyNode;
    friend void onPropertyChanged(MM_Session mmc, const char* label, const char* property, const char* value);
    friend void onStagePositionChanged(MM_Session mmc, char* label, double pos);
public:
    Microscope();
    ~Microscope();

    bool DetectDevice() const;
    Status Connect() override;
    Status Disconnect() override;
    bool IsConnected() override { return connected; }

    Status SetProperty(std::string name, std::string value) override;

    ::PropertyNode *Node(std::string name) override;
    std::map<std::string, ::PropertyNode *> NodeMap() override;

private:
    void handlePropertyChangedCallback(MM_Session mmc, std::string label, std::string property, std::string value);
    void handleStagePositionChangedCallback(MM_Session mmc, std::string label, double pos);
    PropertyNode* getNodeFromMMLabelProperty(std::string label, std::string property);

    std::mutex mmc_mutex;
    MM_Session mmc = nullptr;

    // node_map is populated in constructor of the class.
    // A node is marked as valid when the corresponding module is connected.
    std::map<std::string, PropertyNode*> node_map;

    // connected records current connection status.
    // If a connection is lost, mmc is not nullptr, but connected is false.
    std::atomic<bool> connected = false;
};

class PropertyNode: public ::PropertyNode {
    friend class Microscope;
public:
    std::string Name() override { return this->name; }
    std::string Description() override { return this->description; }
    bool Valid() override { return this->valid; }
    bool Readable() override { return true; }
    bool Writeable() override { return !readonly; }
    std::vector<std::string> Options() override { return this->options; }

    StatusOr<std::string> GetValue() override;
    Status SetValue(std::string value) override;
    Status WaitFor(std::chrono::milliseconds timeout) override;
    Status WaitUntil(std::chrono::steady_clock::time_point timepoint) override;

    std::optional<std::string> GetSnapshot() override;

private:
    Microscope *dev;

    // Property info
    std::string name;
    std::string description;
    std::string default_value;
    std::vector<std::string> options;
    std::string mm_label;
    std::string mm_property;
    bool readonly;
    APIValueConvertor *value_converter;

    // Node is marked valid when the module is loaded, initialized and completed the first GetValue() call without error. 
    bool valid = false;
    
    // handleValueUpdate updates snapshot, determines set operation status, and sends operation complete event.
    void handleValueUpdate(std::string value);

    std::shared_mutex mutex_snapshot;
    std::optional<std::string> snapshot_value;
    std::chrono::steady_clock::time_point snapshot_timepoint;

    std::shared_mutex mutex_set;
    std::condition_variable_any cv_set;
    std::optional<std::string> pending_set_value;
};

} // namespace Nikon

#endif
