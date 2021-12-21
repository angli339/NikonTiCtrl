#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <filesystem>
#include <vector>
namespace fs = std::filesystem;

#include <grpcpp/channel.h>
#include <tensorflow_serving/apis/prediction_service.grpc.pb.h>

#include "data/imagedata.h"

namespace im {

std::vector<double> Hist(ImageData im);
ImageData EqualizeCLAHE(ImageData im, double clip_limit = 2);

struct ImageRegionProp {
    uint16_t label;
    uint32_t bbbox_x0;
    uint32_t bbbox_y0;
    uint32_t bbbox_width;
    uint32_t bbbox_height;
    double area;
    double centroid_x;
    double centroid_y;
    double mean_score;
};

ImageData RegionLabel(ImageData im_score,
                      std::vector<ImageRegionProp> &region_props,
                      double threshold = 0.5);

std::vector<double> RegionMean(ImageData im, ImageData label,
                               std::vector<ImageRegionProp> &region_props);

class UNet {
public:
    UNet(const std::string addr);
    ImageData GetScore(ImageData im);

private:
    std::shared_ptr<::grpc::Channel> grpc_channel;
    std::shared_ptr<::tensorflow::serving::PredictionService::Stub> stub;
};

} // namespace im

#endif
