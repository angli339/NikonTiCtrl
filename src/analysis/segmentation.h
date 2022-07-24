#ifndef SEGMENTATION_H
#define SEGMENTATION_H

#include <vector>

#include <grpcpp/channel.h>
#include <tensorflow_serving/apis/prediction_service.grpc.pb.h>

#include "image/imagedata.h"

ImageData EqualizeCLAHE(ImageData im, double clip_limit = 2);

struct ImageRegionProp {
    uint16_t label;
    uint32_t bbox_x0;
    uint32_t bbox_y0;
    uint32_t bbox_width;
    uint32_t bbox_height;
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
    UNet(const std::string server_addr, const std::string model_name, const std::string input_name, const std::string output_name);
    ImageData GetScore(ImageData im);

private:
    std::string server_addr;
    std::string model_name;
    std::string input_name;
    std::string output_name;

    std::shared_ptr<::grpc::Channel> grpc_channel;
    std::shared_ptr<::tensorflow::serving::PredictionService::Stub> stub;
};

#endif