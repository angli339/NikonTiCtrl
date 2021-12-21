#ifndef LIVE_VIEW_TASK_H
#define LIVE_VIEW_TASK_H

#include "device/devicehub.h"
#include "device/hamamatsu/hamamatsu_dcam.h"
#include "eventstream.h"
#include "data/datamanager.h"
#include "data/imagedata.h"

class LiveViewTask : public EventSender {
public:
    LiveViewTask(DeviceHub *hub, Hamamatsu::DCam *dcam,
                 DataManager *data_manager)
    {
        this->hub = hub;
        this->dcam = dcam;
        this->data_manager = data_manager;
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
    DataManager *data_manager;

    std::atomic<bool> is_running = false;
    std::string task_name = "LiveView";
};

#endif