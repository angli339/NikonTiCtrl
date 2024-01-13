#include "device/hamamatsu/hamamatsu_dcam.h"

#include <cstring>

#include <dcamapi4.h>
#include <dcamprop.h>
#include <fmt/format.h>

#include "logging.h"
#include "utils/time_utils.h"
#include "utils/wmi.h"

namespace Hamamatsu {

std::string DCAMERR_ToString(DCAMERR err)
{
    return fmt::format("{:#010x}", uint32_t(err));
}

DCam::~DCam()
{
    if (IsConnected()) {
        Status status = Disconnect();
        (void)(status);
    }
    for (const auto &[name, node] : node_map) {
        delete node;
    }
}

bool DCam::DetectDevice()
{
    WMI w;
    std::vector<std::string> list1394 = w.list1394DeviceID("HAMAMATSU");
    return list1394.size() != 0;
}

Status DCam::Connect()
{
    std::lock_guard<std::mutex> lk(hdcam_mutex);

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

    //
    // 1. Init API
    //
    DCAMERR err;
    DCAMAPI_INIT apiinit;
    std::memset(&apiinit, 0, sizeof(apiinit));
    apiinit.size = sizeof(apiinit);

    err = dcamapi_init(&apiinit);
    if ((int32_t)err < 0) {
        SendEvent({
            .type = EventType::DeviceConnectionStateChanged,
            .value = DeviceConnectionState::NotConnected,
        });
        return absl::UnavailableError(
            fmt::format("dcamapi_init: {}", DCAMERR_ToString(err)));
    }

    //
    // 2. Open device
    //
    DCAMDEV_OPEN devopen;
    memset(&devopen, 0, sizeof(devopen));
    devopen.size = sizeof(devopen);
    devopen.index = 0;

    err = dcamdev_open(&devopen);
    if ((int32_t)err < 0) {
        SendEvent({
            .type = EventType::DeviceConnectionStateChanged,
            .value = DeviceConnectionState::NotConnected,
        });
        return absl::UnavailableError(
            fmt::format("dcamdev_open: {}", DCAMERR_ToString(err)));
    }
    hdcam = devopen.hdcam;

    //
    // 3. Populate node map
    //
    int32 iProp = 0;
    for (;;) {
        // Get next ID
        err = dcamprop_getnextid(hdcam, &iProp, DCAMPROP_OPTION_SUPPORT);
        if ((int32_t)err < 0) {
            if (err == DCAMERR_NOPROPERTY) {
                break;
            } else {
                SendEvent({
                    .type = EventType::DeviceConnectionStateChanged,
                    .value = DeviceConnectionState::NotConnected,
                });
                return absl::InternalError(
                    fmt::format("dcamprop_getnextid({:#010x}): {}", iProp,
                                DCAMERR_ToString(err)));
            }
        }

        // Name
        char name_c_str[64];
        err = dcamprop_getname(hdcam, iProp, name_c_str, sizeof(name_c_str));
        if ((int32_t)err < 0) {
            SendEvent({
                .type = EventType::DeviceConnectionStateChanged,
                .value = DeviceConnectionState::NotConnected,
            });
            return absl::InternalError(
                fmt::format("dcamprop_getname({:#010x}): {}", iProp,
                            DCAMERR_ToString(err)));
        }
        std::string name = std::string(name_c_str);

        // Attribute
        DCAMPROP_ATTR attr;
        memset(&attr, 0, sizeof(attr));
        attr.cbSize = sizeof(attr);
        attr.iProp = iProp;

        err = dcamprop_getattr(hdcam, &attr);
        if ((int32_t)err < 0) {
            SendEvent({
                .type = EventType::DeviceConnectionStateChanged,
                .value = DeviceConnectionState::NotConnected,
            });
            return absl::InternalError(
                fmt::format("dcamprop_getattr({:#010x}): {}", iProp,
                            DCAMERR_ToString(err)));
        }

        // Get string of Enum
        std::map<int32_t, std::string> enumStringFromInt;
        std::map<std::string, int32_t> enumIntFromString;

        uint8_t prop_type = attr.attribute & DCAMPROP_TYPE_MASK;
        if (prop_type == DCAMPROP_TYPE_MODE) {
            DCAMPROP_VALUETEXT valueText;
            char text_c_str[64];
            memset(&valueText, 0, sizeof(valueText));
            valueText.cbSize = sizeof(valueText);
            valueText.iProp = iProp;
            valueText.value = attr.valuemin;
            valueText.text = text_c_str;
            valueText.textbytes = sizeof(text_c_str);

            for (;;) {
                err = dcamprop_getvaluetext(hdcam, &valueText);
                if ((int32_t)err < 0) {
                    SendEvent({
                        .type = EventType::DeviceConnectionStateChanged,
                        .value = DeviceConnectionState::NotConnected,
                    });
                    return absl::InternalError(
                        fmt::format("dcamprop_getvaluetext({:#010x}, {}): {}",
                                    valueText.iProp, valueText.value,
                                    DCAMERR_ToString(err)));
                }

                int32_t enum_int = (int32_t)(valueText.value);
                std::string enum_text = std::string(text_c_str);
                enumStringFromInt[enum_int] = enum_text;
                enumIntFromString[enum_text] = enum_int;

                err = dcamprop_queryvalue(hdcam, iProp, &valueText.value,
                                          DCAMPROP_OPTION_NEXT);
                if ((int32_t)err < 0) {
                    if (err == DCAMERR_OUTOFRANGE) {
                        break;
                    } else {
                        SendEvent({
                            .type = EventType::DeviceConnectionStateChanged,
                            .value = DeviceConnectionState::NotConnected,
                        });
                        return absl::InternalError(fmt::format(
                            "dcamprop_queryvalue({:#010x}): {}",
                            valueText.iProp, DCAMERR_ToString(err)));
                    }
                }
            }
        }

        PropertyNode *node = new PropertyNode;
        node->dev = this;

        node->iProp = iProp;
        node->name = name;
        node->attribute = attr.attribute;

        node->enumStringFromInt = enumStringFromInt;
        node->enumIntFromString = enumIntFromString;

        node_map[name] = node;
    }

    //
    // 4. Get values
    //
    for (const auto &[name, node] : node_map) {
        if (node->Readable()) {
            StatusOr<std::string> value = node->GetValue();
            if (!value.ok()) {
                LOG_WARN("get value of {}: {}", name,
                         value.status().ToString());
            }
        }
    }

    connected = true;
    SendEvent({
        .type = EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::Connected,
    });
    return absl::OkStatus();
}

Status DCam::Disconnect()
{
    std::lock_guard<std::mutex> lk(hdcam_mutex);

    if (hdcam == NULL) {
        return absl::OkStatus();
    }

    SendEvent({
        .type = EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::Disconnecting,
    });

    // Close Device
    dcamdev_close(hdcam);
    hdcam = nullptr;

    // Un-init API
    dcamapi_uninit();

    connected = false;
    SendEvent({
        .type = EventType::DeviceConnectionStateChanged,
        .value = DeviceConnectionState::NotConnected,
    });
    return absl::OkStatus();
}

Status DCam::AllocBuffer(uint8_t n_buffer_frame)
{
    DCAMERR err = dcambuf_alloc(hdcam, n_buffer_frame);
    if ((int32_t)err < 0) {
        return absl::InternalError(
            fmt::format("dcambuf_alloc: {}", DCAMERR_ToString(err)));
    }
    n_buffer_frame_alloc = n_buffer_frame;

    Status status;
    status = updateWidthHeight();
    if (!status.ok()) {
        return absl::AbortedError(
            fmt::format("get width and height: {}", status.ToString()));
    }
    status = updatePixelFormat();
    if (!status.ok()) {
        return absl::AbortedError(
            fmt::format("get pixel format: {}", status.ToString()));
    }
    return absl::OkStatus();
}

Status DCam::ReleaseBuffer()
{
    DCAMERR err = dcambuf_release(hdcam);
    if ((int32_t)err < 0) {
        return absl::InternalError(
            fmt::format("dcambuf_release: {}", DCAMERR_ToString(err)));
    }
    n_buffer_frame_alloc = 0;
    return absl::OkStatus();
}

uint8_t DCam::BufferAllocated() { return n_buffer_frame_alloc; }

Status DCam::FireTrigger()
{
    DCAMERR err = dcamcap_firetrigger(hdcam);
    if ((int32_t)err < 0) {
        return absl::InternalError(
            fmt::format("dcamcap_firetrigger: {}", DCAMERR_ToString(err)));
    }
    return absl::OkStatus();
}

Status DCam::StartAcquisition()
{
    DCAMERR err;
    DCAMWAIT_OPEN waitOpen;
    memset(&waitOpen, 0, sizeof(waitOpen));
    waitOpen.size = sizeof(waitOpen);
    waitOpen.hdcam = hdcam;

    err = dcamwait_open(&waitOpen);
    if ((int32_t)err < 0) {
        return absl::InternalError(
            fmt::format("dcamwait_open: {}", DCAMERR_ToString(err)));
    }
    hwait = waitOpen.hwait;

    err = dcamcap_start(hdcam, DCAMCAP_START_SNAP);
    if ((int32_t)err < 0) {
        return absl::InternalError(fmt::format(
            "dcamcap_start(DCAMCAP_START_SNAP): {}", DCAMERR_ToString(err)));
    }
    return absl::OkStatus();
}

Status DCam::StartContinousAcquisition()
{
    DCAMERR err;
    DCAMWAIT_OPEN waitOpen;
    memset(&waitOpen, 0, sizeof(waitOpen));
    waitOpen.size = sizeof(waitOpen);
    waitOpen.hdcam = hdcam;

    err = dcamwait_open(&waitOpen);
    if ((int32_t)err < 0) {
        return absl::InternalError(
            fmt::format("dcamwait_open: {}", DCAMERR_ToString(err)));
    }
    hwait = waitOpen.hwait;

    err = dcamcap_start(hdcam, DCAMCAP_START_SEQUENCE);
    if ((int32_t)err < 0) {
        return absl::InternalError(
            fmt::format("dcamcap_start(DCAMCAP_START_SEQUENCE): {}",
                        DCAMERR_ToString(err)));
    }
    return absl::OkStatus();
}

Status DCam::StopAcquisition()
{
    DCAMERR err = dcamcap_stop(hdcam);
    if ((int32_t)err < 0) {
        return absl::InternalError(
            fmt::format("dcamcap_stop: {}", DCAMERR_ToString(err)));
    }

    if (hwait) {
        err = dcamwait_abort(hwait);
        if ((int32_t)err < 0) {
            return absl::InternalError(
                fmt::format("dcamwait_abort: {}", DCAMERR_ToString(err)));
        }

        DCAMWAIT_START waitStart;
        memset(&waitStart, 0, sizeof(waitStart));
        waitStart.size = sizeof(waitStart);
        waitStart.eventmask = DCAMWAIT_CAPEVENT_STOPPED;
        waitStart.timeout = 1000;

        err = dcamwait_start(hwait, &waitStart);
        if ((int32_t)err < 0) {
            if (err == DCAMERR_TIMEOUT) {
                return absl::DeadlineExceededError(
                    fmt::format("after waiting for CAPEVENT_STOPPED for {} ms",
                                waitStart.timeout));
            }
            return absl::InternalError(fmt::format("dcamwait_start({}): {}",
                                                   waitStart.timeout,
                                                   DCAMERR_ToString(err)));
        }

        err = dcamwait_close(hwait);
        if ((int32_t)err < 0) {
            return absl::InternalError(
                fmt::format("dcamwait_close: {}", DCAMERR_ToString(err)));
        }
        hwait = nullptr;
    }
    return absl::OkStatus();
}

Status DCam::WaitExposureEnd(uint32_t timeout_ms)
{
    DCAMERR err;
    DCAMWAIT_START waitStart;
    memset(&waitStart, 0, sizeof(waitStart));
    waitStart.size = sizeof(waitStart);
    waitStart.eventmask = DCAMWAIT_CAPEVENT_EXPOSUREEND;
    waitStart.timeout = timeout_ms;

    err = dcamwait_start(hwait, &waitStart);
    if ((int32_t)err < 0) {
        if (err == DCAMERR_TIMEOUT) {
            return absl::DeadlineExceededError("");
        }
        return absl::InternalError(
            fmt::format("dcamwait_start(EXPOSUREEND, {}): {}",
                        waitStart.timeout, DCAMERR_ToString(err)));
    }
    return absl::OkStatus();
}

Status DCam::WaitFrameReady(uint32_t timeout_ms)
{
    DCAMERR err;
    DCAMWAIT_START waitStart;
    memset(&waitStart, 0, sizeof(waitStart));
    waitStart.size = sizeof(waitStart);
    waitStart.eventmask = DCAMWAIT_CAPEVENT_FRAMEREADY;
    waitStart.timeout = timeout_ms;

    utils::StopWatch frame_ready_sw;

    err = dcamwait_start(hwait, &waitStart);
    if ((int32_t)err < 0) {
        if (err == DCAMERR_ABORT) {
            // Expected when we abort the wait when stopping acquisition
            return absl::CancelledError("dcamwait is aborted");
        }
        return absl::InternalError(
            fmt::format("dcamwait_start(FRAMEREADY, {}): {}", waitStart.timeout,
                        DCAMERR_ToString(err)));
    }
    return absl::OkStatus();
}

StatusOr<ImageData>
DCam::GetFrame(int32_t i_frame,
               std::chrono::system_clock::time_point *tp_exposure_end)
{
    DCAMBUF_FRAME dcam_frame;
    memset(&dcam_frame, 0, sizeof(dcam_frame));
    dcam_frame.size = sizeof(dcam_frame);
    dcam_frame.iFrame = i_frame;

    DCAMERR err = dcambuf_lockframe(hdcam, &dcam_frame);
    if ((int32_t)err < 0) {
        return absl::InternalError(fmt::format("dcambuf_lockframe({}): {}",
                                               i_frame, DCAMERR_ToString(err)));
    }

    ImageData frame(dcam_frame.height, dcam_frame.width, dtype, ctype);
    size_t dcam_buf_size = dcam_frame.rowbytes * dcam_frame.height;
    if (frame.BufSize() != dcam_buf_size) {
        return absl::InternalError(
            fmt::format("buffer size mismatch: calculated {}, DCAM reported {}",
                        frame.BufSize(), dcam_buf_size));
    }
    std::memcpy(frame.Buf().get(), dcam_frame.buf, frame.BufSize());

    if (tp_exposure_end != nullptr) {
        *tp_exposure_end =
            std::chrono::system_clock::from_time_t(
                (time_t)(dcam_frame.timestamp.sec)) +
            std::chrono::microseconds(dcam_frame.timestamp.microsec);
    }
    return frame;
}

Status DCam::updateWidthHeight()
{
    StatusOr<std::string> width_str = Node("IMAGE WIDTH")->GetValue();
    if (!width_str.ok()) {
        return width_str.status();
    }
    width = std::stoi(width_str.value());

    StatusOr<std::string> height_str = Node("IMAGE HEIGHT")->GetValue();
    if (!height_str.ok()) {
        return height_str.status();
    }
    height = std::stoi(height_str.value());

    return absl::OkStatus();
}

Status DCam::updatePixelFormat()
{
    StatusOr<std::string> color_type = Node("COLORTYPE")->GetValue();
    if (!color_type.ok()) {
        return color_type.status();
    }

    StatusOr<std::string> bit_per_channel_str =
        Node("BIT PER CHANNEL")->GetValue();
    if (!bit_per_channel_str.ok()) {
        return bit_per_channel_str.status();
    }
    int bit_per_channel = std::stoi(bit_per_channel_str.value());

    if (color_type.value() == "B/W") {
        switch (bit_per_channel) {
        case 8:
            dtype = DataType::Uint8;
            ctype = ColorType::Mono8;
            break;
        case 10:
            dtype = DataType::Uint16;
            ctype = ColorType::Mono10;
            break;
        case 12:
            dtype = DataType::Uint16;
            ctype = ColorType::Mono12;
            break;
        case 14:
            dtype = DataType::Uint16;
            ctype = ColorType::Mono14;
            break;
        case 16:
            dtype = DataType::Uint16;
            ctype = ColorType::Mono16;
            break;
        default:
            return absl::UnimplementedError(fmt::format(
                "BIT PER CHANNEL={} is not supported", bit_per_channel));
        }
    } else {
        return absl::UnimplementedError(
            fmt::format("COLORTYPE {} is not supported", color_type.value()));
    }
    return absl::OkStatus();
}

::PropertyNode *DCam::Node(std::string name)
{
    auto it = node_map.find(name);
    if (it == node_map.end()) {
        return nullptr;
    }
    return it->second;
}

std::map<std::string, ::PropertyNode *> DCam::NodeMap()
{
    std::map<std::string, ::PropertyNode *> base_node_map;
    for (auto &[name, node] : node_map) {
        base_node_map[name] = node;
    }
    return base_node_map;
}

std::string PropertyNode::Description()
{
    std::string type_str;
    uint8_t prop_type = attribute & DCAMPROP_TYPE_MASK;
    switch (prop_type) {
    case DCAMPROP_TYPE_NONE:
        type_str = "NONE";
    case DCAMPROP_TYPE_MODE:
        type_str = "MODE";
    case DCAMPROP_TYPE_LONG:
        type_str = "LONG";
    case DCAMPROP_TYPE_REAL:
        type_str = "REAL";
    default:
        type_str = fmt::format("<UnknownType({})>", prop_type);
    }

    std::string attr_str;
    if (attribute & DCAMPROP_ATTR_READABLE) {
        attr_str += "READABLE";
    }
    if (attribute & DCAMPROP_ATTR_WRITABLE) {
        attr_str += "WRITABLE";
    }
    if (attribute & DCAMPROP_ATTR_AUTOROUNDING) {
        attr_str += " AUTOROUNDING";
    }
    if (attribute & DCAMPROP_ATTR_STEPPING_INCONSISTENT) {
        attr_str += " STEPPING_INCONSISTENT";
    }
    if (attribute & DCAMPROP_ATTR_VOLATILE) {
        attr_str += " VOLATILE";
    }
    if (attribute & DCAMPROP_ATTR_DATASTREAM) {
        attr_str += " DATASTREAM";
    }
    if (attribute & DCAMPROP_ATTR_ACCESSREADY) {
        attr_str += " ACCESSREADY";
    }
    if (attribute & DCAMPROP_ATTR_ACCESSBUSY) {
        attr_str += " ACCESSBUSY";
    }

    std::vector<std::string> description;
    description.push_back(fmt::format("Type: {}", type_str));
    description.push_back(fmt::format("Attribute: {}", attr_str));

    return fmt::format("{}", fmt::join(description, "\n"));
}

bool PropertyNode::Readable() { return (attribute & DCAMPROP_ATTR_READABLE); }

bool PropertyNode::Writeable()
{
    if (!(attribute & DCAMPROP_ATTR_WRITABLE)) {
        return false;
    }
    if ((attribute & DCAMPROP_ATTR_ACCESSREADY) &&
        (attribute & DCAMPROP_ATTR_ACCESSBUSY))
    {
        return true;
    }

    DCAMERR err;
    int32 status;
    err = dcamcap_status(dev->hdcam, &status);
    if ((int32_t)err < 0) {
        // Do no throw exception, user will get error when setting the property
        // if it is actually not writeable
        LOG_ERROR("dcamcap_status: {}", DCAMERR_ToString(err));
        return true;
    }
    switch (status) {
    case DCAMCAP_STATUS_BUSY:
        return (attribute & DCAMPROP_ATTR_ACCESSBUSY);
    case DCAMCAP_STATUS_READY:
        return (attribute & DCAMPROP_ATTR_ACCESSREADY);
    default:
        return true;
    }
}

std::vector<std::string> PropertyNode::Options()
{
    if (!enumIntFromString.empty()) {
        std::vector<std::string> enum_string_list;
        for (const auto &[enum_string, enum_int] : enumIntFromString) {
            enum_string_list.push_back(enum_string);
        }
        return enum_string_list;
    }
    return {};
}

StatusOr<std::string> PropertyNode::GetValue()
{
    DCAMERR err;
    double dcam_value;

    err = dcamprop_getvalue(dev->hdcam, iProp, &dcam_value);
    if ((int32_t)err < 0) {
        return absl::InternalError(fmt::format("dcamprop_getvalue({:#010x})",
                                               iProp, DCAMERR_ToString(err)));
    }

    std::string value;
    uint8_t prop_type = attribute & DCAMPROP_TYPE_MASK;
    if (prop_type == DCAMPROP_TYPE_MODE) {
        auto it = enumStringFromInt.find(int32_t(dcam_value));
        if (it == enumStringFromInt.end()) {
            value = fmt::format("ENUM_{:d}", dcam_value);
        }
        return it->second;
    } else if (prop_type == DCAMPROP_TYPE_LONG) {
        value = fmt::format("{:d}", (int32_t)dcam_value);
    } else if (prop_type == DCAMPROP_TYPE_REAL) {
        value = fmt::format("{:g}", dcam_value);
    } else {
        return absl::UnimplementedError(
            fmt::format("unexpected property type {} with value {}", prop_type,
                        dcam_value));
    }

    handleValueUpdate(value);
    return value;
}

Status PropertyNode::SetValue(std::string value)
{
    double dcam_value;

    uint8_t prop_type = attribute & DCAMPROP_TYPE_MASK;
    if (prop_type == DCAMPROP_TYPE_MODE) {
        auto it = enumIntFromString.find(value);
        if (it == enumIntFromString.end()) {
            return absl::InvalidArgumentError("invalid enumerate value");
        }
        dcam_value = it->second;
    } else if (prop_type == DCAMPROP_TYPE_LONG) {
        dcam_value = std::stod(value);
    } else if (prop_type == DCAMPROP_TYPE_REAL) {
        dcam_value = std::stod(value);
    } else {
        return absl::UnimplementedError(
            fmt::format("unexpected property type {}", prop_type));
    }

    DCAMERR err;
    if (attribute & DCAMPROP_ATTR_AUTOROUNDING) {
        if ((prop_type != DCAMPROP_TYPE_LONG) &&
            (prop_type != DCAMPROP_TYPE_REAL))
        {
            return absl::UnimplementedError(fmt::format(
                "unexpected property type {} with AUTOROUNDING attribute",
                prop_type));
        }
        err = dcamprop_setgetvalue(dev->hdcam, iProp, &dcam_value);
        if ((int32_t)err < 0) {
            return absl::InternalError(
                fmt::format("dcamprop_setgetvalue({:#010x}, {:g}): {}", iProp,
                            dcam_value, DCAMERR_ToString(err)));
        }
        if (prop_type == DCAMPROP_TYPE_LONG) {
            value = fmt::format("{:d}", (int32_t)dcam_value);
        } else if (prop_type == DCAMPROP_TYPE_REAL) {
            value = fmt::format("{:g}", dcam_value);
        }
    } else {
        err = dcamprop_setvalue(dev->hdcam, iProp, dcam_value);
        if ((int32_t)err < 0) {
            return absl::InternalError(
                fmt::format("dcamprop_setvalue({:#010x}, {:g}): {}", iProp,
                            dcam_value, DCAMERR_ToString(err)));
        }
    }

    handleValueUpdate(value);
    return absl::OkStatus();
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

    if (value_changed) {
        dev->SendEvent({.type = EventType::DevicePropertyValueUpdate,
                        .path = name,
                        .value = value});
    }
}

} // namespace Hamamatsu
