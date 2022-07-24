#include "image/imagemanager.h"

#include <ctime>
#include <fmt/chrono.h>
#include <fmt/os.h>
#include <stdexcept>
#include <nlohmann/json.hpp>

#include "config.h"
#include "logging.h"
#include "image/imageio.h"
#include "experimentcontrol.h"

ImageManager::ImageManager(ExperimentControl *exp)
{
    this->exp = exp;
}

ImageManager::~ImageManager()
{
    std::unique_lock<std::shared_mutex> lk(dataset_mutex);
    for (NDImage *ndimage : dataset) {
        delete ndimage;
    }
    dataset.clear();
    dataset_map.clear();
}

void ImageManager::LoadFromDB()
{
    std::unique_lock<std::shared_mutex> lk(dataset_mutex);
    for (NDImage *ndimage : dataset) {
        delete ndimage;
    }
    dataset.clear();
    dataset_map.clear();

    for (const auto& ndimage_row : exp->DB()->GetAllNDImages()) {
        NDImage *ndimage = new NDImage;
        ndimage->index = ndimage_row.index;
        ndimage->name = ndimage_row.name;
        ndimage->channel_names = ndimage_row.ch_names.get<std::vector<std::string>>();
        ndimage->width = ndimage_row.width;
        ndimage->height = ndimage_row.height;
        ndimage->n_ch = ndimage_row.n_ch;
        ndimage->n_z = ndimage_row.n_z;
        ndimage->n_t = ndimage_row.n_t;
        ndimage->dtype = DataType::Uint16;
        ndimage->ctype = ColorType::Mono16;

        dataset.push_back(ndimage);
        dataset_map[ndimage->name] = ndimage;
    }

    for (const auto& image_row : exp->DB()->GetAllImages()) {
        NDImage *ndimage = dataset_map[image_row.ndimage_name];
        int i_ch = ndimage->ChannelIndex(image_row.ch_name);
        int i_z = image_row.i_z;
        int i_t = image_row.i_t;
        ndimage->relpath_map[{i_ch, i_z, i_t}] = image_row.path;
    }
}

void ImageManager::writeNDImageRow(NDImage *ndimage)
{
    NDImageRow row = NDImageRow{
        .index = ndimage->Index(),
        .name = ndimage->Name(),
        .ch_names = ndimage->ChannelNames(),
        .width = ndimage->Width(),
        .height = ndimage->Height(),
        .n_ch = ndimage->NChannels(),
        .n_z = ndimage->NDimZ(),
        .n_t = ndimage->NDimT(),
    };
    if (ndimage->Site()) {
        row.plate_id = ndimage->Site()->Well()->Plate()->ID();
        row.well_id = ndimage->Site()->Well()->ID();
        row.site_id = ndimage->Site()->ID();
    }
    exp->DB()->InsertOrReplaceRow(row);
}

void ImageManager::writeImageRow(NDImage *ndimage, int i_ch, int i_z, int i_t)
{
    ImageRow row = ImageRow{
        .ndimage_name = ndimage->Name(),
        .ch_name = ndimage->ChannelName(i_ch),
        .i_z = i_z,
        .i_t = i_t,
        .path = ndimage->relpath_map[{i_ch, i_z, i_t}].string(),
        .exposure_ms = 0,
    };
    exp->DB()->InsertOrReplaceRow(row);
}

void ImageManager::SetLiveViewFrame(ImageData new_frame)
{
    std::lock_guard<std::mutex> lk(mutex_live_frame);
    live_view_frame = new_frame;
    new_frame_set = true;
    cv_live_frame.notify_all();
}

ImageData ImageManager::GetNextLiveViewFrame()
{
    std::unique_lock<std::mutex> lk(mutex_live_frame);
    cv_live_frame.wait(lk, [this] { return new_frame_set; });
    new_frame_set = false;
    return live_view_frame;
}

std::vector<NDImage *> ImageManager::ListNDImage() { return dataset; }

