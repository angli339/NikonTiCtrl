#include "api_server.h"

#include <stdexcept>

#include "image/imageutils.h"
#include "logging.h"

api::DataType DataTypeToPB(DataType dtype)
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

DataType DataTypeFromPB(api::DataType pb_dtype)
{
    switch (pb_dtype) {
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
        throw std::invalid_argument("unimplemented ctype");
    }
}

api::ColorType ColorTypeToPB(ColorType ctype)
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
        throw std::invalid_argument("unimplemented ctype");
    }
}

ColorType ColorTypeFromPB(api::ColorType pb_ctype)
{
    switch (pb_ctype) {
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
        throw std::invalid_argument("unimplemented ctype");
    }
}

api::PlateType PlateTypeToPB(PlateType pb_platetype)
{
    switch (pb_platetype) {
    case PlateType::Slide:
        return api::PlateType::SLIDE;
    case PlateType::Wellplate96:
        return api::PlateType::WELLPLATE96;
    case PlateType::Wellplate384:
        return api::PlateType::WELLPLATE384;
    default:
        throw std::invalid_argument("unimplemented PlateType");
    }
}

PlateType PlateTypeFromPB(api::PlateType pb_platetype)
{
    switch (pb_platetype) {
    case api::PlateType::SLIDE:
        return PlateType::Slide;
    case api::PlateType::WELLPLATE96:
        return PlateType::Wellplate96;
    case api::PlateType::WELLPLATE384:
        return PlateType::Wellplate384;
    default:
        throw std::invalid_argument("unimplemented PlateType");
    }
}

