#include "flir_spinnaker.h"

#include <SpinGenApi/SpinnakerGenApi.h>

#include "logging.h"

namespace FLIR {

std::vector<std::string> DetectDevice()
{
    std::vector<std::string> sn_list;

    Spinnaker::SystemPtr system = Spinnaker::System::GetInstance();
    Spinnaker::CameraList camList = system->GetCameras();
    for (int i = 0; i < camList.GetSize(); i++) {
        Spinnaker::CameraPtr pCam = camList.GetByIndex(i);
        Spinnaker::GenApi::CStringPtr str_node =
            pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");
        std::string sn = std::string(str_node->GetValue());
        sn_list.push_back(sn);
    }
    camList.Clear();
    system->ReleaseInstance();
    return sn_list;
}

Camera::Camera(std::string serial) { this->serial = serial; }

Camera::~Camera()
{
    clearNodeMap();
    pCam = nullptr; // Release shared_ptr to the camera
    if (system) {
        system->ReleaseInstance();
        system = nullptr;
    }
}

Status Camera::Connect()
{
    if (connected) {
        throw std::runtime_error("already connected");
    }

    if (system == nullptr) {
        system = Spinnaker::System::GetInstance();
    }

    if (pCam == nullptr) {
        Spinnaker::CameraList camList = system->GetCameras();
        if (!serial.empty()) {
            pCam = camList.GetBySerial(serial);
            if (pCam == nullptr) {
                camList.Clear();
                system->ReleaseInstance();
                return absl::UnavailableError("camera not found");
            }
        } else {
            if (camList.GetSize() == 0) {
                camList.Clear();
                system->ReleaseInstance();
                return absl::UnavailableError("no camera found");
            } else if (camList.GetSize() > 1) {
                LOG_WARN("found more than one ({}) cameras, using the first",
                         camList.GetSize());
            }
            pCam = camList.GetByIndex(0);
            if (pCam == nullptr) {
                camList.Clear();
                system->ReleaseInstance();
                return absl::UnavailableError("failed get the first camera");
            }
        }
        camList.Clear();
    }

    SendEvent({
        .type = ::EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::Connecting,
    });

    pCam->Init();
    populateNodes();

    connected = true;
    SendEvent({
        .type = ::EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::Connected,
    });

    return absl::OkStatus();
}

Status Camera::Disconnect()
{
    SendEvent({
        .type = ::EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::Disconnecting,
    });

    if (pCam) {
        pCam->DeInit();
    }
    clearNodeMap();
    populateTLNodes();
    connected = false;

    SendEvent({
        .type = ::EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::NotConnected,
    });

    return absl::OkStatus();
}


::PropertyNode *Camera::Node(std::string name)
{
    auto it = node_map.find(name);
    if (it == node_map.end()) {
        return nullptr;
    }
    return it->second;
}

std::map<std::string, ::PropertyNode *> Camera::NodeMap()
{
    std::map<std::string, ::PropertyNode *> base_node_map;
    for (auto &[name, node] : node_map) {
        base_node_map[name] = node;
    }
    return base_node_map;
}

void Camera::clearNodeMap()
{
    for (auto &[name, node] : node_map) {
        delete node;
    }
    node_map.clear();
}

void Camera::populateTLNodes()
{
    Spinnaker::GenApi::INodeMap &tl_dev_node_map = pCam->GetTLDeviceNodeMap();
    Spinnaker::GenApi::NodeList_t tl_dev_node_list;
    tl_dev_node_map.GetNodes(tl_dev_node_list);
    for (const auto &dev_node : tl_dev_node_list) {
        std::string name = std::string(dev_node->GetName());

        if (dev_node->IsFeature()) {
            PropertyNode *node = new PropertyNode;
            node->dev = this;
            node->name = name;

            node_map[name] = node;
        }
    }
}

void Camera::populateNodes()
{
    Spinnaker::GenApi::INodeMap &dev_node_map = pCam->GetNodeMap();
    Spinnaker::GenApi::NodeList_t dev_node_list;
    dev_node_map.GetNodes(dev_node_list);
    for (const auto &dev_node : dev_node_list) {
        std::string name = std::string(dev_node->GetName());

        if (dev_node->IsFeature()) {
            PropertyNode *node = new PropertyNode;
            node->dev = this;
            node->name = name;

            node_map[name] = node;
        }
    }
}

Spinnaker::GenApi::INode *PropertyNode::get_dev_node()
{
    if (dev->connected) {
        Spinnaker::GenApi::INodeMap &dev_node_map = dev->pCam->GetNodeMap();
        return dev_node_map.GetNode(name.c_str());
    } else {
        Spinnaker::GenApi::INodeMap &dev_node_map = dev->pCam->GetTLDeviceNodeMap();
        return dev_node_map.GetNode(name.c_str());
    }
}

std::string PropertyNode::Description()
{
    Spinnaker::GenApi::INode *dev_node = get_dev_node();
    return std::string(dev_node->GetDescription());
}

bool PropertyNode::Valid()
{
    Spinnaker::GenApi::INode *dev_node = get_dev_node();
    return Spinnaker::GenApi::IsAvailable(dev_node);
}

bool PropertyNode::Readable()
{
    Spinnaker::GenApi::INode *dev_node = get_dev_node();
    return Spinnaker::GenApi::IsReadable(dev_node);
}

bool PropertyNode::Writeable()
{
    Spinnaker::GenApi::INode *dev_node = get_dev_node();
    return Spinnaker::GenApi::IsReadable(dev_node);
}

StatusOr<std::string> PropertyNode::GetValue()
{
    Spinnaker::GenApi::INode *dev_node = get_dev_node();
    Spinnaker::GenApi::CValuePtr str_node = static_cast<Spinnaker::GenApi::CValuePtr>(dev_node);
    return std::string(str_node->ToString());
}

Status PropertyNode::SetValue(std::string value)
{
    Spinnaker::GenApi::INode *dev_node = get_dev_node();
    Spinnaker::GenApi::CValuePtr str_node = static_cast<Spinnaker::GenApi::CValuePtr>(dev_node);
    // TODO: do things!
    return absl::OkStatus();
}

std::optional<std::string> PropertyNode::GetSnapshot()
{
    StatusOr<std::string> value = GetValue();
    if (value.ok()) {
        return value.value();
    }
    return {};
}

} // namespace FLIR
