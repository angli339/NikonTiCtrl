#ifndef DEVICE_FLIR_SPINNAKER_H
#define DEVICE_FLIR_SPINNAKER_H

#include <atomic>
#include <map>
#include <string>

#include <Spinnaker.h>

#include "device/device.h"

namespace FLIR {

class Camera;
class PropertyNode;

std::vector<std::string> DetectDevice();

class Camera : public Device {
    friend class PropertyNode;

public:
    Camera() {}
    Camera(std::string serial);
    ~Camera();

    Status Connect() override;
    Status Disconnect() override;
    bool IsConnected() override { return connected; }

    ::PropertyNode *Node(std::string name) override;
    std::map<std::string, ::PropertyNode *> NodeMap() override;

private:
    std::string serial;
    Spinnaker::SystemPtr system = nullptr;
    Spinnaker::CameraPtr pCam = nullptr;
    
    std::atomic<bool> connected = false;
    std::map<std::string, PropertyNode *> node_map;
    void clearNodeMap();
    void populateTLNodes();
    void populateNodes();
};

class PropertyNode : public ::PropertyNode {
    friend class Camera;

public:
    std::string Name() override { return name; }
    std::string Description() override;
    bool Valid() override;
    bool Readable() override;
    bool Writeable() override;
    std::vector<std::string> Options() override { return {}; }

    StatusOr<std::string> GetValue() override;
    Status SetValue(std::string value) override;

    std::optional<std::string> GetSnapshot() override;

private:
    Camera *dev;
    std::string name;

    Spinnaker::GenApi::INode *get_dev_node();
};

} // namespace FLIR

#endif
