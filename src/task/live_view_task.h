#ifndef LIVE_VIEW_TASK_H
#define LIVE_VIEW_TASK_H

#include "device/devicehub.h"
#include "device/hamamatsu/hamamatsu_dcam.h"
#include "eventstream.h"
#include "image/imagemanager.h"
#include "image/imagedata.h"

class LiveViewTask : public EventSender {
public:
    LiveViewTask(DeviceHub *hub, Hamamatsu::DCam *dcam,
                 ImageManager *image_manager)
    {
        this->hub = hub;
        this->dcam = dcam;
        this->image_manager = image_manager;
    }

    bool IsRunning()
    {
        return is_running;
    }
    void Run();
    void Stop();

protected:
    void PrepareBuffer();
    Status StartAcqusition();
    ImageData GetFrame();
    Status StopAcqusition();

private:
    DeviceHub *hub;
    Hamamatsu::DCam *dcam;
    ImageManager *image_manager;

    std::atomic<bool> is_running = false;
    std::string task_name = "LiveView";
};

#endif