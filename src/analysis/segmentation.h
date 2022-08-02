#ifndef SEGMENTATION_H
#define SEGMENTATION_H

#include <vector>

#include <grpcpp/channel.h>
#include <tensorflow_serving/apis/prediction_service.grpc.pb.h>
#include <xtensor/xadapt.hpp>
#include <xtensor/xarray.hpp>

#include "image/imagedata.h"

xt::xarray<uint16_t> EqualizeCLAHE(xt::xarray<uint16_t> im,
                                   double clip_limit = 2);

struct ImageRegionProp {
    uint16_t label;
    uint32_t bbox_x0;
    uint32_t bbox_y0;
    uint32_t bbox_width;
    uint32_t bbox_height;
    double area;
    double centroid_x;
    double centroid_y;
};

xt::xarray<uint16_t> RegionLabel(xt::xarray<float> im_score,
                                 std::vector<ImageRegionProp> &region_props,
                                 double threshold = 0.5);

template <typename T>
xt::xarray<double> RegionSum(xt::xarray<T> im, xt::xarray<uint16_t> label,
                             int max_label);

class UNet {
public:
    UNet(const std::string server_addr, const std::string model_name,
         const std::string input_name, const std::string output_name);
    xt::xarray<float> GetScore(xt::xarray<float> im);

private:
    std::string server_addr;
    std::string model_name;
    std::string input_name;
    std::string output_name;

    std::shared_ptr<::grpc::Channel> grpc_channel;
    std::shared_ptr<::tensorflow::serving::PredictionService::Stub> stub;
};

template <typename T>
xt::xarray<double> RegionSum(xt::xarray<T> im, xt::xarray<uint16_t> label,
                             int max_label)
{
    if (im.shape() != label.shape()) {
        throw std::invalid_argument("im and label have different shapes");
    }

    std::vector<double> sum;
    sum.resize(max_label + 1, 0);

    auto im_fl = xt::flatten(im);
    auto label_fl = xt::flatten(label);
    for (int i = 0; i < im_fl.size(); i++) {
        uint16_t pixel_label = label_fl[i];
        sum[pixel_label] += im_fl[i];
    }

    return xt::adapt(sum);
}

#endif