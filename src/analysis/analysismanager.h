#ifndef ANALYSISMANAGER_H
#define ANALYSISMANAGER_H

#include <string>
#include <map>
#include <vector>
#include <tuple>
#include <filesystem>

#include "analysis/segmentation.h"
#include "utils/hdf5file.h"

struct QuantificationResults;

class ExperimentControl;

class AnalysisManager {
public:
    AnalysisManager(ExperimentControl *exp);
    ~AnalysisManager();
    void LoadFile();

    xt::xarray<double> GetSegmentationScore(std::string ndimage_name, std::string ch_name, int i_t);
    int QuantifyRegions(std::string ndimage_name, int i_t, std::string segmentation_ch);

private:
    ExperimentControl *exp;
    HDF5File *h5file = nullptr;

    UNet unet;

    std::map<std::tuple<std::string, int>, QuantificationResults> quantifications;
};

struct QuantificationResults {
    std::vector<ImageRegionProp> region_props;

    std::vector<std::string> ch_names;
    std::vector<xt::xarray<float>> raw_intensity_mean;
};

#endif
