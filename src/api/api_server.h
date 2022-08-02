#ifndef APISERVER_H
#define APISERVER_H

#include <future>
#include <grpcpp/grpcpp.h>
#include <memory>

#include "device/devicehub.h"
#include "experimentcontrol.h"

#include "api.grpc.pb.h"
#include "api.pb.h"

using grpc::ServerContext;
using grpc::ServerReader;
namespace protobuf = google::protobuf;

class APIServer final : public api::NikonTiCtrl::Service {
public:
    APIServer(std::string listen_addr, ExperimentControl *exp);

    void Wait();
    void Shutdown();

    // Device
    grpc::Status GetProperty(ServerContext *context,
                             const api::GetPropertyRequest *req,
                             api::GetPropertyResponse *resp) override;
    grpc::Status SetProperty(ServerContext *context,
                             const api::SetPropertyRequest *req,
                             google::protobuf::Empty *resp) override;
    grpc::Status WaitProperty(ServerContext *context,
                              const api::WaitPropertyRequest *req,
                              google::protobuf::Empty *resp) override;
    grpc::Status ListProperty(ServerContext *context,
                              const api::ListPropertyRequest *req,
                              api::ListPropertyResponse *resp) override;

    // Channel
    grpc::Status ListChannel(ServerContext *context, const protobuf::Empty *req,
                             api::ListChannelResponse *resp) override;
    grpc::Status SwitchChannel(ServerContext *context,
                               const ::api::SwitchChannelRequest *req,
                               protobuf::Empty *resp) override;

    // Experiment
    grpc::Status OpenExperiment(ServerContext *context,
                                const api::OpenExperimentRequest *req,
                                google::protobuf::Empty *resp) override;

    // Sample
    grpc::Status ListPlate(ServerContext *context,
                           const google::protobuf::Empty *req,
                           api::ListPlateResponse *resp) override;
    grpc::Status AddPlate(ServerContext *context,
                          const api::AddPlateRequest *req,
                          google::protobuf::Empty *resp) override;
    grpc::Status
    SetPlatePositionOrigin(ServerContext *context,
                           const api::SetPlatePositionOriginRequest *req,
                           google::protobuf::Empty *resp) override;
    grpc::Status SetPlateMetadata(ServerContext *context,
                                  const api::SetPlateMetadataRequest *req,
                                  google::protobuf::Empty *resp) override;
    grpc::Status SetWellsEnabled(ServerContext *context,
                                 const api::SetWellsEnabledRequest *req,
                                 google::protobuf::Empty *resp) override;
    grpc::Status SetWellsMetadata(ServerContext *context,
                                  const api::SetWellsMetadataRequest *req,
                                  google::protobuf::Empty *resp) override;
    grpc::Status CreateSites(ServerContext *context,
                             const api::CreateSitesRequest *req,
                             google::protobuf::Empty *resp) override;

    // Task
    grpc::Status AcquireMultiChannel(ServerContext *context,
                                     const api::AcquireMultiChannelRequest *req,
                                     protobuf::Empty *resp) override;
    // Data
    grpc::Status ListNDImage(ServerContext *context,
                             const google::protobuf::Empty *req,
                             api::ListNDImageResponse *resp) override;
    grpc::Status GetNDImage(ServerContext *context,
                            const api::GetNDImageRequest *req,
                            api::GetNDImageResponse *resp) override;
    grpc::Status GetImageData(ServerContext *context,
                              const api::GetImageDataRequest *req,
                              api::GetImageDataResponse *resp) override;

    // Image Analysis
    grpc::Status
    GetSegmentationScore(ServerContext *context,
                         const api::GetSegmentationScoreRequest *req,
                         api::GetSegmentationScoreResponse *resp) override;

    grpc::Status QuantifyRegions(ServerContext *context,
                                 const api::QuantifyRegionsRequest *req,
                                 api::QuantifyRegionsResponse *resp) override;

private:
    std::shared_ptr<grpc::Server> server;

    ExperimentControl *exp;
};

#endif // APISERVER_H
