#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <filesystem>
#include <map>
#include <shared_mutex>
#include <vector>

#include "image/imagedata.h"
#include "image/imageutils.h"
#include "image/ndimage.h"
#include "eventstream.h"

struct QuantificationResults;

class ImageManager : public EventSender {
public:
    ImageManager();

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

    int QuantifyRegions(std::string ndimage_name, int i_z, int i_t, std::string segmentation_ch);

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
    std::map<std::tuple<std::string, int, int>, QuantificationResults> quantifications;
};

struct QuantificationResults {
    int n_regions;
    std::vector<im::ImageRegionProp> region_props;
    std::vector<std::string> ch_names;
    std::vector<std::vector<double>> ch_means;
};

#endif
