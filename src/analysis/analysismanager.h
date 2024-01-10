#ifndef ANALYSISMANAGER_H
#define ANALYSISMANAGER_H

#include <filesystem>
#include <map>
#include <string>
#include <tuple>
#include <vector>
#include <shared_mutex>

#include "eventstream.h"
#include "analysis/utils.h"
#include "utils/hdf5file.h"

struct QuantificationResults;

class ExperimentControl;

class AnalysisManager : public EventSender {
public:
    AnalysisManager(ExperimentControl *exp);
    ~AnalysisManager();
    void LoadFile();

    xt::xarray<double> GetSegmentationScore(std::string ndimage_name, int i_t,
                                            std::string ch_name);
    int QuantifyRegions(std::string ndimage_name, int i_t,
                        std::string segmentation_ch);

    std::vector<std::string> GetNDImageNames();
    bool HasQuantification(std::string ndimage_name, int i_t);
    QuantificationResults GetQuantification(std::string ndimage_name, int i_t);

private:
    ExperimentControl *exp;
    HDF5File *h5file = nullptr;

    UNet unet;

    std::shared_mutex mutex_quant;
    std::vector<std::string> ndimage_names;
    std::map<std::tuple<std::string, int>, QuantificationResults>
        quantifications;
};

struct QuantificationResults {
    std::vector<ImageRegionProp> region_props;
    std::vector<double> unet_score;

    std::vector<std::string> ch_names;
    std::vector<xt::xarray<float>> raw_intensity_mean;
};

#endif