bool ImageManager::HasNDImage(std::string ndimage_name)
{
    std::shared_lock<std::shared_mutex> lk(dataset_mutex);

    auto it = dataset_map.find(ndimage_name);
    if (it == dataset_map.end()) {
        return false;
    }
    return true;
}

NDImage *ImageManager::GetNDImage(std::string ndimage_name)
{
    std::shared_lock<std::shared_mutex> lk(dataset_mutex);

    auto it = dataset_map.find(ndimage_name);
    if (it == dataset_map.end()) {
        return nullptr;
    }
    return it->second;
}

void ImageManager::NewNDImage(std::string ndimage_name,
                             std::vector<std::string> ch_names, Site *site)
{
    std::filesystem::path im_path = GetImageDir();

    std::unique_lock<std::shared_mutex> lk(dataset_mutex);

    auto it = dataset_map.find(ndimage_name);
    if (it != dataset_map.end()) {
        // Overwrite
        delete it->second;
        dataset_map.erase(it);
    }

    // Create NDImage
    NDImage *ndimage = new NDImage(ndimage_name, ch_names);
    ndimage->index = dataset.size();
    ndimage->site = site;
    ndimage->exp_dir = exp->ExperimentDir();
    dataset.push_back(ndimage);
    dataset_map[ndimage_name] = ndimage;

    // Write to DB
    exp->DB()->BeginTransaction();
    try {
        writeNDImageRow(ndimage);
        exp->DB()->Commit();
    } catch (std::exception &e) {
        dataset.pop_back();
        dataset_map.erase(ndimage_name);
        exp->DB()->Rollback();
        throw std::runtime_error(fmt::format("cannot write NDImage to DB: {}, rolled back", e.what()));
    }

    lk.unlock();

    SendEvent({
        .type = EventType::NDImageCreated,
        .value = ndimage_name,
    });
}

void ImageManager::AddImage(std::string ndimage_name, int i_ch, int i_z, int i_t,
                           ImageData data, nlohmann::ordered_json metadata)

{
    NDImage *ndimage = GetNDImage(ndimage_name);
    if (ndimage == nullptr) {
        throw std::invalid_argument("name not exists");
    }

    ndimage->AddImage(i_ch, i_z, i_t, data, metadata);
    ndimage->width = data.Width();
    ndimage->height = data.Height();
    
    std::string filename = fmt::format("{}-{}-{:03d}-{:04d}.tif", ndimage->name, ndimage->channel_names[i_ch], i_z, i_t);
    std::filesystem::path relpath = fmt::format("images/{}", filename);
    std::filesystem::path fullpath = exp->ExperimentDir() / relpath;
    TiffMetadata tiff_meta;
    tiff_meta.metadata = ndimage->metadata_map[{i_ch, i_z, i_t}];
    ImageWrite(fullpath, data, tiff_meta);
    ndimage->exp_dir = exp->ExperimentDir();
    ndimage->relpath_map[{i_ch, i_z, i_t}] = relpath;

    // Write to DB
    exp->DB()->BeginTransaction();
    try {
        writeNDImageRow(ndimage);
        writeImageRow(ndimage, i_ch, i_z, i_t);
        exp->DB()->Commit();
    } catch (std::exception &e) {
        dataset.pop_back();
        dataset_map.erase(ndimage_name);
        exp->DB()->Rollback();
        throw std::runtime_error(fmt::format("cannot write NDImage to DB: {}, rolled back", e.what()));
    }

    SendEvent({
        .type = EventType::NDImageChanged,
        .value = ndimage_name,
    });
}

std::filesystem::path ImageManager::GetImageDir() {
    std::filesystem::path exp_dir = exp->ExperimentDir();
    if (exp_dir.empty()) {
        throw std::runtime_error("experiment dir not set");
    }

    // Create if not exists
    std::filesystem::path path = exp_dir / "images";
    if (!std::filesystem::exists(path)) {
        if (!std::filesystem::create_directories(path)) {
            throw std::runtime_error(
                fmt::format("failed to create dir {}", path.string()));
        }
    }

    return path;
 }
