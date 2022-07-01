#include "device/prior/prior_proscan.h"
#include "device/prior/prior_proscan_prop_info.h"

#include <thread>

#include <visa.h>

#include "logging.h"
#include "utils/wmi.h"

std::string ViStatusToString(ViStatus status)
{
    return fmt::format("{:#10x}", uint32_t(status));
}

namespace PriorProscan {

Proscan::Proscan(std::string port_name)
{
    this->port_name = port_name;
    for (const auto &[name, info] : prop_info) {
        PropertyNode *node = new PropertyNode;

        node->dev = this;
        node->name = name;
        node->description = info.description;
        node->getCommand = info.getCommand;
        node->setCommand = info.setCommand;
        node->setResponse = info.setResponse;
        node->isVolatile = info.isVolatile;

        node->valid = false;
        node_map[name] = node;
    }
}

Proscan::~Proscan()
{
    if (IsConnected()) {
        Disconnect();
    }
    for (const auto &[name, node] : node_map) {
        delete node;
    }
}

bool Proscan::DetectDevice() const
{
    WMI w;
    std::vector<std::string> usbList;

    try {
        usbList = w.listUSBDeviceID("10DB", "1234");
    } catch (std::runtime_error &e) {
        LOG_DEBUG("Prior Proscan: failed to detect device: %s", e.what());
        throw e;
    }

    if (usbList.size() > 0) {
        LOG_DEBUG("Prior Proscan: USB connection detected");
    }
    return usbList.size() != 0;
}

Status Proscan::Connect()
{
    std::unique_lock<std::mutex> lk(port_mutex);

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

    ViStatus vi_status;

    if (rm == VI_NULL) {
        vi_status = viOpenDefaultRM(&rm);
        if (vi_status != VI_SUCCESS) {
            SendEvent({
                .type = EventType::DeviceConnectionStateChanged,
                .value = DeviceConnectionState::NotConnected,
            });
            return absl::UnavailableError(fmt::format(
                "viOpenDefaultRM: {}", ViStatusToString(vi_status)));
        }
    }

    // Open serial port
    vi_status = viOpen(rm, port_name.c_str(), VI_EXCLUSIVE_LOCK, 50, &dev);
    if (vi_status != VI_SUCCESS) {
        SendEvent({
            .type = EventType::DeviceConnectionStateChanged,
            .value = DeviceConnectionState::NotConnected,
        });
        return absl::UnavailableError(
            fmt::format("viOpen: {}", ViStatusToString(vi_status)));
    }

    // Set baudrate to 38400
    vi_status = viSetAttribute(dev, VI_ATTR_ASRL_BAUD, 38400);
    if (vi_status != VI_SUCCESS) {
        SendEvent({
            .type = EventType::DeviceConnectionStateChanged,
            .value = DeviceConnectionState::NotConnected,
        });
        return absl::UnavailableError(
            fmt::format("viSetAttribute(VI_ATTR_ASRL_BAUD=38400): {}",
                        ViStatusToString(vi_status)));
    }

    // Set termination character
    vi_status = viSetAttribute(dev, VI_ATTR_TERMCHAR, '\r');
    if (vi_status != VI_SUCCESS) {
        SendEvent({
            .type = EventType::DeviceConnectionStateChanged,
            .value = DeviceConnectionState::NotConnected,
        });
        return absl::UnavailableError(
            fmt::format("viSetAttribute(VI_ATTR_TERMCHAR='\\r'): {}",
                        ViStatusToString(vi_status)));
    }

    // Set minimum timeout to 50 ms
    vi_status = viSetAttribute(dev, VI_ATTR_TMO_VALUE, 50);
    if (vi_status != VI_SUCCESS) {
        SendEvent({
            .type = EventType::DeviceConnectionStateChanged,
            .value = DeviceConnectionState::NotConnected,
        });
        return absl::UnavailableError(
            fmt::format("viSetAttribute(VI_ATTR_TMO_VALUE=50): {}",
                        ViStatusToString(vi_status)));
    }

    // Release lock to allow query()
    lk.unlock();

    // Establish communication
    Status status = checkCommunication(2);
    if (!status.ok()) {
        status = switchBaudrate();
        if (!status.ok()) {
            SendEvent({
                .type = EventType::DeviceConnectionStateChanged,
                .value = DeviceConnectionState::NotConnected,
            });
            return status;
        }
    }

    // Init properties
    std::vector<std::pair<std::string, std::string>> init_prop_list = {
        {"CommandProtocol", "0"},
        {"XYResolution", "0.1"},
    };
    for (const auto &[name, value] : init_prop_list) {
        auto node = node_map[name];
        // check whether set value is needed
        StatusOr<std::string> get_value = node->GetValue();
        if (!get_value.ok()) {
            SendEvent({
                .type = EventType::DeviceConnectionStateChanged,
                .value = DeviceConnectionState::NotConnected,
            });
            return absl::UnavailableError(
                fmt::format("initialize {}: get value: {}", name,
                            get_value.status().ToString()));
        }
        if (get_value.value() == value) {
            continue;
        }

        // set value
        Status set_status = node->SetValue(value);
        if (!set_status.ok()) {
            SendEvent({
                .type = EventType::DeviceConnectionStateChanged,
                .value = DeviceConnectionState::NotConnected,
            });
            return absl::UnavailableError(
                fmt::format("initialize {}: set to \"{}\": {}", name, value,
                            set_status.ToString()));
        }

        // read back to validate
        StatusOr<std::string> readback_value = node->GetValue();
        if (!readback_value.ok()) {
            SendEvent({
                .type = EventType::DeviceConnectionStateChanged,
                .value = DeviceConnectionState::NotConnected,
            });
            return absl::UnavailableError(
                fmt::format("initialize {}: read back: {}", name,
                            readback_value.status().ToString()));
        }
        if (readback_value.value() != value) {
            SendEvent({
                .type = EventType::DeviceConnectionStateChanged,
                .value = DeviceConnectionState::NotConnected,
            });
            return absl::UnavailableError(fmt::format(
                "initialize {}: read back mismatch: set \"{}\", get \"{}\"",
                name, value, readback_value.value()));
        }
    }

    // Enumerate properties
    for (const auto &[name, node] : node_map) {
        node->valid = true;
        if (node->Readable()) {
            StatusOr<std::string> value = node->GetValue();
            if (!value.ok()) {
                node->valid = false;
                LOG_WARN("node {} disabled: get value: {}", name,
                         value.status().ToString());
            }
        }
    }

    // Start polling
    polling = true;
    polling_thread = std::thread([this] {
        std::chrono::duration polling_interval = std::chrono::milliseconds(30);
        while (polling) {
            std::this_thread::sleep_for(polling_interval);
            StatusOr<std::string> value;
            value = node_map["XYPosition"]->GetValue();
            if (!value.ok()) {
                LOG_ERROR("polling XYPosition: {}", value.status().ToString());
                return;
            }
            value = node_map["MotionStatus"]->GetValue();
            if (!value.ok()) {
                LOG_ERROR("polling MotionStatus: {}",
                          value.status().ToString());
                return;
            }
            // TODO: get value of nodes with pending set operation
            //
            // rework the code below to get real dynamic polling interval when
            // an operation is pending (maybe use condition_variable::sleep_for
            // to wake up this thread before timer tick)
            {
                std::shared_lock<std::shared_mutex> lk(
                    node_map["LumenShutter"]->mutex_set);
                if (node_map["LumenShutter"]->pending_set_value.has_value()) {
                    LOG_DEBUG(
                        "[Polling] Pending Op: LumenShutter. MotionStatus={}",
                        value.value());
                    polling_interval = std::chrono::milliseconds(5);
                } else {
                    polling_interval = std::chrono::milliseconds(30);
                }
            }
        }
    });

    connected = true;
    SendEvent({
        .type = EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::Connected,
    });
    return absl::OkStatus();
}

Status Proscan::Disconnect()
{
    if (polling) {
        polling = false;
        polling_thread.join();
    }

    std::lock_guard<std::mutex> lk(port_mutex);
    if (dev == VI_NULL) {
        return absl::OkStatus();
    }

    SendEvent({
        .type = EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::Disconnecting,
    });
    ViStatus vi_status;
    vi_status = viClose(dev);
    if (vi_status != VI_SUCCESS) {
        return absl::UnavailableError(
            fmt::format("viClose: {}", ViStatusToString(vi_status)));
    }
    dev = VI_NULL;

    connected = false;
    SendEvent({
        .type = EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::NotConnected,
    });
    return absl::OkStatus();
}

::PropertyNode *Proscan::Node(std::string name)
{
    auto it = node_map.find(name);
    if (it == node_map.end()) {
        return nullptr;
    }
    return it->second;
}

std::map<std::string, ::PropertyNode *> Proscan::NodeMap()
{
    std::map<std::string, ::PropertyNode *> base_node_map;
    for (const auto &[name, node] : node_map) {
        base_node_map[name] = node;
    }
    return base_node_map;
}

Status Proscan::checkCommunication(int n_attempt)
{
    StatusOr<std::string> resp = query("");
    if (resp.ok()) {
        return absl::OkStatus();
    } else {
        if (n_attempt == 1) {
            return resp.status();
        } else {
            return checkCommunication(n_attempt - 1);
        }
    }
}

Status Proscan::switchBaudrate()
{
    //
    // Check communication at 9600
    //
    ViStatus vi_status;
    vi_status = viSetAttribute(dev, VI_ATTR_ASRL_BAUD, 9600);
    if (vi_status != VI_SUCCESS) {
        return absl::UnavailableError(
            fmt::format("viSetAttribute(VI_ATTR_ASRL_BAUD=9600): {}",
                        ViStatusToString(vi_status)));
    }

    Status status;
    status = checkCommunication(3);
    if (!status.ok()) {
        return absl::UnavailableError(fmt::format(
            "communicate at the default baudrate 9600: {}", status.ToString()));
    }

    // Set ProScan to 38400
    status = write("BAUD,38");
    if (!status.ok()) {
        return absl::UnavailableError(fmt::format(
            "set ProScan baudrate: write(\"BAUD,38\"): {}", status.ToString()));
    }
    vi_status = viFlush(dev, VI_WRITE_BUF);
    if (vi_status != VI_SUCCESS) {
        return absl::UnavailableError(
            fmt::format("set ProScan baudrate: viFlush(VI_WRITE_BUF): {}",
                        status.ToString()));
    }

    // Add some delay for ProScan to finish the work
    // Otherwise it does not respone to the commands
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Set serial port to 38400
    vi_status = viSetAttribute(dev, VI_ATTR_ASRL_BAUD, 38400);
    if (vi_status != VI_SUCCESS) {
        return absl::UnavailableError(
            fmt::format("viSetAttribute(VI_ATTR_ASRL_BAUD=38400): {}",
                        ViStatusToString(vi_status)));
    }

    // Check communication at 38400
    status = checkCommunication(2);
    if (!status.ok()) {
        return absl::UnavailableError(
            fmt::format("communicate at the requested baudrate 38400: {}",
                        status.ToString()));
    }
    return absl::OkStatus();
}

StatusOr<uint32_t> Proscan::clearReadBuffer()
{
    char buf[4096];
    ViStatus vi_status;
    ViUInt32 count;

    vi_status = viGetAttribute(dev, VI_ATTR_ASRL_AVAIL_NUM, &count);
    if (vi_status != VI_SUCCESS) {
        return absl::UnavailableError(
            fmt::format("viGetAttribute(VI_ATTR_ASRL_AVAIL_NUM): {}",
                        ViStatusToString(vi_status)));
    }
    if (count > 0) {
        vi_status = viRead(dev, (uint8_t *)buf, count, &count);
        if (vi_status != VI_SUCCESS) {
            return absl::UnavailableError(fmt::format(
                "viRead({}): {}", count, ViStatusToString(vi_status)));
        }
    }
    return count;
}

StatusOr<std::string> Proscan::readline()
{
    ViStatus vi_status;
    ViUInt32 count;

    char buf[4096];
    vi_status = viRead(dev, (uint8_t *)buf, sizeof(buf), &count);

    if (vi_status < VI_SUCCESS) {
        return absl::UnavailableError(
            fmt::format("viRead({}): {}", count, ViStatusToString(vi_status)));
    }
    if (buf[count - 1] != '\r') {
        return absl::UnavailableError(fmt::format(
            "unexpected response: {} bytes not terminated by \\r", count));
    }
    std::string resp = std::string(buf, count - 1);
    return resp;
}

Status Proscan::write(const std::string command)
{
    std::lock_guard<std::mutex> lk(port_mutex);

    ViStatus vi_status;
    ViUInt32 count;

    std::string cmd_str = command + "\r";
    vi_status = viWrite(dev, (const uint8_t *)cmd_str.c_str(),
                        (ViUInt32)(cmd_str.size()), &count);

    if (vi_status != VI_SUCCESS) {
        return absl::UnavailableError(
            fmt::format("viWrite: {}", ViStatusToString(vi_status)));
    }
    return absl::OkStatus();
}

StatusOr<std::string> Proscan::query(const std::string command)
{
    std::lock_guard<std::mutex> lk(port_mutex);

    StatusOr<uint32_t> bytes_discarded = clearReadBuffer();
    if (!bytes_discarded.ok()) {
        return absl::UnavailableError(fmt::format(
            "clearReadBuffer: {}", bytes_discarded.status().ToString()));
    }
    if (*bytes_discarded > 0) {
        LOG_WARN("discarded {} bytes of unexpected data before sending command",
                 *bytes_discarded);
    }

    ViStatus vi_status;
    ViUInt32 count;

    //
    // Send command
    //
    std::string cmd_str = command + "\r";
    vi_status = viWrite(dev, (const uint8_t *)cmd_str.c_str(),
                        ViUInt32(cmd_str.size()), &count);
    if (vi_status != VI_SUCCESS) {
        return absl::UnavailableError(
            fmt::format("viWrite: {}", ViStatusToString(vi_status)));
    }

    //
    // Read line
    //
    StatusOr<std::string> resp = readline();
    if (!resp.ok()) {
        return absl::UnavailableError(
            fmt::format("readline: {}", resp.status().ToString()));
    }

    //
    // Check for ProScan Error Code
    //
    std::string resp_str = *resp;
    if ((resp_str.size() >= 2) && (resp_str.substr(0, 2) == "E,")) {
        auto it = error_code.find(resp_str);
        if (it != error_code.end()) {
            std::string err_msg = it->second;
            return absl::UnavailableError(
                fmt::format("ProScan response: {}({})", resp_str, err_msg));
        } else {
            return absl::UnavailableError(
                fmt::format("ProScan response: {}", resp_str));
        }
    }
    return resp;
}

bool PropertyNode::Readable()
{
    if (name == "LumenOutputIntensity") {
        return true;
    }
    if (name == "XYPosition") {
        return true;
    }
    if (getCommand != "") {
        return true;
    } else {
        return false;
    }
}

bool PropertyNode::Writeable()
{
    if (name == "LumenOutputIntensity") {
        return true;
    }
    if (name == "XYPosition") {
        return true;
    }
    if (setCommand != "") {
        return true;
    } else {
        return false;
    }
}

double Proscan::getXYResolution()
{
    std::optional<std::string> xy_resolution_str =
        node_map["XYResolution"]->GetSnapshot();
    if (!xy_resolution_str.has_value()) {
        throw std::runtime_error("cannot get snapshot of XYResolution");
    }

    if ((xy_resolution_str.value() != "0.1") &&
        (xy_resolution_str.value() != "1")) {
        throw std::invalid_argument(fmt::format(
            "unexpected xy_resolution: '{}'", xy_resolution_str.value()));
    }
    try {
        return std::stod(xy_resolution_str.value());
    } catch (std::exception &e) {
        throw std::invalid_argument(
            fmt::format("invalid xy_resolution: '{}': {}",
                        xy_resolution_str.value(), e.what()));
    }
}

std::string Proscan::convertXYPositionFromRaw(std::string raw_xy_position)
{
    double xy_resolution = getXYResolution();

    int x_raw, y_raw;
    try {
        auto n = raw_xy_position.find(',');
        x_raw = std::stoi(raw_xy_position.substr(0, n));
        y_raw = std::stoi(raw_xy_position.substr(n + 1));
    } catch (std::exception &e) {
        throw std::invalid_argument(fmt::format(
            "invalid raw_xy_position: '{}': {}", raw_xy_position, e.what()));
    }

    double x = xy_resolution * (double)(x_raw);
    double y = xy_resolution * (double)(y_raw);
    std::string xy_position = fmt::format("{:.1f},{:.1f}", x, y);
    return xy_position;
}

std::string Proscan::convertXYPositionToRaw(std::string xy_position)
{
    double xy_resolution = getXYResolution();

    double x, y;
    try {
        auto n = xy_position.find(',');
        x = stod(xy_position.substr(0, n));
        y = stod(xy_position.substr(n + 1));
    } catch (std::exception &e) {
        throw std::invalid_argument(fmt::format("invalid xy_position: '{}': {}",
                                                xy_position, e.what()));
    }

    int x_raw = (int)(x / xy_resolution);
    int y_raw = (int)(y / xy_resolution);
    std::string raw_xy_position = fmt::format("{:d},{:d}", x_raw, y_raw);
    return raw_xy_position;
}

StatusOr<std::string> PropertyNode::GetValue()
{
    if (!Readable()) {
        return absl::PermissionDeniedError("not readable");
    }
    if (name == "LumenOutputIntensity") {
        std::lock_guard<std::mutex> lk(dev->pseudo_prop_mutex);
        std::string value = fmt::format("{:d}", dev->lumen_output_intensity);
        // Getting LumenOutputIntensity does not trigger an update event.
        // Getting LumenShutter or setting LumenOutputIntensity will trigger
        // update events.
        return value;
    }

    if (name == "XYPosition") {
        StatusOr<std::string> raw_value =
            dev->node_map["RawXYPosition"]->GetValue();
        if (!raw_value.ok()) {
            return absl::UnavailableError(fmt::format(
                "get RawXYPosition: {}", raw_value.status().ToString()));
        }
        std::string value;
        try {
            value = dev->convertXYPositionFromRaw(raw_value.value());
        } catch (std::exception &e) {
            return absl::UnavailableError(
                fmt::format("convert from RawXYPosition: {}", e.what()));
        }
        handleValueUpdate(value);
        return value;
    }

    StatusOr<std::string> resp = dev->query(getCommand);
    if (!resp.ok()) {
        return absl::UnavailableError(
            fmt::format("query: {}", resp.status().ToString()));
    }
    std::string value = resp.value();

    std::optional<uint8_t> lumen_intensity;
    if (name == "LumenShutter") {
        if (value == "0") {
            value = "Off";
        } else {
            try {
                lumen_intensity = std::stoi(value);
                value = "On";
            } catch (std::exception &e) {
                return absl::UnavailableError(fmt::format(
                    "invalid response: \"{}\", expecting an integer", value));
            }
        }
    }
    handleValueUpdate(value);

    // Update LumenOutputIntensity when the actual number is read back when
    // getting LumenShutter
    if (lumen_intensity.has_value()) {
        std::lock_guard<std::mutex> lk(dev->pseudo_prop_mutex);
        if (lumen_intensity.value() != dev->lumen_output_intensity) {
            std::string lumen_intensity_str =
                fmt::format("{}", lumen_intensity.value());
            dev->node_map["LumenOutputIntensity"]->handleValueUpdate(
                lumen_intensity_str);
        }
    }
    return value;
}

Status PropertyNode::SetValue(std::string value)
{
    if (!Writeable()) {
        return absl::PermissionDeniedError("not writeable");
    }

    if (name == "XYPosition") {
        std::string raw_value;
        try {
            raw_value = dev->convertXYPositionToRaw(value);
        } catch (std::exception &e) {
            return absl::UnavailableError(
                fmt::format("convert to RawXYPosition: {}", e.what()));
        }
        Status status = dev->node_map["RawXYPosition"]->SetValue(raw_value);
        if (!status.ok()) {
            return status;
        }

        // record the set operation as pending
        std::unique_lock<std::shared_mutex> lk(mutex_set);
        pending_set_value = value;
        return absl::OkStatus();
    }

    if (name == "LumenOutputIntensity") {
        uint8_t intensity;
        try {
            intensity = std::stoi(value);
        } catch (std::exception &e) {
            return absl::InvalidArgumentError(
                fmt::format("convert to integer: {}", e.what()));
        }
        if ((intensity < 1) || (intensity > 100)) {
            return absl::OutOfRangeError("expected range is [1, 100]");
        }

        // save the value
        std::lock_guard<std::mutex> lk(dev->pseudo_prop_mutex);
        dev->lumen_output_intensity = intensity;

        // record the set operation as pending, then provide the value to
        // generate value update and operation complete events
        {
            std::unique_lock<std::shared_mutex> lk_set(mutex_set);
            pending_set_value = value;
        }
        handleValueUpdate(value);
        return absl::OkStatus();
    }

    // format value used for the serial command
    std::string cmd_value;
    if (name == "LumenShutter") {
        if (value == "On") {
            std::lock_guard<std::mutex> lk(dev->pseudo_prop_mutex);
            cmd_value = fmt::format("{:d}", dev->lumen_output_intensity);
        } else if (value == "Off") {
            cmd_value = "0";
        } else {
            return absl::InvalidArgumentError("expecting 'On' or 'Off'");
        }
    } else {
        cmd_value = value;
    }

    // send the serial command and validate against the expected response
    if (setResponse == "") {
        Status status = dev->write(fmt::format(setCommand, cmd_value));
        if (!status.ok()) {
            return status;
        }
    } else {
        StatusOr<std::string> resp =
            dev->query(fmt::format(setCommand, cmd_value));
        if (!resp.ok()) {
            return resp.status();
        }
        if (resp.value() != setResponse) {
            return absl::UnavailableError(
                fmt::format("unexpected response '{}', expecting '{}'",
                            resp.value(), setResponse));
        }
    }

    // record the set operation as pending
    std::unique_lock<std::shared_mutex> lk(mutex_set);
    pending_set_value = value;
    LOG_DEBUG("[Pending Set Op] {}={}", name, pending_set_value.value());
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

void Proscan::handleMotionStatusUpdate(std::string motion_status_str)
{
    uint8_t motion_status;
    try {
        motion_status = std::stoi(motion_status_str);
    } catch (std::exception &e) {
        LOG_ERROR("invalid response motion_status='{}': {}", motion_status_str,
                  e.what());
        return;
    }

    // 5  4  3  2  1  0
    // F2 F1 F3 ?  Y  X
    const uint8_t MOTIONSTATUS_XY_MASK = 0b11;
    const uint8_t MOTIONSTATUS_F3 = 1 << 3;
    const uint8_t MOTIONSTATUS_F1 = 1 << 4;
    const uint8_t MOTIONSTATUS_F2 = 1 << 5;

    std::vector<std::pair<std::string, uint8_t>> motion_status_fields = {
        {"RawXYPosition", MOTIONSTATUS_XY_MASK},
        {"XYPosition", MOTIONSTATUS_XY_MASK},
        {"FilterWheel3", MOTIONSTATUS_F3},
        {"FilterWheel1", MOTIONSTATUS_F1},
        {"LumenShutter", MOTIONSTATUS_F2},
    };

    for (const auto &[node_name, field_mask] : motion_status_fields) {
        auto node = node_map[node_name];

        std::unique_lock<std::shared_mutex> lk(node->mutex_set);

        // save the status for sending events and notifications later
        bool value_updated = false;
        std::string previous_set_value;
        bool operation_completed = false;

        // check if a node has pending set operation and has stopped motion
        if ((node->pending_set_value.has_value()) &&
            ((motion_status & field_mask) == 0))
        {
            if ((node_name == "FilterWheel1") ||
                (node_name == "FilterWheel3") || (node_name == "LumenShutter"))
            {
                // update snapshot value without another get request to save
                // time positions are not updated based on set value, since get
                // value may not equal set value
                std::unique_lock<std::shared_mutex> lk_snapshot(
                    node->mutex_snapshot);
                node->snapshot_value = node->pending_set_value.value();
                previous_set_value = node->pending_set_value.value();
                value_updated = true;
            }
            // mark operation as complete by reseting pending_set_value
            node->pending_set_value.reset();
            operation_completed = true;
            LOG_DEBUG("[Set Op Complete] {}={}", node_name, previous_set_value);
        }

        // unlock and notify
        lk.unlock();

        if (value_updated) {
            SendEvent({
                .type = EventType::DevicePropertyValueUpdate,
                .path = node_name,
                .value = previous_set_value,
            });
        }

        if (operation_completed) {
            node->cv_set.notify_all();
            SendEvent({
                .type = EventType::DeviceOperationComplete,
                .path = node_name,
                .value = previous_set_value,
            });
        }
    }
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

    if (name == "MotionStatus") {
        // do not check for value_changed, because MotionStatus may not be set
        // before changing back to 0, if the motion stops too fast (shutter)
        //
        // we do not send DevicePropertyValueUpdate event of MotionStatus
        // itself handleMotionStatusUpdate will notify operation complete of
        // motion properties
        dev->handleMotionStatusUpdate(value);
        return;
    }

    // find out whether set operation exists and is completed
    bool set_completed = false;
    std::unique_lock<std::shared_mutex> lk_set(mutex_set);
    if (pending_set_value.has_value()) {
        std::string request_set_value = pending_set_value.value();
        if (request_set_value == value) {
            set_completed = true;
            pending_set_value.reset();
        }
    }

    // unlock and notify
    lk_set.unlock();

    if (value_changed) {
        dev->SendEvent({
            .type = EventType::DevicePropertyValueUpdate,
            .path = name,
            .value = value,
        });
    }
    if (set_completed) {
        cv_set.notify_all();
        dev->SendEvent({.type = EventType::DeviceOperationComplete,
                        .path = name,
                        .value = value});
    }
}

} // namespace PriorProscan
