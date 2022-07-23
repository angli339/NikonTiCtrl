#ifndef NDIMAGE_H
#define NDIMAGE_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "image/imagedata.h"
#include "sample/sample.h"

class ImageManager;

class NDImage {
    friend class ImageManager;
public:
    NDImage(std::string name, std::vector<std::string> channel_names);

    ::Site *Site();

    int Index();
    std::string Name();
    int NumImages();

    int Width();
    int Height();
    int NChannels();
    int NDimZ();
    int NDimT();

    ::DataType DataType();
    ::ColorType ColorType();

    std::vector<std::string> ChannelNames();
    std::string ChannelName(int i_ch);
    int ChannelIndex(std::string);

    void SetFolder(std::filesystem::path folder);

    void AddImage(int i_ch, int i_z, int i_t, ImageData data,
                  nlohmann::ordered_json metadata);
    void SaveImage(int i_ch, int i_z, int i_t);

    bool HasData(int i_ch, int i_z, int i_t);
    ImageData GetData(int i_ch, int i_z, int i_t);

private:
    NDImage() {}
    
    ::Site *site = nullptr;

    int index = -1;
    std::string name;
    std::vector<std::string> channel_names;

    uint32_t width = 0;
    uint32_t height = 0;
    int n_ch = 0;
    int n_z = 0;
    int n_t = 0;

    ::DataType dtype;
    ::ColorType ctype;
    std::optional<double> pixel_size_um;

    std::map<std::tuple<int, int, int>, ImageData> dataset;
    std::map<std::tuple<int, int, int>, nlohmann::ordered_json> metadata_map;
    std::map<std::tuple<int, int, int>, std::filesystem::path> relpath_map;

    std::filesystem::path exp_dir;
    // std::string getImageName(int i_ch, int i_z, int i_t);
};

#endif
