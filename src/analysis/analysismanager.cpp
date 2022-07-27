#include "analysismanager.h"

#include <fmt/os.h>
#include <xtensor/xadapt.hpp>
#include <xtensor/xview.hpp>

#include "config.h"
#include "logging.h"
#include "image/imageio.h"
#include "experimentcontrol.h"

AnalysisManager::AnalysisManager(ExperimentControl *exp)
    : unet(config.system.unet_model.server_addr,
           config.system.unet_model.model_name,
           config.system.unet_model.input_name,
           config.system.unet_model.output_name)
{
    this->exp = exp;
    if (!exp->ExperimentDir().empty()) {
        h5file = new HDF5File(exp->ExperimentDir() / "analysis.h5");
    }
}

AnalysisManager::~AnalysisManager()
{
    if (h5file) {
        delete h5file;
    }
}

void AnalysisManager::LoadFile()
{
    if (h5file) {
        delete h5file;
        h5file = nullptr;
    }
    if (!exp->ExperimentDir().empty()) {
        h5file = new HDF5File(exp->ExperimentDir() / "analysis.h5");
    }
}

xt::xarray<double> AnalysisManager::GetSegmentationScore(std::string ndimage_name, std::string ch_name, int i_t)
{
    // Find image
    NDImage *ndimage = exp->Images()->GetNDImage(ndimage_name);
    int i_ch = ndimage->ChannelIndex(ch_name);
    ImageData im_raw = ndimage->GetData(i_ch, 0, i_t);

    std::vector<size_t> shape = {im_raw.Height(), im_raw.Width()};
    auto im_raw_arr = xt::adapt((uint16_t *)im_raw.Buf().get(), im_raw.size(),
        xt::no_ownership(), shape);

    // Preprocess
    xt::xarray<uint16_t> im_eq = EqualizeCLAHE(im_raw_arr);

    // U-Net
    return unet.GetScore(im_eq);
}

