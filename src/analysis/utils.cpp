#include "utils.h"

#include <stdexcept>

#include <fmt/format.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <xtensor/xview.hpp>


xt::xarray<uint16_t> EqualizeCLAHE(xt::xarray<uint16_t> im, double clip_limit)
{

    cv::Mat in_mat(im.shape(0), im.shape(1), CV_16U, im.data());
    cv::Mat out_mat;
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clip_limit, cv::Size(8, 8));
    clahe->apply(in_mat, out_mat);

    xt::xarray<uint16_t> im_eq = xt::xarray<uint16_t>::from_shape(im.shape());
    size_t bufsize = im_eq.size() * sizeof(uint16_t);
    if (bufsize != out_mat.total() * out_mat.elemSize()) {
        throw std::runtime_error("unexpected out_mat size");
    }
    memcpy(im_eq.data(), out_mat.data, bufsize);
    return im_eq;
}

xt::xarray<float> Normalize(xt::xarray<uint16_t> im)
{
    cv::Mat in_mat(im.shape(0), im.shape(1), CV_16UC1, im.data());
    in_mat.convertTo(in_mat, CV_32FC1);

    cv::Mat out_mat;
    cv::normalize(in_mat, out_mat, 1, 0, cv::NormTypes::NORM_MINMAX);

    xt::xarray<float> imnorm = xt::xarray<float>::from_shape(im.shape());
    size_t bufsize = imnorm.size() * sizeof(float);
    if (bufsize != out_mat.total() * out_mat.elemSize()) {
        throw std::runtime_error("unexpected out_mat size");
    }
    memcpy(imnorm.data(), out_mat.data, bufsize);
    return imnorm;
}

xt::xarray<uint16_t> RegionLabel(xt::xarray<float> im_score,
                                 std::vector<ImageRegionProp> &region_props)
{
    // Threshold
    cv::Mat score_mat(im_score.shape(0), im_score.shape(1), CV_32F,
                      im_score.data());
    cv::Mat mask_mat;
    cv::threshold(score_mat, mask_mat, 0.5, 1, cv::THRESH_BINARY);
    mask_mat.convertTo(mask_mat, CV_8U, 1, 0);

    // Label image with region props
    cv::Mat label_mat;
    cv::Mat stats_mat;
    cv::Mat centroids_mat;

    int n_labels = cv::connectedComponentsWithStats(
        mask_mat, label_mat, stats_mat, centroids_mat, 8, CV_16U);

    xt::xarray<uint16_t> im_label = xt::xarray<uint16_t>::from_shape(
        {(size_t)(label_mat.rows), (size_t)(label_mat.cols)});
    size_t bufsize = im_label.size() * sizeof(uint16_t);
    if (bufsize != label_mat.total() * label_mat.elemSize()) {
        throw std::runtime_error("unexpected label_mat size");
    }
    memcpy(im_label.data(), label_mat.data, bufsize);

    // Convert region props
    for (int label = 0; label < n_labels; label++) {
        ImageRegionProp prop;
        prop.label = label;
        prop.bbox_x0 = stats_mat.at<int32_t>(
            label, cv::ConnectedComponentsTypes::CC_STAT_LEFT);
        prop.bbox_y0 = stats_mat.at<int32_t>(
            label, cv::ConnectedComponentsTypes::CC_STAT_TOP);
        prop.bbox_width = stats_mat.at<int32_t>(
            label, cv::ConnectedComponentsTypes::CC_STAT_WIDTH);
        prop.bbox_height = stats_mat.at<int32_t>(
            label, cv::ConnectedComponentsTypes::CC_STAT_HEIGHT);
        prop.area = stats_mat.at<int32_t>(
            label, cv::ConnectedComponentsTypes::CC_STAT_AREA);

        prop.centroid_x = centroids_mat.at<double>(label, 0);
        prop.centroid_y = centroids_mat.at<double>(label, 1);

        region_props.push_back(prop);
    }

    return im_label;
}


