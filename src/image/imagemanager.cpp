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
    : unet(config.system.unet_model.server_addr,
           config.system.unet_model.model_name,
           config.system.unet_model.input_name,
           config.system.unet_model.output_name)
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
        ndimage->filepath_map[{i_ch, i_z, i_t}] = exp->ExperimentDir() / image_row.path;
    }
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
                             std::vector<std::string> ch_names)
{
    std::filesystem::path im_path = GetImageDir();

    std::unique_lock<std::shared_mutex> lk(dataset_mutex);

    auto it = dataset_map.find(ndimage_name);
    if (it != dataset_map.end()) {
        // Overwrite
        delete it->second;
        dataset_map.erase(it);
    }

    NDImage *ndimage = new NDImage(ndimage_name, ch_names);
    ndimage->SetFolder(im_path);
    dataset.push_back(ndimage);
    dataset_map[ndimage_name] = ndimage;

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
    ndimage->SaveImage(i_ch, i_z, i_t);

    SendEvent({
        .type = EventType::NDImageChanged,
        .value = ndimage_name,
    });
}

int ImageManager::QuantifyRegions(std::string ndimage_name, int i_z, int i_t,
                                std::string segmentation_ch)
{
    // Find image
    NDImage *ndimage = GetNDImage(ndimage_name);
    int i_ch = ndimage->ChannelIndex(segmentation_ch);
    ImageData im_raw = ndimage->GetData(i_ch, i_z, i_t);

    //
    // Segmentation
    //

    LOG_DEBUG("Segment {}", ndimage_name);
    // Preprocess
    ImageData im_eq = im::EqualizeCLAHE(im_raw);

    // U-Net
    ImageData im_score = unet.GetScore(im_eq);

    // Segment score image and calculate mean score of regions
    std::vector<im::ImageRegionProp> region_prop;
    ImageData im_labels = im::RegionLabel(im_score, region_prop);
    std::vector<double> score_mean =
        im::RegionMean(im_score, im_labels, region_prop);

    // Remove low-score regions and renumber the labels
    std::vector<im::ImageRegionProp> region_prop_filtered;
    im::ImageRegionProp rp0; // placeholder
    rp0.label = 0;
    rp0.area = 1;
    region_prop_filtered.push_back(rp0);

    int n_regions = 0;
    for (int old_label = 0; old_label < score_mean.size(); old_label++) {
        region_prop[old_label].mean_score = score_mean[old_label];
        if (score_mean[old_label] > 0.9) {
            n_regions++;
            region_prop[old_label].label = n_regions;
            region_prop_filtered.push_back(region_prop[old_label]);
        } else {
            region_prop[old_label].label = 0;
        }
    }

    // Renumber labels in label image
    uint16_t *im_labels_buf = reinterpret_cast<uint16_t *>(im_labels.Buf().get());
    for (int i = 0; i < im_labels.size(); i++) {
        uint16_t old_label = im_labels_buf[i];
        im_labels_buf[i] = region_prop[old_label].label;
    }

    LOG_DEBUG("Segmentation completed: {}/{} passed filter", region_prop_filtered.size()-1, region_prop.size());
    
    // Save label image
    TiffMetadata tiff_meta;
    tiff_meta.metadata["model_name"] = config.system.unet_model.model_name;
    tiff_meta.metadata["segmentation_channel"] = segmentation_ch;

    std::filesystem::path im_label_dir = GetSegmentationLabelDir();
    std::string im_label_filename = fmt::format("{}-{:03d}-{:04d}.tif", ndimage_name, i_z, i_t);
    std::filesystem::path im_label_path = im_label_dir / im_label_filename;

    ImageWrite(im_label_path, im_labels, tiff_meta);

    LOG_DEBUG("Label image saved");

    //
    // Quantification
    //
    QuantificationResults results;
    results.n_regions = n_regions;
    results.region_props = region_prop_filtered;

    for (int i_ch = 0; i_ch < ndimage->NChannels(); i_ch++) {
        ImageData im_ch = ndimage->GetData(i_ch, i_z, i_t);
        std::vector<double> ch_mean = im::RegionMean(im_ch, im_labels, region_prop_filtered);

        results.ch_names.push_back(ndimage->ChannelName(i_ch));
        results.ch_means.push_back(ch_mean);
    }
    quantifications[{ndimage_name, i_z, i_t}] = results;
    
    std::filesystem::path im_quant_dir = GetQuantificationDir();
    std::string im_quant_filename = fmt::format("{}-{:03d}-{:04d}.csv", ndimage_name, i_z, i_t);
    std::filesystem::path im_quant_path = im_quant_dir / im_quant_filename;
    
    auto out = fmt::output_file(im_quant_path.string().c_str(), O_CREAT|O_WRONLY|O_TRUNC);
    out.print("label,bbox_x0,bbox_y0,bbox_width,bbox_height,area,centroid_x,centroid_y,mean_score");
    for (int i_ch = 0; i_ch < ndimage->NChannels(); i_ch++) {
        out.print(",mean_{}", results.ch_names[i_ch]);
    }
    out.print("\n");

    for (int i = 1; i < region_prop_filtered.size(); i++) {
        im::ImageRegionProp rp = region_prop_filtered[i];
        out.print("{},{},{},{},{},", rp.label, rp.bbox_x0, rp.bbox_y0, rp.bbox_width, rp.bbox_height);
        out.print("{},{},{},{}", rp.area, rp.centroid_x, rp.centroid_y, rp.mean_score);
        for (int i_ch = 0; i_ch < ndimage->NChannels(); i_ch++) {
            out.print(",{}", results.ch_means[i_ch][i]);
        }
        out.print("\n");
    }

    LOG_DEBUG("Quantification completed");

    return n_regions;
}

std::filesystem::path ImageManager::GetImageDir() {
    std::filesystem::path exp_dir = exp->ExperimentDir();
    if (exp_dir.empty()) {
        throw std::runtime_error("experiment dir not set");
    }

    // Create if not exists
    std::filesystem::path path = exp_dir / "images";
    if (!fs::exists(path)) {
        if (!fs::create_directories(path)) {
            throw std::runtime_error(
                fmt::format("failed to create dir {}", path.string()));
        }
    }

    return path;
 }

 std::filesystem::path ImageManager::GetSegmentationLabelDir() {
    std::filesystem::path exp_dir = exp->ExperimentDir();
    if (exp_dir.empty()) {
        throw std::runtime_error("experiment dir not set");
    }

    // Create if not exists
    std::filesystem::path path = exp_dir / "images";
    if (!fs::exists(path)) {
        if (!fs::create_directories(path)) {
            throw std::runtime_error(
                fmt::format("failed to create dir {}", path.string()));
        }
    }

    return path;
 }

 std::filesystem::path ImageManager::GetQuantificationDir() {
    std::filesystem::path exp_dir = exp->ExperimentDir();
    if (exp_dir.empty()) {
        throw std::runtime_error("experiment dir not set");
    }

    // Create if not exists
    std::filesystem::path path = exp_dir / "analysis" / "quantification";
    if (!fs::exists(path)) {
        if (!fs::create_directories(path)) {
            throw std::runtime_error(
                fmt::format("failed to create dir {}", path.string()));
        }
    }

    return path;
 }