APIServer::APIServer(std::string listen_addr, ExperimentControl *exp)
{
    this->exp = exp;

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

void APIServer::Wait() { server->Wait(); }
void APIServer::Shutdown() { server->Shutdown(); }

grpc::Status APIServer::GetProperty(ServerContext *context,
                                    const api::GetPropertyRequest *req,
                                    api::GetPropertyResponse *resp)
{
    for (const auto &name : req->name()) {
        StatusOr<std::string> value;
        try {
            value = exp->Devices()->GetProperty(name);
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
        status = exp->Devices()->SetProperty(property_value_map);
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
        status = exp->Devices()->WaitPropertyFor(
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
        property_list = exp->Devices()->ListProperty(name);
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
    std::vector<std::string> preset_names = exp->Channels()->ListPresetNames();

    for (const auto &preset_name : preset_names) {
        ChannelPreset preset = exp->Channels()->GetPreset(preset_name);
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
        exp->Channels()->SwitchChannel(req->channel().preset_name(),
                                       req->channel().exposure_ms(),
                                       req->channel().illumination_intensity());
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::OpenExperiment(ServerContext *context,
                                       const api::OpenExperimentRequest *req,
                                       google::protobuf::Empty *resp)
{
    try {
        if (req->has_base_dir()) {
            exp->SetBaseDir(req->base_dir());
        }
        exp->OpenExperiment(req->name());
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::ListPlate(ServerContext *context,
                                  const google::protobuf::Empty *req,
                                  api::ListPlateResponse *resp)
{
    try {
        for (const auto &plate : exp->Samples()->Plates()) {
            auto plate_pb = resp->add_plate();
            plate_pb->set_uuid(plate->UUID());
            plate_pb->set_type(PlateTypeToPB(plate->Type()));
            plate_pb->set_id(plate->ID());
            if (plate->PositionOrigin().has_value()) {
                auto pos_origin = plate->PositionOrigin().value();
                plate_pb->mutable_pos_origin()->set_x(pos_origin.x);
                plate_pb->mutable_pos_origin()->set_y(pos_origin.y);
            }
            plate_pb->set_metadata(plate->Metadata().dump());

            for (const auto &well : plate->Wells()) {
                auto well_pb = plate_pb->add_well();
                well_pb->set_uuid(well->UUID());
                well_pb->set_id(well->ID());
                well_pb->mutable_rel_pos()->set_x(well->RelativePosition().x);
                well_pb->mutable_rel_pos()->set_y(well->RelativePosition().y);
                well_pb->set_enabled(well->Enabled());
                well_pb->set_metadata(well->Metadata().dump());

                for (const auto &site : well->Sites()) {
                    auto site_pb = well_pb->add_site();
                    site_pb->set_uuid(site->UUID());
                    site_pb->set_id(site->ID());
                    site_pb->mutable_rel_pos()->set_x(
                        site->RelativePosition().x);
                    site_pb->mutable_rel_pos()->set_y(
                        site->RelativePosition().y);
                    site_pb->set_enabled(site->Enabled());
                    site_pb->set_metadata(site->Metadata().dump());
                }
            }
        }
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::AddPlate(ServerContext *context,
                                 const api::AddPlateRequest *req,
                                 protobuf::Empty *resp)
{
    try {
        exp->Samples()->AddPlate(PlateTypeFromPB(req->plate_type()),
                                 req->plate_id());
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status
APIServer::SetPlatePositionOrigin(ServerContext *context,
                                  const api::SetPlatePositionOriginRequest *req,
                                  google::protobuf::Empty *resp)
{
    try {
        Plate *plate = exp->Samples()->PlateByUUID(req->plate_uuid());
        if (plate == nullptr) {
            return grpc::Status(
                grpc::StatusCode::NOT_FOUND,
                fmt::format("plate '{}' not found", req->plate_uuid()));
        }
        exp->Samples()->SetPlatePositionOrigin(plate->ID(), req->x(), req->y());
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status
APIServer::SetPlateMetadata(ServerContext *context,
                            const api::SetPlateMetadataRequest *req,
                            google::protobuf::Empty *resp)
{
    try {
        Plate *plate = exp->Samples()->PlateByUUID(req->plate_uuid());
        if (plate == nullptr) {
            return grpc::Status(
                grpc::StatusCode::NOT_FOUND,
                fmt::format("plate '{}' not found", req->plate_uuid()));
        }
        auto value = nlohmann::ordered_json::parse(req->json_value());
        exp->Samples()->SetPlateMetadata(plate->ID(), req->key(), value);
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::SetWellsEnabled(ServerContext *context,
                                        const api::SetWellsEnabledRequest *req,
                                        google::protobuf::Empty *resp)
{
    try {
        Plate *plate = exp->Samples()->PlateByUUID(req->plate_uuid());
        if (plate == nullptr) {
            return grpc::Status(
                grpc::StatusCode::NOT_FOUND,
                fmt::format("plate '{}' not found", req->plate_uuid()));
        }
        std::vector<std::string> wells;
        for (const auto &well : req->well_id()) {
            wells.push_back(well);
        }
        exp->Samples()->SetWellsEnabled(plate->ID(), wells, req->enabled());
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status
APIServer::SetWellsMetadata(ServerContext *context,
                            const api::SetWellsMetadataRequest *req,
                            google::protobuf::Empty *resp)
{
    try {
        Plate *plate = exp->Samples()->PlateByUUID(req->plate_uuid());
        if (plate == nullptr) {
            return grpc::Status(
                grpc::StatusCode::NOT_FOUND,
                fmt::format("plate '{}' not found", req->plate_uuid()));
        }
        std::vector<std::string> wells;
        for (const auto &well : req->well_id()) {
            wells.push_back(well);
        }
        auto value = nlohmann::ordered_json::parse(req->json_value());
        exp->Samples()->SetWellsMetadata(plate->ID(), wells, req->key(), value);
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::CreateSites(ServerContext *context,
                                    const api::CreateSitesRequest *req,
                                    google::protobuf::Empty *resp)
{
    try {
        Plate *plate = exp->Samples()->PlateByUUID(req->plate_uuid());
        if (plate == nullptr) {
            return grpc::Status(
                grpc::StatusCode::NOT_FOUND,
                fmt::format("plate '{}' not found", req->plate_uuid()));
        }
        std::vector<std::string> wells;
        for (const auto &well : req->well_id()) {
            wells.push_back(well);
        }
        exp->Samples()->CreateSitesOnCenteredGrid(
            plate->ID(), wells, req->n_x(), req->n_y(), req->spacing_x(),
            req->spacing_y());
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
        Site *site = nullptr;
        if (!req->site_uuid().empty()) {
            site = exp->Samples()->SiteByUUID(req->site_uuid());
            if (site == nullptr) {
                return grpc::Status(
                    grpc::StatusCode::NOT_FOUND,
                    fmt::format("site '{}' not found", req->site_uuid()));
            }
        }
        exp->AcquireMultiChannel(req->ndimage_name(), channels, req->i_z(),
                                 req->i_t(), site, metadata);
        exp->WaitMultiChannelTask();
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
    try {
        auto ndimage_list = exp->Images()->ListNDImage();
        for (const auto &im : ndimage_list) {
            auto ndimage_pb = resp->add_ndimage();
            ndimage_pb->set_name(im->Name());
            for (const auto &ch_name : im->ChannelNames()) {
                ndimage_pb->add_ch_name(ch_name);
            }
            ndimage_pb->set_width(im->Width());
            ndimage_pb->set_height(im->Height());
            ndimage_pb->set_n_ch(im->NChannels());
            ndimage_pb->set_n_z(im->NDimZ());
            ndimage_pb->set_n_t(im->NDimT());
            ndimage_pb->set_dtype(DataTypeToPB(im->DataType()));
            ndimage_pb->set_ctype(ColorTypeToPB(im->ColorType()));
        }
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::GetNDImage(ServerContext *context,
                                   const api::GetNDImageRequest *req,
                                   api::GetNDImageResponse *resp)
{
    try {
        NDImage *im = exp->Images()->GetNDImage(req->ndimage_name());
        if (im == nullptr) {
            return grpc::Status(
                grpc::StatusCode::NOT_FOUND,
                fmt::format("ndimage '{}' not found", req->ndimage_name()));
        }

        auto ndimage_pb = resp->mutable_ndimage();
        ndimage_pb->set_name(im->Name());
        for (const auto &ch_name : im->ChannelNames()) {
            ndimage_pb->add_ch_name(ch_name);
        }
        ndimage_pb->set_width(im->Width());
        ndimage_pb->set_height(im->Height());
        ndimage_pb->set_n_ch(im->NChannels());
        ndimage_pb->set_n_z(im->NDimZ());
        ndimage_pb->set_n_t(im->NDimT());
        ndimage_pb->set_dtype(DataTypeToPB(im->DataType()));
        ndimage_pb->set_ctype(ColorTypeToPB(im->ColorType()));
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status APIServer::GetImageData(ServerContext *context,
                                     const api::GetImageDataRequest *req,
                                     api::GetImageDataResponse *resp)
{
    try {
        NDImage *ndimage = exp->Images()->GetNDImage(req->ndimage_name());
        if (ndimage == nullptr) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND,
                                "ndimage not found");
        }
        int i_ch = ndimage->ChannelIndex(req->channel_name());
        if (i_ch < 0) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND,
                                "channel not found");
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
        resp->mutable_data()->set_dtype(DataTypeToPB(data.DataType()));
        resp->mutable_data()->set_ctype(ColorTypeToPB(data.ColorType()));
        resp->mutable_data()->set_buf(data.Buf().get(), data.BufSize());
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }
    return grpc::Status::OK;
}

grpc::Status
APIServer::GetSegmentationScore(ServerContext *context,
                                const api::GetSegmentationScoreRequest *req,
                                api::GetSegmentationScoreResponse *resp)
{
    xt::xarray<float> score;
    try {
        score = exp->Analysis()->GetSegmentationScore(
            req->ndimage_name(), req->i_t(), req->ch_name());
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }

    resp->mutable_data()->set_height(score.shape(0));
    resp->mutable_data()->set_width(score.shape(1));
    resp->mutable_data()->set_dtype(api::DataType::FLOAT32);
    resp->mutable_data()->set_ctype(api::ColorType::UNKNOWN_CTYPE);
    resp->mutable_data()->set_buf(score.data(), score.size() * sizeof(float));
    return grpc::Status::OK;
}

grpc::Status APIServer::QuantifyRegions(ServerContext *context,
                                        const api::QuantifyRegionsRequest *req,
                                        api::QuantifyRegionsResponse *resp)
{
    int n_regions;
    QuantificationResults results;
    try {
        n_regions = exp->Analysis()->QuantifyRegions(
            req->ndimage_name(), req->i_t(), req->segmentation_ch());

        // Workaround to deal with 0 cell condition
        // Currently QuantificationResults is not saved in 0 cell condition
        // resulting in failure in GetQuantification
        if (n_regions == 0) {
            resp->set_n_regions(n_regions);
            return grpc::Status::OK;
        }

        results =
            exp->Analysis()->GetQuantification(req->ndimage_name(), req->i_t());

        resp->set_n_regions(n_regions);
        for (const auto &rp : results.region_props) {
            auto pb_rp = resp->add_region_prop();
            pb_rp->set_label(rp.label);
            pb_rp->set_bbox_x0(rp.bbox_x0);
            pb_rp->set_bbox_y0(rp.bbox_y0);
            pb_rp->set_bbox_width(rp.bbox_width);
            pb_rp->set_bbox_height(rp.bbox_height);
            pb_rp->set_area(rp.area);
            pb_rp->set_centroid_x(rp.centroid_x);
            pb_rp->set_centroid_y(rp.centroid_y);
        }
        for (int i_ch = 0; i_ch < results.ch_names.size(); i_ch++) {
            std::string ch_name = results.ch_names[i_ch];
            xt::xarray<double> mean_intensity =
                results.raw_intensity_mean[i_ch];

            auto pb_ch = resp->add_raw_intensity();
            pb_ch->set_ch_name(ch_name);
            for (int i = 0; i < n_regions; i++) {
                pb_ch->add_values(mean_intensity[i]);
            }
        }
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }

    return grpc::Status::OK;
}

grpc::Status
APIServer::GetQuantification(ServerContext *context,
                             const api::GetQuantificationRequest *req,
                             api::GetQuantificationResponse *resp)
{
    try {
        QuantificationResults results =
            exp->Analysis()->GetQuantification(req->ndimage_name(), req->i_t());

        for (const auto &rp : results.region_props) {
            auto pb_rp = resp->add_region_prop();
            pb_rp->set_label(rp.label);
            pb_rp->set_bbox_x0(rp.bbox_x0);
            pb_rp->set_bbox_y0(rp.bbox_y0);
            pb_rp->set_bbox_width(rp.bbox_width);
            pb_rp->set_bbox_height(rp.bbox_height);
            pb_rp->set_area(rp.area);
            pb_rp->set_centroid_x(rp.centroid_x);
            pb_rp->set_centroid_y(rp.centroid_y);
        }
        for (int i_ch = 0; i_ch < results.ch_names.size(); i_ch++) {
            std::string ch_name = results.ch_names[i_ch];
            xt::xarray<double> mean_intensity =
                results.raw_intensity_mean[i_ch];

            auto pb_ch = resp->add_raw_intensity();
            pb_ch->set_ch_name(ch_name);
            for (int i = 0; i < mean_intensity.shape(0); i++) {
                pb_ch->add_values(mean_intensity[i]);
            }
        }
    } catch (std::exception &e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            fmt::format("unexpected exception: {}", e.what()));
    }

    return grpc::Status::OK;
}