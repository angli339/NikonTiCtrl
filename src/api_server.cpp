#include "api_server.h"

#include <stdexcept>

#include <QtConcurrentRun>
#include <QThread>

#include "logger.h"
#include "utils/string_utils.h"

std::string api_server_address("0.0.0.0:50051");

Status APIServer::GetProperty(ServerContext* context, const api::GetPropertyRequest* req, api::GetPropertyResponse* resp)
{
    //
    // Get from cache
    //
    if (req->from_cache()) {
        auto cache = dev->getCachedDeviceProperty();

        // Get all properties in cache
        if ((req->name_size() == 1) && (req->name(0) == "*")) {
            for (const auto& [name, value] : cache) {
                auto property = resp->add_property();
                property->set_name(name);
                property->set_value(value);
            }
            return Status::OK;
        }

        // Get selected properties from cache
        for (const auto& name : req->name()) {
            auto it = cache.find(name);
            if (it == cache.end()) {
                return Status(grpc::StatusCode::NOT_FOUND, fmt::format("property {} not found in cache", name));;
            }
            auto value = it->second;

            auto property = resp->add_property();
            property->set_name(name);
            property->set_value(value);
        }
        return Status::OK;
    }

    //
    // Get property from devices
    //
    for (const auto& name : req->name()) {
        std::string value;        
        try {
            if (util::hasPrefix(name, "Task")) {
                value = taskControl->getTaskProperty(util::trimPrefix(name, "Task/"));
            } else {
                value = dev->getDeviceProperty(name);
            }
        } catch (const std::invalid_argument& e) {
            return Status(grpc::StatusCode::INVALID_ARGUMENT, fmt::format("failed to get {}, invalid argument: {}", name, e.what()));
        } catch (const std::runtime_error& e) {
            return Status(grpc::StatusCode::INTERNAL, fmt::format("failed to get {}, runtime error: {}", name, e.what()));
        }

        auto property = resp->add_property();
        property->set_name(name);
        property->set_value(value); 
    }
    return Status::OK;
}


Status APIServer::SetProperty(ServerContext* context, const api::SetPropertyRequest* req, google::protobuf::Empty* resp)
{
    for (const auto& property : req->property()) {
        std::string name = property.name();
        std::string value = property.value();
        try {
            if (util::hasPrefix(name, "Task")) {
                taskControl->setTaskProperty(util::trimPrefix(name, "Task/"), value);
            } else {
                dev->setDeviceProperty(name, value);
            }
        } catch (const std::invalid_argument& e) {
            return Status(grpc::StatusCode::INVALID_ARGUMENT, fmt::format("failed to set {}={}, invalid argument: {}", name, value, e.what()));
        } catch (const std::runtime_error& e) {
            return Status(grpc::StatusCode::INTERNAL, fmt::format("failed to set {}={}, rumtime error: {}", name, value, e.what()));
        }
    }
    return Status::OK;
}

Status APIServer::WaitProperty(ServerContext* context, const api::WaitPropertyRequest* req, google::protobuf::Empty* resp)
{
    std::vector<std::string> propertyList(req->name().begin(), req->name().end());
    std::chrono::nanoseconds timeout;
    timeout = std::chrono::seconds(req->timeout().seconds()) + std::chrono::nanoseconds(req->timeout().nanos());
    if (timeout == std::chrono::nanoseconds(0)) {
        timeout = std::chrono::seconds(30);
    }
    
    bool status;
    try {
        status = dev->waitDeviceProperty(propertyList, std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
    } catch (const std::invalid_argument& e) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, fmt::format("failed to wait property, invalid argument: {}", e.what()));
    } catch (const std::runtime_error& e) {
        return Status(grpc::StatusCode::INTERNAL, fmt::format("failed to wait property, rumtime error: {}", e.what()));
    }
    if (status) {
        return Status::OK;
    } else {
        return Status(grpc::StatusCode::DEADLINE_EXCEEDED, "timeout");
    }
}

