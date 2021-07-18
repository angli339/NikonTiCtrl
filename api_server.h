#ifndef APISERVER_H
#define APISERVER_H

#include <grpcpp/grpcpp.h>
#include <memory>
#include <QObject>

#include "api.grpc.pb.h"
#include "api.pb.h"
#include "datamanager.h"
#include "devicecontrol.h"
#include "taskcontrol.h"

using grpc::Status;
using grpc::ServerContext;

class APIServer final : public QObject, public api::NikonTiCtrl::Service
{
    Q_OBJECT
public:
    explicit APIServer(QObject *parent = nullptr);

    void setDeviceControl(DeviceControl *dev) { this->dev = dev; }
    void setDataManager(DataManager *dataManager) { this->dataManager = dataManager; }
    void setTaskControl(TaskControl *taskControl) { this->taskControl = taskControl; }

    Status GetProperty(ServerContext* context, const api::GetPropertyRequest* req, api::GetPropertyResponse* resp) override;
    Status SetProperty(ServerContext* context, const api::SetPropertyRequest* req, google::protobuf::Empty* resp) override;
    Status WaitProperty(ServerContext* context, const api::WaitPropertyRequest* req, google::protobuf::Empty* resp) override;
    Status ListProperty(ServerContext* context, const api::ListPropertyRequest* req, api::ListPropertyResponse* resp) override;
    Status SnapImage(ServerContext* context, const api::SnapImageRequest* req, api::ImageID* resp) override;
    Status GetImage(ServerContext* context, const api::ImageID* req, api::Image* resp) override;
    Status LiveImage(grpc::ServerContext* context, const api::LiveImageRequest* req, grpc::ServerWriter<api::Image>* writer) override;

private:
    std::shared_ptr<grpc::Server> server;

    DeviceControl *dev;
    TaskControl *taskControl;
    DataManager *dataManager;
};

#endif // APISERVER_H