int AnalysisManager::QuantifyRegions(std::string ndimage_name, int i_t,
                                std::string segmentation_ch)
{
    // Find image
    NDImage *ndimage = exp->Images()->GetNDImage(ndimage_name);
    int i_ch = ndimage->ChannelIndex(segmentation_ch);
    ImageData im_raw = ndimage->GetData(i_ch, 0, i_t);

    std::vector<size_t> shape = {im_raw.Height(), im_raw.Width()};
    auto im_raw_arr = xt::adapt((uint16_t *)im_raw.Buf().get(), im_raw.size(),
        xt::no_ownership(), shape);

    //
    // Segmentation
    //

    LOG_DEBUG("Segment {}", ndimage_name);
    // Preprocess
    xt::xarray<uint16_t> im_eq = EqualizeCLAHE(im_raw_arr);
    xt::xarray<float> im_eq_f32 = xt::xarray<float>(im_eq) / 65535;

    // U-Net
    xt::xarray<float> im_score = unet.GetScore(im_eq_f32);

    // Segment score image and calculate mean score of regions
    std::vector<ImageRegionProp> region_prop;
    xt::xarray<uint16_t> im_labels = RegionLabel(im_score, region_prop);
    xt::xarray<double> score_sum = RegionSum(im_score, im_labels, region_prop.size()-1);

    xt::xarray<double> area = xt::xarray<double>::from_shape({region_prop.size()});
    for (int i = 0; i < region_prop.size(); i++) {
        area[i] = region_prop[i].area;
    }
    xt::xarray<double> score_mean = score_sum / area;

    // Remove low-score regions
    std::vector<ImageRegionProp> region_prop_filtered;
    std::vector<double> score_mean_filtered;
    for (int i = 0; i < region_prop.size(); i++) {
        if (score_mean[i] > 0.9) {
            region_prop_filtered.push_back(region_prop[i]);
            score_mean_filtered.push_back(score_mean[i]);
        }
    }

    // Renumber the labels
    std::vector<uint16_t> newLabelFromOld;
    newLabelFromOld.resize(region_prop.size(), 0);
    newLabelFromOld[0] = 0; // label 0 is still label 0
    for (int i = 0; i < region_prop_filtered.size(); i++) {
        uint16_t old_label = region_prop_filtered[i].label;
        uint16_t new_label = i + 1;
        newLabelFromOld[old_label] = new_label;
        region_prop_filtered[i].label = new_label;
    }

    // Renumber labels in label image
    auto im_labels_fl = xt::flatten(im_labels);
    for (int i = 0; i < im_labels_fl.size(); i++) {
        uint16_t old_label = im_labels_fl[i];
        im_labels_fl[i] = newLabelFromOld[old_label];
    }

    LOG_DEBUG("Segmentation completed: {}/{} passed filter", region_prop_filtered.size(), region_prop.size());
    
    //
    // Save label image
    //
    std::string group_name = fmt::format("/segmentation/{}/{}", ndimage_name, i_t);
    h5file->write(fmt::format("{}/label_image", group_name), im_labels, true);
    h5file->flush();

    LOG_DEBUG("Label image saved");

    //
    // Quantification
    //
    xt::xarray<double> area_filtered = xt::xarray<double>::from_shape({region_prop_filtered.size()});
    for (int i = 0; i < region_prop_filtered.size(); i++) {
        area_filtered[i] = region_prop_filtered[i].area;
    }

    QuantificationResults results;
    results.region_props = region_prop_filtered;

    for (int i_ch = 0; i_ch < ndimage->NChannels(); i_ch++) {
        ImageData im_ch = ndimage->GetData(i_ch, 0, i_t);

        std::vector<size_t> shape = {im_ch.Height(), im_ch.Width()};
        xt::xarray<uint16_t> im_ch_arr = xt::adapt((uint16_t *)im_ch.Buf().get(), im_ch.size(),  xt::no_ownership(), shape);

        xt::xarray<double> ch_sum = RegionSum(im_ch_arr, im_labels, region_prop_filtered.back().label);
        xt::xarray<float> ch_mean = xt::view(ch_sum, xt::range(1, ch_sum.size())) / area_filtered;

        results.ch_names.push_back(ndimage->ChannelName(i_ch));
        results.raw_intensity_mean.push_back(ch_mean);
    }
    quantifications[{ndimage_name, i_t}] = results;
    
    //
    // Save quantification
    //
    StructArray rp_sarr({
        {"label", Dtype::uint16},
        {"bbox_x0", Dtype::uint32},
        {"bbox_y0", Dtype::uint32},
        {"bbox_width", Dtype::uint32},
        {"bbox_height", Dtype::uint32},
        {"area", Dtype::float64},
        {"centroid_x", Dtype::float64},
        {"centroid_y", Dtype::float64},
        {"score_mean", Dtype::float64},
    }, results.region_props.size());

    for (int i = 0 ; i < results.region_props.size(); i++) {
        rp_sarr.Field<uint16_t>("label")[i] = results.region_props[i].label;
        rp_sarr.Field<uint32_t>("bbox_x0")[i] = results.region_props[i].bbox_x0;
        rp_sarr.Field<uint32_t>("bbox_y0")[i] = results.region_props[i].bbox_y0;
        rp_sarr.Field<uint32_t>("bbox_width")[i] = results.region_props[i].bbox_width;
        rp_sarr.Field<uint32_t>("bbox_height")[i] = results.region_props[i].bbox_height;
        rp_sarr.Field<double>("area")[i] = results.region_props[i].area;
        rp_sarr.Field<double>("centroid_x")[i] = results.region_props[i].centroid_x;
        rp_sarr.Field<double>("centroid_y")[i] = results.region_props[i].centroid_y;
    }
    rp_sarr.Field<double>("score_mean") = xt::adapt(score_mean_filtered);

    h5file->write(fmt::format("{}/region_props", group_name), rp_sarr);

    StructArray raw_intensity_mean_sarr(results.ch_names, Dtype::float32, results.region_props.size());
    for (int i = 0; i < results.ch_names.size(); i++) {
        std::string ch_name = results.ch_names[i];
        raw_intensity_mean_sarr.Field<float>(ch_name) = results.raw_intensity_mean[i];
    }
    h5file->write(fmt::format("{}/raw_intensity_mean", group_name), raw_intensity_mean_sarr);
    h5file->flush();

    LOG_DEBUG("Quantification completed");

    return region_prop_filtered.size();
}
