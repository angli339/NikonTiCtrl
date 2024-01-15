#ifndef MULTI_CHANNEL_TASK_H
#define MULTI_CHANNEL_TASK_H

#include <filesystem>
#include <future>
#include <map>
#include <string>

#include "channel.h"
#include "config.h"
#include "device/devicehub.h"
#include "device/hamamatsu/hamamatsu_dcam.h"
#include "eventstream.h"
#include "image/imagemanager.h"
#include "task/channelcontrol.h"
#include "utils/time_utils.h"

class ExperimentControl;

class MultiChannelTask : public EventSender {
public:
    MultiChannelTask(ExperimentControl *exp);

    Status Acquire(std::string ndimage_name, std::vector<Channel> channels,
                   int i_z, int i_t, Site *site = nullptr,
                   nlohmann::ordered_json metadata = nullptr);

protected:
    Status EnableTrigger();
    Status PrepareBuffer();
    Status StartAcqusition();
    Status ExposeFrame(int i_ch, PropertyValueMap *property_snapshot);
    StatusOr<ImageData>
    GetFrame(int i_ch, std::chrono::system_clock::time_point *timestamp);
    void StopAcqusition();

private:
    ExperimentControl *exp;
    Hamamatsu::DCam *dcam;

    std::string ndimage_name;
    std::vector<Channel> channels;

    utils::StopWatch sw_exposure_end;
};

#endif
