#include "api_server.h"

#include <stdexcept>

#include "data/imageutils.h"
#include "logging.h"

api::DataType DataTypeToAPI(DataType dtype)
{
    switch (dtype) {
    case DataType::Uint8:
        return api::DataType::UINT8;
    case DataType::Uint16:
        return api::DataType::UINT16;
    case DataType::Int16:
        return api::DataType::INT16;
    case DataType::Int32:
        return api::DataType::INT32;
    case DataType::Float32:
        return api::DataType::FLOAT32;
    case DataType::Float64:
        return api::DataType::FLOAT64;
    default:
        return api::DataType::UNKNOWN_DTYPE;
    }
}

DataType DataTypeFromAPI(api::DataType api_dtype)
{
    switch (api_dtype) {
    case api::DataType::UINT8:
        return DataType::Uint8;
    case api::DataType::UINT16:
        return DataType::Uint16;
    case api::DataType::INT16:
        return DataType::Int16;
    case api::DataType::INT32:
        return DataType::Int32;
    case api::DataType::FLOAT32:
        return DataType::Float32;
    case api::DataType::FLOAT64:
        return DataType::Float64;
    default:
        return DataType::Unknown;
    }
}

api::ColorType ColorTypeToAPI(ColorType ctype)
{
    switch (ctype) {
    case ColorType::Mono8:
        return api::ColorType::MONO8;
    case ColorType::Mono10:
        return api::ColorType::MONO10;
    case ColorType::Mono12:
        return api::ColorType::MONO12;
    case ColorType::Mono14:
        return api::ColorType::MONO14;
    case ColorType::Mono16:
        return api::ColorType::MONO16;
    case ColorType::BayerRG8:
        return api::ColorType::BAYERRG8;
    case ColorType::BayerRG16:
        return api::ColorType::BAYERRG16;
    default:
        return api::ColorType::UNKNOWN_CTYPE;
    }
}

ColorType ColorTypeFromAPI(api::ColorType api_ctype)
{
    switch (api_ctype) {
    case api::ColorType::MONO8:
        return ColorType::Mono8;
    case api::ColorType::MONO10:
        return ColorType::Mono10;
    case api::ColorType::MONO12:
        return ColorType::Mono12;
    case api::ColorType::MONO14:
        return ColorType::Mono14;
    case api::ColorType::MONO16:
        return ColorType::Mono16;
    case api::ColorType::BAYERRG8:
        return ColorType::BayerRG8;
    case api::ColorType::BAYERRG16:
        return ColorType::BayerRG16;
    default:
        return ColorType::Unknown;
    }
}

