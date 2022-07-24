#include "image/ndimage.h"

#include "image/imageio.h"
#include "utils/uuid.h"
#include "version.h"

NDImage::NDImage(std::string name, std::vector<std::string> channel_names)
{
    this->name = name;
    this->channel_names = channel_names;
    n_ch = channel_names.size();
}

::Site *NDImage::Site()
{
    return site;
}

int NDImage::Index()
{
    return index;
}

std::string NDImage::Name()
{
    return name;
}

int NDImage::NumImages()
{
    return dataset.size();
}

int NDImage::Width()
{
    return width;
}

int NDImage::Height()
{
    return height;
}

int NDImage::NChannels()
{
    return n_ch;
}

int NDImage::NDimZ()
{
    return n_z;
}

int NDImage::NDimT()
{
    return n_t;
}

::DataType NDImage::DataType() 
{
    return dtype;
}

::ColorType NDImage::ColorType()
{
    return ctype;
}

std::vector<std::string> NDImage::ChannelNames()
{
    return channel_names;
}

std::string NDImage::ChannelName(int i_ch)
{
    return channel_names[i_ch];
}

int NDImage::ChannelIndex(std::string channel_name)
{
    for (int i = 0; i < channel_names.size(); i++) {
        if (channel_names[i] == channel_name) {
            return i;
        }
    }
    return -1;
}

void NDImage::AddImage(int i_ch, int i_z, int i_t, ImageData data,
                       nlohmann::ordered_json metadata)
{
    if ((i_ch < 0) || (i_ch >= n_ch)) {
        throw std::out_of_range("i_ch out of range");
    }
    if (i_z < 0) {
        throw std::out_of_range("i_z out of range");
    }
    if (i_t < 0) {
        throw std::out_of_range("i_t out of range");
    }

    // Prepend NDImage info to metadata
    nlohmann::ordered_json new_metadata;
    new_metadata["ndimage"] = {
        {"name", name}, {"channel", channel_names[i_ch]},
        {"i_ch", i_ch}, {"i_z", i_z},
        {"i_t", i_t},
    };
    for (const auto &[k, v] : metadata.items()) {
        new_metadata[k] = v;
    }

    // Add image data and metadata
    if (i_z >= n_z) {
        n_z = i_z + 1;
    }
    if (i_t >= n_t) {
        n_t = i_t + 1;
    }
    dataset[{i_ch, i_z, i_t}] = data;
    metadata_map[{i_ch, i_z, i_t}] = new_metadata;
}

bool NDImage::HasData(int i_ch, int i_z, int i_t)
{
    auto it = relpath_map.find({i_ch, i_z, i_t});
    if (it == relpath_map.end()) {
        return false;
    }
    return true;
}

ImageData NDImage::GetData(int i_ch, int i_z, int i_t)
{
    auto it_file = relpath_map.find({i_ch, i_z, i_t});
    if (it_file == relpath_map.end()) {
        throw std::invalid_argument("index not found");
    }
    std::filesystem::path relpath = it_file->second;
    std::filesystem::path fullpath = exp_dir / relpath;

    auto it_data = dataset.find({i_ch, i_z, i_t});
    if (it_data != dataset.end()) {
        return it_data->second;
    } else {
        ImageData data = ImageRead(fullpath);
        dataset[{i_ch, i_z, i_t}] = data;
        return data;
    }
}
