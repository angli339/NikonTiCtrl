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
#include "data/datamanager.h"
#include "task/channelcontrol.h"
#include "utils/time_utils.h"

class MultiChannelTask : public EventSender {
public:
    MultiChannelTask(DeviceHub *hub, Hamamatsu::DCam *dcam,
                     ChannelControl *channel_control,
                     DataManager *data_manager);

    Status Acquire(std::string ndimage_name, std::vector<Channel> channels,
                   int i_z, int i_t, nlohmann::ordered_json metadata = nullptr);

protected:
    Status EnableTrigger();
    Status PrepareBuffer();
    Status StartAcqusition();
    std::map<PropertyPath, std::string> ExposeFrame(int i_ch);
    ImageData GetFrame(int i_ch,
                       std::chrono::system_clock::time_point *timestamp);
    Status StopAcqusition();

private:
    DeviceHub *hub;
    Hamamatsu::DCam *dcam;
    ChannelControl *channel_control;
    DataManager *data_manager;

    std::string ndimage_name;
    std::vector<Channel> channels;

    utils::StopWatch sw_exposure_end;
};

#endif