UNet::UNet(const std::string server_addr, const std::string model_name,
           const int model_version, const std::string input_name)
{
    grpc::ChannelArguments channel_args;
    channel_args.SetInt(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH, 20 * 1024 * 1024);
    channel_args.SetInt(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH, 20 * 1024 * 1024);
    grpc_channel = grpc::CreateCustomChannel(
        server_addr, grpc::InsecureChannelCredentials(), channel_args);
    stub = tensorflow::serving::PredictionService::NewStub(grpc_channel);

    this->server_addr = server_addr;
    this->model_name = model_name;
    this->model_version = model_version;
    this->input_name = input_name;
}

xt::xarray<float> UNet::GetScore(xt::xarray<float> im)
{
    grpc::ClientContext ctx;
    tensorflow::serving::PredictRequest req;
    tensorflow::serving::PredictResponse resp;

    req.mutable_model_spec()->set_name(this->model_name);
    req.mutable_model_spec()->mutable_version()->set_value(this->model_version);
    req.mutable_model_spec()->set_signature_name("serving_default");

    tensorflow::TensorProto &input_tensor =
        (*req.mutable_inputs())[this->input_name];
    input_tensor.set_dtype(tensorflow::DataType::DT_FLOAT);

    input_tensor.mutable_tensor_shape()->add_dim()->set_size(1);
    input_tensor.mutable_tensor_shape()->add_dim()->set_size(im.shape(0));
    input_tensor.mutable_tensor_shape()->add_dim()->set_size(im.shape(1));
    input_tensor.mutable_tensor_shape()->add_dim()->set_size(1);

    input_tensor.mutable_float_val()->Resize(im.size(), 0);
    memcpy(input_tensor.mutable_float_val()->mutable_data(), im.data(),
           im.size() * sizeof(float));

    // Run predict
    grpc::Status status = stub->Predict(&ctx, req, &resp);
    if (!status.ok()) {
        throw std::runtime_error(
            fmt::format("stub.Predict: {}", status.error_message()));
    }

    // Get output tensor proto
    if (resp.outputs().size() != 1) {
        throw std::runtime_error(fmt::format(
            "unexpected output: {} output tensors", resp.outputs().size()));
    }
    tensorflow::TensorProto output_tensor = resp.outputs().begin()->second;

    // Validate type and shape
    if (output_tensor.dtype() != tensorflow::DataType::DT_FLOAT) {
        throw std::runtime_error(
            fmt::format("unexpected output dtype {}", output_tensor.dtype()));
    }

    // Convert tensor to xarray
    std::vector<uint32_t> shape;
    for (int i = 0; i < output_tensor.tensor_shape().dim_size(); i++) {
        shape.push_back(output_tensor.tensor_shape().dim(i).size());
    }
    xt::xarray<float> score = xt::xarray<float>::from_shape(shape);
    memcpy(score.data(), output_tensor.float_val().data(),
           output_tensor.float_val_size() * sizeof(float));

    // Slice output based on shape
    if (shape.size() == 3) {
        // shape = [1, height, width]
        if (shape[0] != 1) {
            throw std::runtime_error(fmt::format("unexpected output shape ({})",
                                                 fmt::join(shape, ", ")));
        }
        xt::xarray<float> score_sliced =
            xt::view(score, 0, xt::all(), xt::all());
        return score_sliced;
    } else if (shape.size() == 4) {
        // shape = [1, height, width, 2]
        if (shape[0] != 1) {
            throw std::runtime_error(fmt::format("unexpected output shape ({})",
                                                 fmt::join(shape, ", ")));
        }
        if (shape[3] != 2) {
            throw std::runtime_error(fmt::format("unexpected output shape ({})",
                                                 fmt::join(shape, ", ")));
        }
        xt::xarray<float> score_sliced =
            xt::view(score, 0, xt::all(), xt::all(), 1);
        return score_sliced;
    } else {
        throw std::runtime_error(fmt::format("unexpected output shape ({})",
                                             fmt::join(shape, ", ")));
    }
}