Status APIServer::ListProperty(ServerContext* context, const api::ListPropertyRequest* req, api::ListPropertyResponse* resp)
{
    std::vector<std::string> propertylist;

    try {
        std::string name = req->name();
        if (util::hasPrefix(name, "Task")) {
            propertylist = taskControl->listTaskProperty(util::trimPrefix(name, "Task/"));
            for (auto& p : propertylist) {
                p = "Task/" + p;
            }
        } else {
            propertylist = dev->listDeviceProperty(req->name());
            if (name == "") {
                propertylist.push_back("Task");
            }
        }
        
        
    } catch (const std::invalid_argument& e) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, fmt::format("failed to list property '{}', invalid argument: {}", req->name(), e.what()));
    } catch (const std::runtime_error& e) {
        return Status(grpc::StatusCode::INTERNAL, fmt::format("failed to list property '{}', rumtime error: {}", req->name(), e.what()));
    }

    for (const auto& name : propertylist) {
        resp->add_name(name);
    }
    return Status::OK;
}

Status APIServer::SnapImage(ServerContext* context, const api::SnapImageRequest* req, api::ImageID* resp)
{
    std::string id;
    try {
        id = taskControl->captureImage(req->name());
    } catch (const std::invalid_argument& e) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, fmt::format("captureImage failed, invalid argument: {}", e.what()));
    } catch (const std::runtime_error& e) {
        return Status(grpc::StatusCode::INTERNAL, fmt::format("captureImage failed, rumtime error: {}", e.what()));
    }
    
    resp->set_id(id);
    return Status::OK;
}

Status APIServer::GetImage(ServerContext* context, const api::ImageID* req, api::Image* resp)
{
    Image *im;

    try {
        im = dataManager->getImage(req->id());
    } catch (const std::invalid_argument& e) {
        return Status(grpc::StatusCode::NOT_FOUND, fmt::format("get image '{}': {}", req->id(), e.what()));
    }
    
    resp->set_width(im->width);
    resp->set_height(im->height);
    if (im->buf != NULL) {
        // because reading image from file is not yet implemented
        resp->set_data(im->buf, im->bufSize);
    }

    switch (im->pixelFormat) {
    case PixelMono8:
        resp->set_pixel_type(api::Image_PixelType::Image_PixelType_MONO8);
        break;
    case PixelMono16:
        resp->set_pixel_type(api::Image_PixelType::Image_PixelType_MONO16);
        break;
    }
    resp->set_timestamp(im->timestamp);

    return Status::OK;
}

Status APIServer::LiveImage(grpc::ServerContext* context, const api::LiveImageRequest* req, grpc::ServerWriter<api::Image>* writer)
{
    taskControl->setTaskState("Live");
    for (int i = 0; i < req->n_image(); i++) {
        Image *im = dataManager->getNextLiveImage();
        if (im == nullptr) {
            return Status::OK;
        }
        api::Image resp;
        resp.set_width(im->width);
        resp.set_height(im->height);
        if (im->buf != NULL) {
            // because reading image from file is not yet implemented
            resp.set_data(im->buf, im->bufSize);
        }

        switch (im->pixelFormat) {
        case PixelMono8:
            resp.set_pixel_type(api::Image_PixelType::Image_PixelType_MONO8);
            break;
        case PixelMono16:
            resp.set_pixel_type(api::Image_PixelType::Image_PixelType_MONO16);
            break;
        }
        resp.set_timestamp(im->timestamp);

        bool status = writer->Write(resp);
        if (!status) {
            break;
        }
    }
    taskControl->setTaskState("Stop");
    return Status::OK;
}

namespace {
  std::shared_ptr<grpc::Server> buildAndStartService(APIServer *service)
  {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(api_server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    return server;
  }
}

APIServer::APIServer(QObject *parent)
  : QObject(parent)
  , server(buildAndStartService(this))
{
    if (this->server == nullptr) {
        throw std::runtime_error(fmt::format("failed to listen {}", api_server_address));
    }
    QtConcurrent::run([=] {
        LOG_INFO("APIServer: listening {}", api_server_address);
        this->server->Wait();
    });
}
