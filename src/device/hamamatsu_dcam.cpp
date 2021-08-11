#include "hamamatsu_dcam.h"

#include <cstdint>
#include <stdexcept>
#include <unordered_map>

#include "logger.h"
#include "utils/wmi.h"

#include "dcamprop.h"
#include "dcamapi4.h"

struct NumRange {
    double Min;
    double Max;
    double Step;
};

typedef enum {
    TypeNone = DCAMPROP_TYPE_NONE,
    TypeEnum = DCAMPROP_TYPE_MODE,
    TypeInt = DCAMPROP_TYPE_LONG,
    TypeFloat = DCAMPROP_TYPE_REAL,
} PropType;

struct HamamatsuPropInfo {
    int32 ID;
    std::string Name;
    PropType Type;

    bool Writable;
    bool Readable;
    bool AccessReady;
    bool AccessBusy;
    bool Autorounding;
    bool Volatile;

    struct NumRange *NumRange;
    std::unordered_map<std::string, double> *EnumValues;
};

std::unordered_map<std::string, double> *DCAMGetEnumValues(HDCAM hdcam, int32_t iProp, int32_t valuemin) {
    auto enumValues = new std::unordered_map<std::string, double>;

    DCAMERR err;
    DCAMPROP_VALUETEXT valueText;
    char text[64];
    memset(&valueText, 0, sizeof(valueText));
    valueText.cbSize = sizeof(valueText);
    valueText.iProp = iProp;
    valueText.value = valuemin;
    valueText.text = text;
    valueText.textbytes = sizeof(text);

    for (;;) {
        err = dcamprop_getvaluetext(hdcam, &valueText);
        if ((int32_t)err < 0) {
            throw std::runtime_error(fmt::format("dcamprop_getvaluetext failed: {:#10x}", uint32_t(err)));
        }

        enumValues->insert({std::string(text), valueText.value});

        err = dcamprop_queryvalue(hdcam, iProp, &valueText.value, DCAMPROP_OPTION_NEXT);
        if ((int32_t)err < 0) {
            if (err != DCAMERR_OUTOFRANGE) {
                throw std::runtime_error(fmt::format("dcamprop_queryvalue failed after {}, err={:#10x}", valueText.value, uint32_t(err)));
            }
            break;
        }
    }
    return enumValues;
}

HamamatsuDCAM::HamamatsuDCAM(QObject *parent) : QObject(parent)
{
}

HamamatsuDCAM::~HamamatsuDCAM()
{
    if (connected) {
        disconnect();
    }
}

bool HamamatsuDCAM::detectDevice() {
    WMI w;
    std::vector<std::string> list1394 = w.list1394DeviceID("HAMAMATSU");
    return list1394.size() != 0;
}

