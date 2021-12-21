#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <filesystem>
#include <map>
#include <shared_mutex>
#include <vector>

#include "data/imagedata.h"
#include "data/imageutils.h"
#include "data/ndimage.h"
#include "eventstream.h"

class DataManager : public EventSender {
public:
    DataManager();

    void SetLiveViewFrame(ImageData new_frame);
    ImageData GetNextLiveViewFrame();

    std::filesystem::path ExperimentPath();
    void SetExperimentPath(std::filesystem::path path);

    std::filesystem::path ImagePath();

    std::vector<NDImage *> ListNDImage();
    bool HasNDImage(std::string ndimage_name);
    NDImage *GetNDImage(std::string ndimage_name);

    void NewNDImage(std::string ndimage_name,
                    std::vector<NDImageChannel> channel_info);
    void AddImage(std::string ndimage_name, int i_ch, int i_z, int i_t,
                  ImageData data, nlohmann::ordered_json metadata);


private:
    std::mutex mutex_live_frame;
    std::condition_variable cv_live_frame;
    bool new_frame_set = false;
    ImageData live_view_frame;

    std::filesystem::path exp_path;

    std::shared_mutex dataset_mutex;
    std::vector<NDImage *> dataset;
    std::map<std::string, NDImage *> dataset_map;

    im::UNet unet;
};

#endif
