#ifndef DEVICE_PRIOR_PROSCAN_H
#define DEVICE_PRIOR_PROSCAN_H

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include <absl/status/status.h>
#include <absl/status/statusor.h>

#include <visatype.h>

#include "device/device.h"

std::string ViStatusToString(ViStatus status);

namespace PriorProscan {

using absl::Status;
using absl::StatusOr;

class PriorProscan;
class PropertyNode;

class Proscan : public Device {
    friend class PropertyNode;

public:
    Proscan(std::string port_name);
    ~Proscan();

    bool DetectDevice() const;
    Status Connect() override;
    Status Disconnect() override;
    bool IsConnected() override { return connected; }

    ::PropertyNode *Node(std::string name) override;
    std::map<std::string, ::PropertyNode *> NodeMap() override;

private:
    Status checkCommunication(int n_attempt);
    Status switchBaudrate();
    StatusOr<uint32_t> clearReadBuffer();
    Status write(const std::string command);
    StatusOr<std::string> readline();
    StatusOr<std::string> query(const std::string command);

    double getXYResolution();
    std::string convertXYPositionFromRaw(std::string raw_xy_position);
    std::string convertXYPositionToRaw(std::string raw_xy_position);

    void handleMotionStatusUpdate(std::string motion_status_str);

    std::mutex port_mutex;
    std::string port_name;
    ViSession rm = VI_NULL;
    ViSession dev = VI_NULL;

    std::map<std::string, PropertyNode *> node_map;

    std::atomic<bool> connected = false;

    std::atomic<bool> polling = false;
    std::thread polling_thread;

    std::mutex pseudo_prop_mutex;
    uint8_t lumen_output_intensity = 100;
};

class PropertyNode : public ::PropertyNode {
    friend class Proscan;

public:
    std::string Name() override { return this->name; }
    std::string Description() override { return this->description; }
    bool Valid() override { return this->valid; }
    bool Readable() override;
    bool Writeable() override;
    std::vector<std::string> Options() override { return {}; }

    StatusOr<std::string> GetValue() override;
    Status SetValue(std::string value) override;
    Status WaitFor(std::chrono::milliseconds timeout) override;
    Status WaitUntil(std::chrono::steady_clock::time_point timepoint) override;

    std::optional<std::string> GetSnapshot() override;

private:
    Proscan *dev;

    // Property info
    std::string name;
    std::string description;
    std::string getCommand;
    std::string setCommand;
    std::string setResponse;
    bool isVolatile;

    bool valid;

    // handleValueUpdate updates snapshot, determines set operation status, and
    // sends operation complete event.
    void handleValueUpdate(std::string value);

    std::shared_mutex mutex_snapshot;
    std::optional<std::string> snapshot_value;
    std::chrono::steady_clock::time_point snapshot_timepoint;

    std::shared_mutex mutex_set;
    std::condition_variable_any cv_set;
    std::optional<std::string> pending_set_value;
};

} // namespace PriorProscan

#endif