void HamamatsuDCAM::connect()
{
    LOG_INFO("HamamatsuDCAM: detecting devices with DCAMAPI");
    emit propertyUpdated("", "Connecting");
    utils::StopWatch sw;

    DCAMERR err;
    DCAMAPI_INIT apiinit;
    memset(&apiinit, 0, sizeof(apiinit));
    apiinit.size = sizeof(apiinit);

    err = dcamapi_init(&apiinit);
    if ((int32_t)err < 0) {
        if (err == DCAMERR_NOCAMERA) {
            LOG_INFO("HamamatsuDCAM: no camera detected.");
            return;
        } else {
            LOG_ERROR("HamamatsuDCAM: dcamapi_init failed {:#010x}", uint32_t(err));
        }
    }
    LOG_INFO("HamamatsuDCAM: found {} devices in {:.3f}ms", apiinit.iDeviceCount, sw.Milliseconds());

    //
    // Open Device
    //

    LOG_INFO("HamamatsuDCAM: connecting...");
    sw.Reset();

    DCAMDEV_OPEN devopen;
    memset(&devopen, 0, sizeof(devopen));
    devopen.size = sizeof(devopen);
    devopen.index = 0;

    err = dcamdev_open(&devopen);
    if ((int32_t)err < 0) {
        LOG_ERROR("HamamatsuDCAM: dcamdev_open failed ({:#010x})", uint32_t(err));
    }

    hdcam = devopen.hdcam;
    LOG_INFO("HamamatsuDCAM: connected in {:.3f}ms", sw.Milliseconds());

    //
    // Enumerate Properties
    //

    LOG_INFO("HamamatsuDCAM: enumerating properties...");
    sw.Reset();

    int32 iProp = 0;
    for (;;) {
        err = dcamprop_getnextid(hdcam, &iProp, DCAMPROP_OPTION_SUPPORT);
        if ((int32_t)err < 0) {
            if (err != DCAMERR_NOPROPERTY) {
                LOG_ERROR("HamamatsuDCAM: dcamprop_getnextid failed after {:#010x} ({:#010x})", iProp, uint32_t(err));
                break;
            }
            break;
        }

        char name[64];
        err = dcamprop_getname(hdcam, iProp, name, sizeof(name));
        if ((int32_t)err < 0) {
            LOG_ERROR("HamamatsuDCAM: dcamprop_getname({:#010x}) failed ({:#010x})", iProp, uint32_t(err));
            break;
        }

        DCAMPROP_ATTR attr;
        memset(&attr, 0, sizeof(attr));
        attr.cbSize = sizeof(attr);
        attr.iProp = iProp;

        err = dcamprop_getattr(hdcam, &attr);
        if ((int32_t)err < 0) {
            LOG_ERROR("HamamatsuDCAM: dcamprop_getattr({:#010x}) failed ({:#010x})", iProp, err);
            break;
        }

        struct HamamatsuPropInfo *info = new(struct HamamatsuPropInfo);
        memset(info, 0, sizeof(struct HamamatsuPropInfo));
        info->ID = iProp;
        info->Name = std::string(name);

        uint8_t propType = attr.attribute & DCAMPROP_TYPE_MASK;
        if ((propType != DCAMPROP_TYPE_NONE)
            && (propType != DCAMPROP_TYPE_MODE)
            && (propType != DCAMPROP_TYPE_LONG)
            && (propType != DCAMPROP_TYPE_REAL)) {
            LOG_ERROR("HamamatsuDCAM: iProp={:#010x} has invalid type={:#04x}", iProp, propType);
            break;
        }
        info->Type = (PropType)propType;

        info->Readable = attr.attribute & DCAMPROP_ATTR_READABLE;
        info->Writable = attr.attribute & DCAMPROP_ATTR_WRITABLE;
        info->AccessReady = attr.attribute & DCAMPROP_ATTR_ACCESSREADY;
        info->AccessBusy = attr.attribute & DCAMPROP_ATTR_ACCESSBUSY;
        info->Autorounding = attr.attribute & DCAMPROP_ATTR_AUTOROUNDING;
        info->Volatile = attr.attribute & DCAMPROP_ATTR_VOLATILE;

        if (attr.attribute & DCAMPROP_ATTR_HASRANGE) {
            struct NumRange *range = new struct NumRange;
            range->Min = attr.valuemin;
            range->Max = attr.valuemax;
            range->Step = attr.valuestep;

            info->NumRange = range;
        }

        if (info->Type == TypeEnum) {
            info->EnumValues = DCAMGetEnumValues(hdcam, iProp, attr.valuemin);
        }

        propInfo.insert({info->Name, info});
    }

    connected = true;
    LOG_INFO("HamamatsuDCAM: property enumeration completed in {:.3f}ms", sw.Milliseconds());
    emit propertyUpdated("", "Connected");

//    for(const auto& [name, info] : propInfo) {
//        qDebug("%s: 0x%08ld", name.c_str(), info->ID);
//        if (info->EnumValues) {
//            for (const auto& [enumName, enumValue] : *info->EnumValues) {
//                qDebug("    %s(%g)", enumName.c_str(), enumValue);
//            }
//        }
//    }
}

