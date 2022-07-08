#ifndef NDIMAGE_H
#define NDIMAGE_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "data/imagedata.h"

struct NDImageChannel {
    std::string name;
    uint32_t width;
    uint32_t height;
    DataType dtype;
    ColorType ctype;
    std::optional<double> pixel_size_um;

    bool operator==(const NDImageChannel &) const = default;
};

class NDImage {
public:
    NDImage(std::string name, std::vector<NDImageChannel> channel_info);

    std::string Name();
    int NumImages();
    int NChannels();
    int NDimZ();
    int NDimT();
    std::vector<NDImageChannel> ChannelInfo();
    std::string ChannelName(int i_ch);
    int ChannelIndex(std::string);

    void SetFolder(std::filesystem::path folder);

    void AddImage(int i_ch, int i_z, int i_t, ImageData data,
                  nlohmann::ordered_json metadata);
    void SaveImage(int i_ch, int i_z, int i_t);

    bool HasData(int i_ch, int i_z, int i_t);
    ImageData GetData(int i_ch, int i_z, int i_t);

private:
    std::string name;
    std::vector<NDImageChannel> channel_info;

    int n_ch = 0;
    int n_z = 0;
    int n_t = 0;

    std::map<std::tuple<int, int, int>, ImageData> dataset;
    std::map<std::tuple<int, int, int>, nlohmann::ordered_json> metadata_map;

    std::filesystem::path folder;
    std::string getImageName(int i_ch, int i_z, int i_t);
};

#endif
