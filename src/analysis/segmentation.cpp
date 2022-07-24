#include "segmentation.h"

#include <stdexcept>

#include <fmt/format.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

ImageData EqualizeCLAHE(ImageData im, double clip_limit)
{
    cv::Mat in_mat = im.AsMat();
    cv::Mat out_mat;
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clip_limit, cv::Size(8, 8));
    clahe->apply(in_mat, out_mat);

    ImageData im_eq =
        ImageData(im.Height(), im.Width(), im.DataType(), im.ColorType());
    if (im_eq.BufSize() != out_mat.total() * out_mat.elemSize()) {
        throw std::runtime_error("unexpected out_mat size");
    }
    memcpy(im_eq.Buf().get(), out_mat.data, im_eq.BufSize());
    return im_eq;
}

ImageData RegionLabel(ImageData im_score,
                      std::vector<ImageRegionProp> &region_props,
                      double threshold)
{
    cv::Mat score_mat = im_score.AsMat();
    cv::Mat mask_mat;
    cv::threshold(score_mat, mask_mat, threshold, 1, cv::THRESH_BINARY);
    mask_mat.convertTo(mask_mat, CV_8U, 255, 0);

    cv::Mat label_mat;
    cv::Mat stats_mat;
    cv::Mat centroids_mat;

    int n_labels = cv::connectedComponentsWithStats(
        mask_mat, label_mat, stats_mat, centroids_mat, 8, CV_16U);

    // Label Image
    ImageData im_label = ImageData(label_mat.rows, label_mat.cols,
                                   DataType::Uint16, ColorType::Mono16);
    im_label.CopyFrom(label_mat);

    // Region props
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

std::vector<double> RegionMean(ImageData im, ImageData label,
                               std::vector<ImageRegionProp> &region_props)
{
    if ((im.Width() != label.Width()) || (im.Height() != label.Height())) {
        throw std::invalid_argument("im and label have different shapes");
    }

    int n_labels = region_props.size();
    std::vector<double> sum;
    sum.resize(n_labels, 0);

    uint16_t *im_label_buf = reinterpret_cast<uint16_t *>(label.Buf().get());

    uint8_t *im_buf_u8;
    uint16_t *im_buf_u16;
    float *im_buf_f32;

    switch (im.DataType()) {
    case DataType::Uint8:
        im_buf_u8 = reinterpret_cast<uint8_t *>(im.Buf().get());
        for (int i = 0; i < im.size(); i++) {
            sum[im_label_buf[i]] += im_buf_u8[i];
        }
        break;
    case DataType::Uint16:
        im_buf_u16 = reinterpret_cast<uint16_t *>(im.Buf().get());
        for (int i = 0; i < im.size(); i++) {
            sum[im_label_buf[i]] += im_buf_u16[i];
        }
        break;
    case DataType::Float32:
        im_buf_f32 = reinterpret_cast<float *>(im.Buf().get());
        for (int i = 0; i < im.size(); i++) {
            sum[im_label_buf[i]] += im_buf_f32[i];
        }
        break;
    default:
        throw std::invalid_argument("unsupported datatype");
    }

    std::vector<double> mean;
    mean.resize(n_labels, 0);
    for (int label = 0; label < n_labels; label++) {
        mean[label] = sum[label] / region_props[label].area;
    }
    return mean;
}

UNet::UNet(const std::string server_addr, const std::string model_name, const std::string input_name, const std::string output_name)
{
    grpc::ChannelArguments channel_args;
    channel_args.SetInt(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH, 20 * 1024 * 1024);
    channel_args.SetInt(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH, 20 * 1024 * 1024);
    grpc_channel = grpc::CreateCustomChannel(
        server_addr, grpc::InsecureChannelCredentials(), channel_args);
    stub = tensorflow::serving::PredictionService::NewStub(grpc_channel);

    this->server_addr = server_addr;
    this->model_name = model_name;
    this->input_name = input_name;
    this->output_name = output_name;
}

ImageData UNet::GetScore(ImageData im)
{
    ImageData im_f32 = im.AsFloat32();

    grpc::ClientContext ctx;
    tensorflow::serving::PredictRequest req;
    tensorflow::serving::PredictResponse resp;

    req.mutable_model_spec()->set_name(this->model_name);
    req.mutable_model_spec()->set_signature_name("serving_default");
    // req.mutable_model_spec()->mutable_version()->set_value(1);

    tensorflow::TensorProto &input_tensor = (*req.mutable_inputs())[this->input_name];
    input_tensor.set_dtype(tensorflow::DataType::DT_FLOAT);

    input_tensor.mutable_tensor_shape()->add_dim()->set_size(1);
    input_tensor.mutable_tensor_shape()->add_dim()->set_size(im_f32.Height());
    input_tensor.mutable_tensor_shape()->add_dim()->set_size(im_f32.Width());
    input_tensor.mutable_tensor_shape()->add_dim()->set_size(1);

    input_tensor.mutable_float_val()->Resize(im_f32.size(), 0);
    memcpy(input_tensor.mutable_float_val()->mutable_data(), im_f32.Buf().get(),
           im_f32.BufSize());

    grpc::Status status = stub->Predict(&ctx, req, &resp);
    if (!status.ok()) {
        throw std::runtime_error(
            fmt::format("stub.Predict: {}", status.error_message()));
    }

    auto it = resp.outputs().find(this->output_name);
    if (it == resp.outputs().end()) {
        throw std::runtime_error("output tensor not found");
    }
    tensorflow::TensorProto output_tensor = it->second;

    if (output_tensor.dtype() != tensorflow::DataType::DT_FLOAT) {
        throw std::runtime_error(
            fmt::format("unexpected output dtype {}", output_tensor.dtype()));
    }
    uint32_t out_height = output_tensor.tensor_shape().dim(1).size();
    uint32_t out_width = output_tensor.tensor_shape().dim(2).size();
    ImageData score =
        ImageData(out_height, out_width, DataType::Float32, ColorType::Unknown);
    score.CopyFrom(output_tensor.float_val().data(),
                   output_tensor.float_val().size());

    return score;
}