void HamamatsuDCAM::disconnect()
{
    if (hdcam == 0) {
        return;
    }

    LOG_INFO("HamamatsuDCAM: disconnecting...");
    emit propertyUpdated("", "Disconnected");
    utils::StopWatch sw;

    for(auto& [name, info] : propInfo) {
        if (info->NumRange) {
            delete info->NumRange;
        }
        if (info->EnumValues) {
            delete info->EnumValues;
        }
        delete info;
    }

    if (hdcam != nullptr) {
        dcamdev_close(hdcam);
        hdcam = nullptr;
    }
    dcamapi_uninit();

    connected = false;

    LOG_INFO("HamamatsuDCAM: disconnected in {:.3f}ms", sw.Milliseconds());
    emit propertyUpdated("", "Disconnected");
}

std::vector<std::string> HamamatsuDCAM::listDeviceProperty()
{
    std::vector<std::string> propertyList;
    propertyList.reserve(propInfo.size());

    for (const auto& kv : propInfo) {
        propertyList.push_back(kv.first);
    }

    return propertyList;
}

std::string HamamatsuDCAM::getDeviceProperty(const std::string name, bool /*force_update*/)
{
    if (name == "") {
        if (connected) {
            return "Connected";
        }
        return "Disconnected";
    }

    auto kv = propInfo.find(name);
    if (kv == propInfo.end()) {
        throw std::invalid_argument("invalid name");
    }
    auto info = kv->second;

    DCAMERR err;
    int32 iProp = info->ID;
    double value;

    slog::Fields log_fields;
    log_fields["api_call"] = fmt::format("dcamprop_getvalue({}({:#010x}))", name, uint32_t(iProp));
    
    utils::StopWatch sw_api_call;
    err = dcamprop_getvalue(hdcam, iProp, &value);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    log_fields["response"] = value;
    
    if ((int32_t)err < 0) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(err));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("dcamprop_getvalue failed: {:#10x}", uint32_t(err)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }

    if (info->Type == TypeFloat) {
        return std::to_string(value);
    }
    if (info->Type == TypeInt) {
        return std::to_string(int32(value));
    }
    if (info->Type == TypeEnum) {
        for (const auto& [enumName, enumValue] : *info->EnumValues) {
            if (enumValue == value) {
                return enumName;
            }
        }
        return "ENUM_" + std::to_string(value);
    }
    throw std::runtime_error("getDeviceProperty: invalid type");
}

void HamamatsuDCAM::setDeviceProperty(const std::string name, const std::string value)
{
    auto kv = propInfo.find(name);
    if (kv == propInfo.end()) {
        throw std::invalid_argument("invalid name");
    }
    auto info = kv->second;

    DCAMERR err;
    int32 iProp = info->ID;
    double floatValue;
    if (info->Type == TypeFloat) {
        floatValue = stof(value);
    } else if (info->Type == TypeInt) {
        floatValue = (double)stoi(value);
    } else if (info->Type == TypeEnum) {
        auto kvEnum = info->EnumValues->find(value);
        if (kvEnum == info->EnumValues->end()) {
            throw std::invalid_argument("invalid enum value");
        }
        floatValue = kvEnum->second;
    } else {
        throw std::runtime_error("setDeviceProperty: invalid type");
    }

    slog::Fields log_fields;
    log_fields["api_call"] = fmt::format("dcamprop_setvalue({}({:#010x}))", name, uint32_t(iProp));
    log_fields["request"] = floatValue;

    utils::StopWatch sw_api_call;
    err = dcamprop_setvalue(hdcam, iProp, floatValue);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    if ((int32_t)err < 0) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(err));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("dcamprop_setvalue failed: {:#10x}", uint32_t(err)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }

    return;
}

