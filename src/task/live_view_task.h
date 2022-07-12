#ifndef LIVE_VIEW_TASK_H
#define LIVE_VIEW_TASK_H

#include "device/devicehub.h"
#include "device/hamamatsu/hamamatsu_dcam.h"
#include "eventstream.h"
#include "image/imagemanager.h"
#include "image/imagedata.h"

class ExperimentControl;

class LiveViewTask : public EventSender {
public:
    LiveViewTask(ExperimentControl *exp);
    bool IsRunning();
    void Run();
    void Stop();

protected:
    void PrepareBuffer();
    Status StartAcqusition();
    ImageData GetFrame();
    Status StopAcqusition();

private:
    ExperimentControl *exp;
    Hamamatsu::DCam *dcam;

    std::atomic<bool> is_running = false;
    std::string task_name = "LiveView";
};

#endif