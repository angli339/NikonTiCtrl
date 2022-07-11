#include "data/datamanager.h"

#include <ctime>
#include <fmt/chrono.h>
#include <fmt/os.h>
#include <stdexcept>

#include "config.h"
#include "logging.h"
#include "data/imageio.h"

DataManager::DataManager()
    : unet(config.system.unet_model.server_addr,
           config.system.unet_model.model_name,
           config.system.unet_model.input_name,
           config.system.unet_model.output_name)
{
}

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

std::filesystem::path DataManager::ExperimentPath() { return exp_path; }

void DataManager::SetExperimentPath(std::filesystem::path path)
{
    // Default path
    if (path.empty()) {
        std::time_t t = std::time(nullptr);
        std::string exp_name = fmt::format("{:%Y-%m-%d}", fmt::localtime(t));
        path = std::filesystem::path(config.user.data_root) / exp_name;
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

std::filesystem::path DataManager::ImagePath() { return exp_path / "images"; }

std::vector<NDImage *> DataManager::ListNDImage() { return dataset; }

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

int DataManager::QuantifyRegions(std::string ndimage_name, int i_z, int i_t,
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

    std::string im_label_filename = fmt::format("{}-{:03d}-{:04d}.tif", ndimage_name, i_z, i_t);
    std::filesystem::path im_label_dir= exp_path / "analysis" / "labels";
    std::filesystem::path im_label_path = im_label_dir / im_label_filename;
    if (!fs::exists(im_label_dir)) {
        if (!fs::create_directories(im_label_dir)) {
            throw std::runtime_error(
                fmt::format("failed to create dir {}", im_label_dir.string()));
        }
    }

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
    
    std::string im_quant_filename = fmt::format("{}-{:03d}-{:04d}.csv", ndimage_name, i_z, i_t);
    std::filesystem::path im_quant_dir = exp_path / "analysis" / "quantification";
    std::filesystem::path im_quant_path = im_quant_dir / im_quant_filename;
    if (!fs::exists(im_quant_dir)) {
        if (!fs::create_directories(im_quant_dir)) {
            throw std::runtime_error(
                fmt::format("failed to create dir {}", im_quant_dir.string()));
        }
    }

    auto out = fmt::output_file(im_quant_path.string().c_str(), O_CREAT|O_WRONLY|O_TRUNC);
    out.print("label,bbox_x0,bbox_y0,bbox_width,bbox_height,area,centroid_x,centroid_y,mean_score");
    for (int i_ch = 0; i_ch < ndimage->NChannels(); i_ch++) {
        out.print(",mean_{}", results.ch_names[i_ch]);
    }
    out.print("\n");

    for (int i = 1; i < region_prop_filtered.size(); i++) {
        im::ImageRegionProp rp = region_prop_filtered[i];
        out.print("{},{},{},{},{},", rp.label, rp.bbbox_x0, rp.bbbox_y0, rp.bbbox_width, rp.bbbox_height);
        out.print("{},{},{},{}", rp.area, rp.centroid_x, rp.centroid_y, rp.mean_score);
        for (int i_ch = 0; i_ch < ndimage->NChannels(); i_ch++) {
            out.print(",{}", results.ch_means[i_ch][i]);
        }
        out.print("\n");
    }

    LOG_DEBUG("Quantification completed");

    return n_regions;
}