void HamamatsuDCAM::setBitsPerChannel(uint8_t bits_per_pixel)
{
    DCAMERR err;
    err = dcamprop_setvalue(hdcam, DCAM_IDPROP_BITSPERCHANNEL, (double)bits_per_pixel);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("dcamprop_setvalue failed: {:#10x}", uint32_t(err)));
    }
    return;
}

uint8_t HamamatsuDCAM::getBitsPerChannel()
{
    DCAMERR err;
    double floatValue;
    err = dcamprop_getvalue(hdcam, DCAM_IDPROP_BITSPERCHANNEL, &floatValue);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("dcamprop_getvalue failed: {:#10x}", uint32_t(err)));
    }
    return (uint8_t)floatValue;
}

uint8_t HamamatsuDCAM::getNumChannels()
{
    DCAMERR err;
    double floatValue;
    err = dcamprop_getvalue(hdcam, DCAM_IDPROP_NUMBEROF_CHANNEL, &floatValue);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("dcamprop_getvalue failed: {:#10x}", uint32_t(err)));
    }
    return (uint8_t)floatValue;
}

uint8_t HamamatsuDCAM::getBytesPerPixel()
{
    DCAMERR err;
    double floatValue;
    err = dcamprop_getvalue(hdcam, DCAM_IDPROP_IMAGE_PIXELTYPE, &floatValue);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("dcamprop_getvalue failed: {:#10x}", uint32_t(err)));
    }
    switch ((uint8_t)floatValue) {
    case DCAM_PIXELTYPE_MONO8:
        return 1;
    case DCAM_PIXELTYPE_MONO16:
        return 2;
    case DCAM_PIXELTYPE_RGB24:
    case DCAM_PIXELTYPE_BGR24:
        return 3;
    case DCAM_PIXELTYPE_RGB48:
    case DCAM_PIXELTYPE_BGR48:
        return 6;
    }
    throw std::runtime_error(fmt::format("dcamprop_getvalue DCAM_IDPROP_IMAGE_PIXELTYPE got unexpected response: {}", floatValue));
}

uint16_t HamamatsuDCAM::getImageWidth()
{
    DCAMERR err;
    double floatValue;
    err = dcamprop_getvalue(hdcam, DCAM_IDPROP_IMAGE_WIDTH, &floatValue);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("dcamprop_getvalue failed: {:#10x}", uint32_t(err)));
    }
    return (uint16_t)floatValue;
}

uint16_t HamamatsuDCAM::getImageHeight()
{
    DCAMERR err;
    double floatValue;
    err = dcamprop_getvalue(hdcam, DCAM_IDPROP_IMAGE_HEIGHT, &floatValue);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("dcamprop_getvalue failed: {:#10x}", uint32_t(err)));
    }
    return (uint16_t)floatValue;
}

void HamamatsuDCAM::setExposure(double exposure_ms)
{
    double exposure_s = exposure_ms/1000.0;
    DCAMERR err;

    slog::Fields log_fields;
    log_fields["api_call"] = "dcamprop_setvalue(DCAM_IDPROP_EXPOSURETIME)";
    log_fields["request"] = exposure_s;

    utils::StopWatch sw_api_call;
    err = dcamprop_setvalue(hdcam, DCAM_IDPROP_EXPOSURETIME, exposure_s);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    if ((int32_t)err < 0) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(err));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("dcamprop_setvalue failed: {:#10x}", uint32_t(err)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }
    return;
}

double HamamatsuDCAM::getExposure()
{
    DCAMERR err;
    double floatValue;
    err = dcamprop_getvalue(hdcam, DCAM_IDPROP_EXPOSURETIME, &floatValue);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("getExposure: dcamprop_getvalue failed: {:#10x}", uint32_t(err)));
    }
    return floatValue * 1000.0;
}

bool HamamatsuDCAM::isBusy() {
    DCAMERR err;
    int32 status;
    err = dcamcap_status(hdcam, &status);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("isBusy: dcamcap_status failed: {:#10x}", uint32_t(err)));
    }

    return (status == DCAMCAP_STATUS_BUSY);
}

