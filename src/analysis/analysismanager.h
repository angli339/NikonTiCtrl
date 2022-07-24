#ifndef ANALYSISMANAGER_H
#define ANALYSISMANAGER_H

#include <string>
#include <map>
#include <vector>
#include <tuple>
#include <filesystem>

#include "analysis/segmentation.h"

struct QuantificationResults;

class ExperimentControl;

class AnalysisManager {
public:
    AnalysisManager(ExperimentControl *exp);

    ImageData GetSegmentationScore(std::string ndimage_name, std::string ch_name, int i_z, int i_t);
    int QuantifyRegions(std::string ndimage_name, int i_z, int i_t, std::string segmentation_ch);
private:
    ExperimentControl *exp;
    std::filesystem::path GetSegmentationLabelDir();
    std::filesystem::path GetQuantificationDir();

    UNet unet;

    std::map<std::tuple<std::string, int, int>, QuantificationResults> quantifications;
};

struct QuantificationResults {
    int n_regions;
    std::vector<ImageRegionProp> region_props;
    std::vector<std::string> ch_names;
    std::vector<std::vector<double>> ch_means;
};

#endif
