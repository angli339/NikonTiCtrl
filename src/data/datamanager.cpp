#include "data/datamanager.h"

#include <ctime>
#include <fmt/chrono.h>
#include <stdexcept>

#include "config.h"
#include "logging.h"

DataManager::DataManager() : unet(config.unet_grpc_addr) {}

void DataManager::SetLiveViewFrame(ImageData new_frame)
{
    std::lock_guard<std::mutex> lk(mutex_live_frame);
    live_view_frame = new_frame;
    new_frame_set = true;
    cv_live_frame.notify_all();
}

ImageData DataManager::GetNextLiveViewFrame()
{
    std::unique_lock<std::mutex> lk(mutex_live_frame);
    cv_live_frame.wait(lk, [this] { return new_frame_set; });
    new_frame_set = false;
    return live_view_frame;
}

std::filesystem::path DataManager::ExperimentPath()
{
    return exp_path;
}

void DataManager::SetExperimentPath(std::filesystem::path path)
{
    // Default path
    if (path.empty()) {
        std::time_t t = std::time(nullptr);
        std::string exp_name = fmt::format("{:%Y-%m-%d}", fmt::localtime(t));
        path = std::filesystem::path(config.data_root) / exp_name;
    }
    if (!fs::exists(path)) {
        if (!fs::create_directories(path)) {
            throw std::runtime_error(
                fmt::format("failed to create dir {}", path.string()));
        }
    }

    // Create sub-dir
    std::filesystem::path image_path = path / "images";
    if (!fs::exists(image_path)) {
        if (!fs::create_directories(image_path)) {
            throw std::runtime_error(
                fmt::format("failed to create dir {}", image_path.string()));
        }
    }

    this->exp_path = path;

    SendEvent({
        .type = EventType::ExperimentPathChanged,
        .value = path.string(),
    });
}

std::filesystem::path DataManager::ImagePath()
{
    return exp_path / "images";
}

std::vector<NDImage *> DataManager::ListNDImage()
{
    return dataset;
}

bool DataManager::HasNDImage(std::string ndimage_name)
{
    std::shared_lock<std::shared_mutex> lk(dataset_mutex);

    auto it = dataset_map.find(ndimage_name);
    if (it == dataset_map.end()) {
        return false;
    }
    return true;
}

NDImage *DataManager::GetNDImage(std::string ndimage_name)
{
    std::shared_lock<std::shared_mutex> lk(dataset_mutex);

    auto it = dataset_map.find(ndimage_name);
    if (it == dataset_map.end()) {
        return nullptr;
    }
    return it->second;
}

void DataManager::NewNDImage(std::string ndimage_name,
                             std::vector<NDImageChannel> channel_info)
{
    std::unique_lock<std::shared_mutex> lk(dataset_mutex);

    auto it = dataset_map.find(ndimage_name);
    if (it != dataset_map.end()) {
        if (it->second->ChannelInfo() == channel_info) {
            return;
        }
        throw std::invalid_argument(
            "duplicated name but different channel_infof");
    }

    // Create data directory if not exists
    if (exp_path.empty()) {
        SetExperimentPath("");
    }

    NDImage *ndimage = new NDImage(ndimage_name, channel_info);
    ndimage->SetFolder(ImagePath());
    dataset.push_back(ndimage);
    dataset_map[ndimage_name] = ndimage;

    lk.unlock();

    SendEvent({
        .type = EventType::NDImageCreated,
        .value = ndimage_name,
    });
}

void DataManager::AddImage(std::string ndimage_name, int i_ch, int i_z, int i_t,
                           ImageData data, nlohmann::ordered_json metadata)

{
    NDImage *ndimage = GetNDImage(ndimage_name);
    if (ndimage == nullptr) {
        throw std::invalid_argument("name not exists");
    }

    ndimage->AddImage(i_ch, i_z, i_t, data, metadata);
    ndimage->SaveImage(i_ch, i_z, i_t);

    SendEvent({
        .type = EventType::NDImageChanged,
        .value = ndimage_name,
    });
}