void HamamatsuDCAM::allocBuffer(int n_frame)
{
    if (bufferSize != 0) {
        releaseBuffer();
    }

    std::lock_guard<std::mutex> lk(hdcam_mutex);

    slog::Fields log_fields;
    log_fields["api_call"] = fmt::format("dcambuf_alloc({})", n_frame);

    utils::StopWatch sw_api_call;
    DCAMERR err = dcambuf_alloc(hdcam, n_frame);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    if ((int32_t)err < 0) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(err));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("allocBuffer: dcambuf_alloc failed: {:#10x}", uint32_t(err)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }
    bufferSize = n_frame;
    return;
}


void HamamatsuDCAM::releaseBuffer()
{
    slog::Fields log_fields;
    log_fields["api_call"] = "dcambuf_release()";
    LOGFIELDS_TRACE(log_fields, "calling dcambuf_release()");

    std::lock_guard<std::mutex> lk(hdcam_mutex);

    utils::StopWatch sw_api_call;
    DCAMERR err = dcambuf_release(hdcam);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    if ((int32_t)err < 0) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(err));
        LOGFIELDS_ERROR(log_fields, "dcambuf_release() failed");
        throw std::runtime_error(fmt::format("releaseBuffer: dcambuf_release failed: {:#10x}", uint32_t(err)));
    } else {
        LOGFIELDS_TRACE(log_fields, "dcambuf_release() completed");
    }
    bufferSize = 0;
}

void HamamatsuDCAM::startAcquisition()
{
    slog::Fields log_fields;
    log_fields["api_call"] = "dcamcap_start(DCAMCAP_START_SNAP)";

    std::lock_guard<std::mutex> lk(hdcam_mutex);

    utils::StopWatch sw_api_call;
    DCAMERR err = dcamcap_start(hdcam, DCAMCAP_START_SNAP);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    if ((int32_t)err < 0) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(err));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("startAcquisition: dcamcap_start failed: {:#10x}", uint32_t(err)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }
    return;
}

void HamamatsuDCAM::startContinuousAcquisition()
{
    slog::Fields log_fields;
    log_fields["api_call"] = "dcamcap_start(DCAMCAP_START_SEQUENCE)";

    std::lock_guard<std::mutex> lk(hdcam_mutex);

    utils::StopWatch sw_api_call;
    DCAMERR err = dcamcap_start(hdcam, DCAMCAP_START_SEQUENCE);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    if ((int32_t)err < 0) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(err));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("startContinuousAcquisition: dcamcap_start failed: {:#10x}", uint32_t(err)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }
    return;
}

void HamamatsuDCAM::stopAcquisition()
{
    slog::Fields log_fields;
    log_fields["api_call"] = "dcamcap_stop()";

    std::lock_guard<std::mutex> lk(hdcam_mutex);

    utils::StopWatch sw_api_call;
    DCAMERR err = dcamcap_stop(hdcam);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    if ((int32_t)err < 0) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(err));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("stopAcquisition: dcamcap_stop failed: {:#10x}", uint32_t(err)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }
    if (pending_hwait != nullptr) {
        err = dcamwait_abort(pending_hwait);
        if ((int32_t)err < 0) {
            throw std::runtime_error(fmt::format("stopAcquisition: dcamwait_abort failed: {:#10x}", uint32_t(err)));
        }
    }
    return;
}

bool HamamatsuDCAM::isAcquisitionReady()
{
    DCAMERR err;
    int32 status;
    err = dcamcap_status(hdcam, &status);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("isAcquisitionReady: dcamcap_status failed: {:#10x}", uint32_t(err)));
    }
    return (status == DCAMCAP_STATUS_READY);
}

bool HamamatsuDCAM::isAcquisitionRunning()
{
    DCAMERR err;
    int32 status;
    err = dcamcap_status(hdcam, &status);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("dcamcap_status failed: {:#10x}", uint32_t(err)));
    }
    return (status == DCAMCAP_STATUS_BUSY);
}

