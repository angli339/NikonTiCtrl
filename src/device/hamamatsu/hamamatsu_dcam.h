#ifndef DEVICE_HAMAMATSU_DCAM
#define DEVICE_HAMAMATSU_DCAM

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>

#include <absl/status/status.h>
#include <absl/status/statusor.h>

#include "data/imagedata.h"
#include "device/device.h"

// dcamapi4.h
typedef struct tag_dcam *HDCAM;
typedef struct DCAMWAIT *HDCAMWAIT;

namespace Hamamatsu {

using absl::Status;
using absl::StatusOr;

class DCam;
class PropertyNode;

class DCam : public Device {
    friend class PropertyNode;

public:
    ~DCam();

    bool DetectDevice();
    Status Connect() override;
    Status Disconnect() override;
    bool IsConnected() override
    {
        return connected;
    }

    ::PropertyNode *Node(std::string name) override;
    std::map<std::string, ::PropertyNode *> NodeMap() override;

    Status AllocBuffer(uint8_t n_frames);
    Status ReleaseBuffer();
    uint8_t BufferAllocated();

    DataType GetDataType()
    {
        return dtype;
    }
    ColorType GetColorType()
    {
        return ctype;
    }
    uint32_t GetWidth()
    {
        return width;
    }
    uint32_t GetHeight()
    {
        return height;
    }

    Status StartAcquisition();
    Status StartContinousAcquisition();
    Status StopAcquisition();
    Status WaitExposureEnd(uint32_t timeout_ms);
    Status WaitFrameReady(uint32_t timeout_ms);
    StatusOr<ImageData>
    GetFrame(int32_t i_frame,
             std::chrono::system_clock::time_point *tp_exposure_end = nullptr);

    Status FireTrigger();

private:
    std::mutex hdcam_mutex;
    HDCAM hdcam = nullptr;

    std::atomic<bool> connected = false;
    std::map<std::string, PropertyNode *> node_map;

    HDCAMWAIT hwait = nullptr;
    uint8_t n_buffer_frame_alloc = 0;

    Status updateWidthHeight();
    Status updatePixelFormat();
    uint32_t width;
    uint32_t height;
    DataType dtype;
    ColorType ctype;
};

class PropertyNode : public ::PropertyNode {
    friend class DCam;

public:
    virtual std::string Name() override
    {
        return name;
    }
    virtual std::string Description() override;
    virtual bool Valid() override
    {
        return dev->IsConnected();
    }
    virtual bool Readable() override;
    virtual bool Writeable() override;
    virtual std::vector<std::string> Options() override;

    virtual StatusOr<std::string> GetValue() override;
    virtual Status SetValue(std::string value) override;
    Status WaitFor(std::chrono::milliseconds timeout) override;
    Status WaitUntil(std::chrono::steady_clock::time_point timepoint) override;

    virtual std::optional<std::string> GetSnapshot() override;

private:
    DCam *dev;

    int32_t iProp;
    std::string name;
    uint32_t attribute;

    std::map<int32_t, std::string> enumStringFromInt;
    std::map<std::string, int32_t> enumIntFromString;

    void handleValueUpdate(std::string value);

    std::shared_mutex mutex_snapshot;
    std::optional<std::string> snapshot_value;
    std::chrono::steady_clock::time_point snapshot_timepoint;
};

} // namespace Hamamatsu

#endif