APIServer::APIServer(std::string listen_addr, DeviceHub *hub,
                     ImagingControl *imaging_control)
{
    this->hub = hub;
    this->imaging_control = imaging_control;

    grpc::ServerBuilder builder;
    builder.SetMaxSendMessageSize(20 * 1024 * 1024);
    builder.SetMaxReceiveMessageSize(20 * 1024 * 1024);
    builder.AddListeningPort(listen_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    server = builder.BuildAndStart();

    if (server == nullptr) {
        throw std::runtime_error(
            fmt::format("cannot bind to address {}", listen_addr));
    }
}

grpc::Status APIServer::GetProperty(ServerContext *context,
                                    const api::GetPropertyRequest *req,
                                    api::GetPropertyResponse *resp)
{
    for (const auto &name : req->name()) {
        StatusOr<std::string> value;
        try {
            value = hub->GetProperty(name);
        } catch (std::exception &e) {
            return grpc::Status(
                grpc::StatusCode::INTERNAL,
                fmt::format("unexpected exception: {}", e.what()));
        }
        if (!value.ok()) {
            return grpc::Status((grpc::StatusCode)value.status().raw_code(),
                                std::string(value.status().message()));
        }
        auto property = resp->add_property();
        property->set_name(name);
        property->set_value(*value);
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::SetProperty(ServerContext *context,
                                    const api::SetPropertyRequest *req,
                                    google::protobuf::Empty *resp)
{
    PropertyValueMap property_value_map;
    for (const auto &property : req->property()) {
        std::string name = property.name();
        std::string value = property.value();
        property_value_map[name] = value;
    }

    Status status;
    try {
        status = hub->SetProperty(property_value_map);
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    if (!status.ok()) {
        return grpc::Status((grpc::StatusCode)status.raw_code(),
                            std::string(status.message()));
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::WaitProperty(ServerContext *context,
                                     const api::WaitPropertyRequest *req,
                                     google::protobuf::Empty *resp)
{
    std::vector<PropertyPath> propertyList(req->name().begin(),
                                           req->name().end());
    std::chrono::nanoseconds timeout;
    timeout = std::chrono::seconds(req->timeout().seconds()) +
              std::chrono::nanoseconds(req->timeout().nanos());
    if (timeout == std::chrono::nanoseconds(0)) {
        timeout = std::chrono::seconds(30);
    }

    absl::Status status;
    try {
        status = hub->WaitPropertyFor(
            propertyList,
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    if (!status.ok()) {
        return grpc::Status((grpc::StatusCode)status.raw_code(),
                            std::string(status.message()));
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::ListProperty(ServerContext *context,
                                     const api::ListPropertyRequest *req,
                                     api::ListPropertyResponse *resp)
{
    std::string name = req->name();
    std::vector<PropertyPath> property_list;

    try {
        property_list = hub->ListProperty(name);
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }

    for (const auto &path : property_list) {
        resp->add_name(path.ToString());
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::ListChannel(ServerContext *context,
                                    const protobuf::Empty *req,
                                    api::ListChannelResponse *resp)
{
    std::vector<std::string> preset_names =
        imaging_control->ChannelControl()->ListPresetNames();

    for (const auto &preset_name : preset_names) {
        ChannelPreset preset =
            imaging_control->ChannelControl()->GetPreset(preset_name);
        auto ch = resp->add_channels();
        ch->set_preset_name(preset_name);
        ch->set_exposure_ms(preset.default_exposure_ms);
        if (!preset.illumination_property.empty()) {
            ch->set_illumination_intensity(
                preset.default_illumination_intensity);
        }
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::SwitchChannel(ServerContext *context,
                                      const ::api::SwitchChannelRequest *req,
                                      protobuf::Empty *resp)
{
    try {
        imaging_control->ChannelControl()->SwitchChannel(
            req->channel().preset_name(), req->channel().exposure_ms(),
            req->channel().illumination_intensity());
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status
APIServer::SetExperimentPath(ServerContext *context,
                             const api::SetExperimentPathRequest *req,
                             google::protobuf::Empty *resp)
{
    try {
        imaging_control->DataManager()->SetExperimentPath(req->path());
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status
APIServer::AcquireMultiChannel(ServerContext *context,
                               const api::AcquireMultiChannelRequest *req,
                               protobuf::Empty *resp)
{
    nlohmann::ordered_json metadata(req->metadata());
    std::vector<Channel> channels;
    for (const auto &ch : req->channels()) {
        channels.push_back(Channel{
            .preset_name = ch.preset_name(),
            .exposure_ms = ch.exposure_ms(),
            .illumination_intensity = ch.illumination_intensity(),
        });
    }
    try {
        imaging_control->AcquireMultiChannel(req->ndimage_name(), channels,
                                             req->i_z(), req->i_t(), metadata);
        imaging_control->WaitMultiChannelTask();
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::ListNDImage(ServerContext *context,
                                    const google::protobuf::Empty *req,
                                    api::ListNDImageResponse *resp)
{
    auto ndimage_list = imaging_control->DataManager()->ListNDImage();
    for (const auto &im : ndimage_list) {
        auto ndimage_pb = resp->add_ndimages();
        ndimage_pb->set_name(im->Name());
        ndimage_pb->set_n_images(im->NumImages());
        ndimage_pb->set_n_ch(im->NChannels());
        ndimage_pb->set_n_z(im->NDimZ());
        ndimage_pb->set_n_t(im->NDimT());
        for (const auto &ch_info : im->ChannelInfo()) {
            auto ch_info_pb = ndimage_pb->add_channel_info();
            ch_info_pb->set_name(ch_info.name);
            ch_info_pb->set_width(ch_info.width);
            ch_info_pb->set_height(ch_info.height);
            ch_info_pb->set_dtype(DataTypeToAPI(ch_info.dtype));
            ch_info_pb->set_ctype(ColorTypeToAPI(ch_info.ctype));
        }
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::GetImage(ServerContext *context,
                                 const api::GetImageRequest *req,
                                 api::GetImageResponse *resp)
{
    NDImage *ndimage =
        imaging_control->DataManager()->GetNDImage(req->ndimage_name());
    if (ndimage == nullptr) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "ndimage not found");
    }
    int i_ch = ndimage->ChannelIndex(req->channel_name());
    if (i_ch < 0) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "channel not found");
    }
    if (req->i_z() < 0 || req->i_z() >= ndimage->NDimZ()) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "i_z not found");
    }
    if (req->i_t() < 0 || req->i_t() >= ndimage->NDimT()) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "i_t not found");
    }

    ImageData data = ndimage->GetData(i_ch, req->i_z(), req->i_t());

    resp->mutable_data()->set_width(data.Width());
    resp->mutable_data()->set_height(data.Height());
    resp->mutable_data()->set_dtype(DataTypeToAPI(data.DataType()));
    resp->mutable_data()->set_ctype(ColorTypeToAPI(data.ColorType()));
    resp->mutable_data()->set_buf(data.Buf().get(), data.BufSize());
    return grpc::Status::OK;
}

grpc::Status
APIServer::GetSegmentationScore(ServerContext *context,
                                const api::GetSegmentationScoreRequest *req,
                                api::GetSegmentationScoreResponse *resp)
{
    DataType dtype = DataTypeFromAPI(req->data().dtype());
    ColorType ctype = ColorTypeFromAPI(req->data().ctype());
    ImageData im =
        ImageData(req->data().height(), req->data().width(), dtype, ctype);
    im.CopyFrom(req->data().buf());

    ImageData score;
    try {
        im::UNet unet(config.system.unet_grpc_addr);
        score = unet.GetScore(im);
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }

    resp->mutable_data()->set_width(score.Width());
    resp->mutable_data()->set_height(score.Height());
    resp->mutable_data()->set_dtype(DataTypeToAPI(score.DataType()));
    resp->mutable_data()->set_ctype(ColorTypeToAPI(score.ColorType()));
    resp->mutable_data()->set_buf(score.Buf().get(), score.BufSize());
    return grpc::Status::OK;
}