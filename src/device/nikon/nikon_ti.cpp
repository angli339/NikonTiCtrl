#include "device/nikon/nikon_ti.h"

#include <fmt/format.h>

#include "device/nikon/mm_api.h"
#include "device/nikon/nikon_ti_prop_info.h"
#include "logging.h"
#include "utils/wmi.h"

namespace NikonTi {

std::map<MM_Session, Microscope *> mm_session_map;

void onPropertyChanged(MM_Session mmc, const char *label, const char *property,
                       const char *value)
{
    auto it = mm_session_map.find(mmc);
    if (it == mm_session_map.end()) {
        return;
    }
    Microscope *dev = it->second;
    dev->handlePropertyChangedCallback(mmc, label, property, value);
}

void onStagePositionChanged(MM_Session mmc, char *label, double pos)
{
    auto it = mm_session_map.find(mmc);
    if (it == mm_session_map.end()) {
        return;
    }
    Microscope *dev = it->second;
    dev->handleStagePositionChangedCallback(mmc, label, pos);
}

struct MM_EventCallback mmCallback = {
    .onPropertyChanged = onPropertyChanged,
    .onStagePositionChanged = onStagePositionChanged,
};

Microscope::Microscope()
{
    try {
        load_MMCoreC();
        LOG_DEBUG("MMCoreC loaded");
    } catch (std::exception &e) {
        throw std::runtime_error(fmt::format("load MMCoreC.dll: {}", e.what()));
    }

    for (const auto &[name, info] : prop_info) {
        PropertyNode *node = new PropertyNode;
        node->dev = this;

        node->name = name;
        node->description = info.description;
        node->default_value = info.default_value;
        node->options = info.options;
        node->mm_label = info.mm_label;
        node->mm_property = info.mm_property;
        node->readonly = info.readonly;
        node->value_converter = info.value_converter;

        node->valid = false;
        node_map[name] = node;
    }
}

Microscope::~Microscope()
{
    if (IsConnected()) {
        Disconnect();
    }
    for (const auto &[name, node] : node_map) {
        delete node;
    }
}

bool Microscope::DetectDevice() const
{
    WMI w;
    std::vector<std::string> usbList;

    try {
        usbList = w.listUSBDeviceID("04B0", "7832");
    } catch (std::exception &e) {
        LOG_ERROR("NikonTi: failed to detect device: {}", e.what());
        throw;
    }

    if (usbList.size() > 0) {
        LOG_DEBUG("NikonTi: USB connection detected");
    }
    return usbList.size() != 0;
}

Status Microscope::Connect()
{
    std::unique_lock<std::mutex> lk(mmc_mutex);

    if (connected) {
        return absl::OkStatus();
    }
    if (!DetectDevice()) {
        return absl::UnavailableError("device not detected");
    }

    SendEvent({
        .type = EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::Connecting,
    });

    MM_Error mm_err;
    mmcore->MM_Open(&mmc);

    //
    // Initialize microscope hub
    //
    std::vector<std::string> loaded_modules;

    mm_err = mmcore->MM_LoadDevice(mmc, "TIScope", "NikonTI", "TIScope");
    if (mm_err != MM_ErrOK) {
        mmcore->MM_Close(mmc);
        mmc = nullptr;
        SendEvent({
            .type = EventType::DeviceConnectionStateChanged,
            .value = DeviceConnectionState::NotConnected,
        });
        return absl::UnavailableError(
            fmt::format("load TIScope: {}", MM_ErrorToString(mm_err)));
    }
    mm_err = mmcore->MM_InitializeDevice(mmc, "TIScope");
    if (mm_err != MM_ErrOK) {
        mmcore->MM_Close(mmc);
        mmc = nullptr;
        SendEvent({
            .type = EventType::DeviceConnectionStateChanged,
            .value = DeviceConnectionState::NotConnected,
        });
        return absl::UnavailableError(
            fmt::format("init TIScope: {}", MM_ErrorToString(mm_err)));
    }
    loaded_modules.push_back("TIScope");

    //
    // Initialize modules
    //
    std::vector<std::string> modules = {
        "TIFilterBlock1", "TIZDrive",    "TIDiaShutter", "TINosePiece",
        "TILightPath",    "TIPFSOffset", "TIPFSStatus",
    };

    std::vector<std::string> module_err_msgs;
    for (const auto &module : modules) {
        mm_err = mmcore->MM_LoadDevice(mmc, module.c_str(), "NikonTI",
                                       module.c_str());
        if (mm_err != MM_ErrOK) {
            module_err_msgs.push_back(fmt::format("{}(LoadDevice: {})", module,
                                                  MM_ErrorToString(mm_err)));
            continue;
        }
        mm_err = mmcore->MM_InitializeDevice(mmc, module.c_str());
        if (mm_err != MM_ErrOK) {
            module_err_msgs.push_back(fmt::format(
                "{}(InitializeDevice: {})", module, MM_ErrorToString(mm_err)));
            continue;
        }
        loaded_modules.push_back(module);

        // Set focus device so that we can get Z focus with GetPosition()
        if (module == "TIZDrive") {
            mm_err = mmcore->MM_SetFocusDevice(mmc, "TIZDrive");
            if (mm_err != MM_ErrOK) {
                module_err_msgs.push_back(
                    fmt::format("{}(SetFocusDevice: {})", module,
                                MM_ErrorToString(mm_err)));
                mm_err = mmcore->MM_UnloadDevice(mmc, module.c_str());
                if (mm_err == MM_ErrOK) {
                    loaded_modules.pop_back();
                } else {
                    module_err_msgs.push_back(
                        fmt::format("{}(UnloadDevice: {})", module,
                                    MM_ErrorToString(mm_err)));
                }
            }
        }
    }

    // Enable property nodes of loaded modules
    for (const auto &module : loaded_modules) {
        for (const auto &[name, node] : node_map) {
            if (node->mm_label == module) {
                node->valid = true;
            }
        }
    }

    // Release lock to allow GetValue()
    lk.unlock();

    // Enumerate properties
    for (const auto &[name, node] : node_map) {
        if (node->Valid()) {
            StatusOr<std::string> value = node->GetValue();
            if (!value.ok()) {
                LOG_WARN("node {} is disabled: {}", name,
                         value.status().ToString());
                node->valid = false;
            }
        }
    }

    // Register callback
    mm_session_map[mmc] = this;
    mmcore->MM_RegisterCallback(mmc, &mmCallback);

    connected = true;
    SendEvent({
        .type = EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::Connected,
    });

    if (module_err_msgs.size() > 0) {
        return absl::AbortedError(
            fmt::format("{}", fmt::join(module_err_msgs, ", ")));
    }
    return absl::OkStatus();
}

Status Microscope::Disconnect()
{
    std::lock_guard<std::mutex> lk(mmc_mutex);

    if (mmc == nullptr) {
        return absl::OkStatus();
    }

    SendEvent({
        .type = EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::Disconnecting,
    });
    mmcore->MM_UnloadAllDevices(mmc);
    mmcore->MM_Close(mmc);
    mm_session_map.erase(mmc);
    mmc = nullptr;

    connected = false;
    SendEvent({
        .type = EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::NotConnected,
    });
    return absl::OkStatus();
}

Status Microscope::SetProperty(std::string name, std::string value)
{
    Status status = Device::SetProperty(name, value);
    if (!status.ok()) {
        return status;
    }

    utils::StopWatch sw;
    if (name == "DiaShutter") {
        for (;;) {
            StatusOr<std::string> get_value = GetProperty(name);
            if (!get_value.ok()) {
                return get_value.status();
            }
            if (get_value.value() == value) {
                return absl::OkStatus();
            }
            if (sw.Milliseconds() > 500) {
                return absl::DeadlineExceededError("");
            }
        }
    }
    return absl::OkStatus();
}

::PropertyNode *Microscope::Node(std::string name)
{
    auto it = node_map.find(name);
    if (it == node_map.end()) {
        return nullptr;
    }
    return it->second;
}

std::map<std::string, ::PropertyNode *> Microscope::NodeMap()
{
    std::map<std::string, ::PropertyNode *> base_node_map;
    for (const auto &[name, node] : node_map) {
        base_node_map[name] = node;
    }
    return base_node_map;
}

void Microscope::handlePropertyChangedCallback(MM_Session mmc,
                                               std::string mm_label,
                                               std::string mm_property,
                                               std::string mm_value)
{
    if (mmc != this->mmc) {
        LOG_WARN("mmc does not match");
        return;
    }
    PropertyNode *node = getNodeFromMMLabelProperty(mm_label, mm_property);
    if (!node) {
        return;
    }
    std::string value;
    if (node->value_converter) {
        try {
            value = node->value_converter->valueFromAPI(mm_value);
        } catch (std::exception &e) {
            LOG_ERROR("convert value \"{}\" from MMCore: {}", mm_value,
                      e.what());
            return;
        }
    } else {
        value = mm_value;
    }
    node->handleValueUpdate(value);
}

void Microscope::handleStagePositionChangedCallback(MM_Session mmc,
                                                    std::string mm_label,
                                                    double pos)
{
    if (mmc != this->mmc) {
        LOG_WARN("mmc does not match");
        return;
    }
    PropertyNode *node = getNodeFromMMLabelProperty(mm_label, "");
    if (!node) {
        return;
    }
    std::string value = fmt::format("{:.3f}", pos);
    node->handleValueUpdate(value);
}

PropertyNode *Microscope::getNodeFromMMLabelProperty(std::string mm_label,
                                                     std::string mm_property)
{
    for (const auto &[name, node] : node_map) {
        if ((node->mm_label == mm_label) && (node->mm_property == mm_property))
        {
            return node;
        }
    }
    return nullptr;
}

StatusOr<std::string> PropertyNode::GetValue()
{
    std::string value;

    if (name == "ZDrivePosition") {
        MM_Error mm_err;
        double mm_value = 0;
        {
            std::lock_guard<std::mutex> lk(dev->mmc_mutex);
            mm_err =
                mmcore->MM_GetPosition(dev->mmc, mm_label.c_str(), &mm_value);
        }
        if (mm_err != 0) {
            return absl::UnavailableError(
                fmt::format("MM_GetPosition: {}", MM_ErrorToString(mm_err)));
        }
        value = fmt::format("{:.3f}", mm_value);
    } else {
        MM_Error mm_err;
        char *mm_value_c_str = nullptr;
        {
            std::lock_guard<std::mutex> lk(dev->mmc_mutex);
            mm_err =
                mmcore->MM_GetProperty(dev->mmc, mm_label.c_str(),
                                       mm_property.c_str(), &mm_value_c_str);
        }
        if (mm_err != 0) {
            return absl::UnavailableError(
                fmt::format("MM_GetProperty: {}", MM_ErrorToString(mm_err)));
        }
        std::string mm_value = std::string(mm_value_c_str);
        mmcore->MM_StringFree(mm_value_c_str);
        if (value_converter) {
            try {
                value = value_converter->valueFromAPI(mm_value);
            } catch (std::exception &e) {
                // value converter bug or communication error
                return absl::UnavailableError(
                    fmt::format("convert value \"{}\" from MMCore: {}",
                                mm_value, e.what()));
            }
        } else {
            value = mm_value;
        }
    }

    handleValueUpdate(value);
    return value;
}

Status PropertyNode::SetValue(std::string value)
{
    // Convert value
    std::string mm_value;
    if (value_converter) {
        try {
            mm_value = value_converter->valueToAPI(value);
        } catch (std::exception &e) {
            return absl::InvalidArgumentError(fmt::format(
                "convert value \"{}\" to MMCore: {}", value, e.what()));
        }
    } else {
        mm_value = value;
    }

    // Set value
    if (name == "ZDrivePosition") {
        double mm_pos = std::stod(mm_value);
        MM_Error mm_err;
        {
            std::lock_guard<std::mutex> lk(dev->mmc_mutex);
            mm_err = mmcore->MM_SetPosition(dev->mmc, mm_label.c_str(), mm_pos);
        }
        if (mm_err != 0) {
            return absl::UnavailableError(
                fmt::format("MM_SetPosition: {}", MM_ErrorToString(mm_err)));
        }
    } else {
        MM_Error mm_err;
        {
            std::lock_guard<std::mutex> lk(dev->mmc_mutex);
            mm_err = mmcore->MM_SetPropertyString(dev->mmc, mm_label.c_str(),
                                                  mm_property.c_str(),
                                                  mm_value.c_str());
        }
        if (mm_err != 0) {
            return absl::UnavailableError(fmt::format(
                "MM_SetPropertyString: {}", MM_ErrorToString(mm_err)));
        }
    }

    // Record the operation
    {
        std::unique_lock<std::shared_mutex> lk(mutex_set);
        pending_set_value = value;
    }

    return absl::OkStatus();
}

Status PropertyNode::WaitFor(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::shared_mutex> lk(mutex_set);
    bool ok = cv_set.wait_for(
        lk, timeout, [this] { return !pending_set_value.has_value(); });
    if (ok) {
        return absl::OkStatus();
    } else {
        return absl::DeadlineExceededError("");
    }
}

Status PropertyNode::WaitUntil(std::chrono::steady_clock::time_point timepoint)
{
    std::unique_lock<std::shared_mutex> lk(mutex_set);
    bool ok = cv_set.wait_until(
        lk, timepoint, [this] { return !pending_set_value.has_value(); });
    if (ok) {
        return absl::OkStatus();
    } else {
        return absl::DeadlineExceededError("");
    }
}

std::optional<std::string> PropertyNode::GetSnapshot()
{
    std::shared_lock<std::shared_mutex> lk(mutex_snapshot);
    return snapshot_value;
}

void PropertyNode::handleValueUpdate(std::string value)
{
    std::optional<std::string> previous_value;
    {
        // update snapshot
        std::unique_lock<std::shared_mutex> lk_snapshot(mutex_snapshot);
        previous_value = snapshot_value;
        snapshot_value = value;
        snapshot_timepoint = std::chrono::steady_clock::now();
    }
    bool value_changed =
        (!previous_value.has_value()) || (value != previous_value.value());

    // find out whether set operation exists and is completed
    bool set_completed = false;
    std::unique_lock<std::shared_mutex> lk_set(mutex_set);
    if (pending_set_value.has_value()) {
        std::string request_set_value = pending_set_value.value();
        if (name == "ZDrivePosition") {
            double request_pos = std::stod(request_set_value);
            double pos = std::stod(value);
            double diff = std::abs(pos - request_pos);
            const double tolerance = 0.1 + 0.001;
            if (diff < tolerance) {
                set_completed = true;
                pending_set_value.reset();
            }
        } else {
            if (request_set_value == value) {
                set_completed = true;
                pending_set_value.reset();
            }
        }
    }

    // unlock and notify
    lk_set.unlock();

    if (value_changed) {
        dev->SendEvent({.type = EventType::DevicePropertyValueUpdate,
                        .path = name,
                        .value = value});
    }
    if (set_completed) {
        cv_set.notify_all();
        dev->SendEvent({.type = EventType::DeviceOperationComplete,
                        .path = name,
                        .value = value});
    }
}

} // namespace NikonTi