void HamamatsuDCAM::waitExposureEnd(int32_t timeout_ms) {
    DCAMERR err;
    DCAMWAIT_OPEN waitOpen;
    memset(&waitOpen, 0, sizeof(waitOpen));
    waitOpen.size = sizeof(waitOpen);
    waitOpen.hdcam=hdcam;

    err = dcamwait_open(&waitOpen);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("waitExposureEnd: dcamwait_open failed: {:#10x}", uint32_t(err)));
    }

    DCAMWAIT_START waitStart;
    memset(&waitStart, 0, sizeof(waitStart));
    waitStart.size = sizeof(waitStart);
    waitStart.eventmask = DCAMWAIT_CAPEVENT_EXPOSUREEND;
    waitStart.timeout = timeout_ms;

    err = dcamwait_start(waitOpen.hwait, &waitStart);
    if ((int32_t)err < 0) {
        DCAMERR err_close = dcamwait_close(waitOpen.hwait);
        if ((int32_t)err_close < 0) {
             throw std::runtime_error(fmt::format("waitExposureEnd: dcamwait_start failed: {:#10x}. dcamwait_close failed: {:#10x}", uint32_t(err), uint32_t(err_close)));
        }
        throw std::runtime_error(fmt::format("waitExposureEnd: dcamwait_start failed: {:#10x}, dcamwait_close completed", uint32_t(err)));
    }
    pending_hwait = waitOpen.hwait;

    err = dcamwait_close(waitOpen.hwait);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("waitExposureEnd: dcamwait_close failed: {:#10x}", uint32_t(err)));
    }
    pending_hwait = nullptr;
}

void HamamatsuDCAM::waitFrameReady(int32_t timeout_ms) {
    DCAMERR err;
    DCAMWAIT_OPEN waitOpen;
    memset(&waitOpen, 0, sizeof(waitOpen));
    waitOpen.size = sizeof(waitOpen);
    waitOpen.hdcam=hdcam;

    err = dcamwait_open(&waitOpen);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("waitFrameReady: dcamwait_open failed: {:#10x}", uint32_t(err)));
    }

    DCAMWAIT_START waitStart;
    memset(&waitStart, 0, sizeof(waitStart));
    waitStart.size = sizeof(waitStart);
    waitStart.eventmask = DCAMWAIT_CAPEVENT_FRAMEREADY;
    waitStart.timeout = timeout_ms;

    err = dcamwait_start(waitOpen.hwait, &waitStart);
    if ((int32_t)err < 0) {
        DCAMERR err_close = dcamwait_close(waitOpen.hwait);
        if ((int32_t)err_close < 0) {
             throw std::runtime_error(fmt::format("waitFrameReady: dcamwait_start failed: {:#10x}. dcamwait_close failed: {:#10x}", uint32_t(err), uint32_t(err_close)));
        }
        throw std::runtime_error(fmt::format("waitFrameReady: dcamwait_start failed: {:#10x}, dcamwait_close completed", uint32_t(err)));
    }
    pending_hwait = waitOpen.hwait;

    err = dcamwait_close(waitOpen.hwait);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("waitFrameReady: dcamwait_close failed: {:#10x}", uint32_t(err)));
    }
    pending_hwait = nullptr;
}

uint8_t *HamamatsuDCAM::getFrame()
{
    DCAMERR err;
    DCAMBUF_FRAME frame;
    memset(&frame, 0, sizeof(frame));
    frame.size = sizeof(frame);
    frame.iFrame = 0;

    err = dcambuf_lockframe(hdcam, &frame);
    if ((int32_t)err < 0) {
        throw std::runtime_error(fmt::format("getFrame: dcambuf_lockframe failed: {:#10x}", uint32_t(err)));
    }

    return (uint8_t *)frame.buf;
}
