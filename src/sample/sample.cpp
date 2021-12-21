#include "sample.h"

#include <fmt/format.h>

SampleArray::SampleArray(std::string id, std::string name,
                         SampleArrayLayout layout)
{
    this->uuid = utils::uuid();
    this->id = id;
    this->name = name;
    this->layout = layout;
    switch (layout) {
    case SampleArrayLayout::Wellplate96:
        spacing_x = -9000;
        spacing_y = -9000;
        break;
    case SampleArrayLayout::Wellplate384:
        spacing_x = -4500;
        spacing_y = -4500;
        break;
    default:
        throw std::invalid_argument("invalid layout");
    }
}

SampleArray::~SampleArray()
{
    std::unique_lock<std::shared_mutex> lk(samples_mutex);
    for (Sample *sample : samples) {
        delete sample;
    }
}

bool SampleArray::PosIDValid(std::string pos_id) const
{
    if (pos_id.size() < 2) {
        return false;
    }
    int i_row, i_col;
    std::tie(i_row, i_col) = PosIDToRowCol(pos_id);

    switch (layout) {
    case SampleArrayLayout::Wellplate96:
        if ((i_row < 0) || (i_row >= 8)) {
            return false;
        }
        if ((i_col < 0) || (i_col >= 12)) {
            return false;
        }
        return true;
    case SampleArrayLayout::Wellplate384:
        if ((i_row < 0) || (i_row >= 16)) {
            return false;
        }
        if ((i_col < 0) || (i_col >= 24)) {
            return false;
        }
        return true;
    default:
        throw std::invalid_argument("invalid layout");
    }
}

std::pair<int, int> SampleArray::PosIDToRowCol(std::string pos_id) const
{
    const char row_name = pos_id[0];
    int i_row = row_name - 'A';
    int i_col = std::stoi(pos_id.substr(1, std::string::npos)) - 1;
    return {i_row, i_col};
}

std::string SampleArray::RowColToPosID(int i_row, int i_col) const
{
    switch (layout) {
    case SampleArrayLayout::Wellplate96:
        if ((i_row < 0) || (i_row >= 8)) {
            throw std::out_of_range("i_row out of range");
        }
        if ((i_col < 0) || (i_col >= 12)) {
            throw std::out_of_range("i_col out of range");
        }
        break;
    case SampleArrayLayout::Wellplate384:
        if ((i_row < 0) || (i_row >= 16)) {
            throw std::out_of_range("i_row out of range");
        }
        if ((i_col < 0) || (i_col >= 24)) {
            throw std::out_of_range("i_col out of range");
        }
        break;
    default:
        throw std::invalid_argument("invalid layout");
    }

    char row_name = 'A' + i_row;
    std::string col_name = fmt::format("{:02d}", i_col + 1);
    return row_name + col_name;
}

Sample *SampleArray::newSample(std::string pos_id, std::string name)
{
    if (id_set.contains(pos_id)) {
        throw std::invalid_argument("pos_id already exists");
    }
    Sample *sample = new Sample(pos_id, name, this);
    samples.push_back(sample);
    id_set.insert(pos_id);
    id_sample_map[pos_id] = sample;
    return sample;
}

Sample *SampleArray::NewSample(std::string pos_id, std::string name)
{
    std::unique_lock<std::shared_mutex> lk(samples_mutex);
    return newSample(pos_id, name);
}

void SampleArray::CreateSamplesRange(std::string pos_id, int n_row, int n_col)
{
    std::unique_lock<std::shared_mutex> lk(samples_mutex);
    if (!PosIDValid(pos_id)) {
        throw std::invalid_argument("invalid pos_id");
    }

    int offset_i_row, offset_i_col;
    std::tie(offset_i_row, offset_i_col) = PosIDToRowCol(pos_id);
    for (int i_row = 0; i_row < n_row; i_row++) {
        if (i_row % 2 == 0) {
            for (int i_col = 0; i_col < n_col; i_col++) {
                std::string pos_id =
                    RowColToPosID(i_row + offset_i_row, i_col + offset_i_col);
                newSample(pos_id, "");
            }
        } else {
            for (int i_col = n_col - 1; i_col >= 0; i_col--) {
                std::string pos_id =
                    RowColToPosID(i_row + offset_i_row, i_col + offset_i_col);
                newSample(pos_id, "");
            }
        }
    }
}

Sample *SampleArray::GetSample(std::string pos_id)
{
    std::shared_lock<std::shared_mutex> lk(samples_mutex);
    auto it = id_sample_map.find(pos_id);
    if (it == id_sample_map.end()) {
        return nullptr;
    }
    return it->second;
}

int SampleArray::TotalNumSites() const
{
    int total = 0;
    for (const auto &sample : samples) {
        total += sample->NumSites();
    }
    return total;
}

std::vector<std::string> SampleArray::GetChannelNames() const
{
    std::vector<std::string> names;
    names.reserve(channels.size());
    for (const auto &channel : channels) {
        names.push_back(channel.preset_name);
    }
    return names;
}

Sample::Sample(std::string id, std::string name)
{
    if (id.empty()) {
        throw std::invalid_argument("id cannot be empty");
    }
    this->uuid = utils::uuid();
    this->id = id;
    this->name = name;
}

Sample::Sample(std::string pos_id, std::string name, const SampleArray *parent)
{
    if (parent == nullptr) {
        throw std::invalid_argument("parent SampleArray cannot be null");
    }
    if (!parent->PosIDValid(pos_id)) {
        throw std::invalid_argument("invalid pos_id");
    }

    this->uuid = utils::uuid();
    this->id = pos_id;
    this->name = name;
    this->parent = parent;
}

Sample::~Sample()
{
    for (Site *site : sites) {
        delete site;
    }
}

std::string Sample::FullID() const
{
    if (parent) {
        return parent->ID() + "-" + id;
    }
    return id;
}

bool Sample::PositionValid() const
{
    if (parent) {
        return parent->RefPositionValid();
    }
    return pos_x.has_value() && pos_y.has_value();
}

std::pair<double, double> Sample::Position() const
{
    if (parent) {
        // Check local overwrite
        if (pos_x.has_value() && pos_y.has_value()) {
            return {pos_x.value(), pos_y.value()};
        }

        int i_row, i_col;
        std::tie(i_row, i_col) = parent->PosIDToRowCol(id);

        return {parent->RefPositionX() + i_col * parent->SampleSpacingX(),
                parent->RefPositionY() + i_row * parent->SampleSpacingY()};
    }
    return {pos_x.value(), pos_y.value()};
}

double Sample::PositionX() const
{
    if (parent) {
        if (pos_x.has_value()) {
            return pos_x.value();
        }
        int i_row, i_col;
        std::tie(i_row, i_col) = parent->PosIDToRowCol(id);

        return parent->RefPositionX() + i_col * parent->SampleSpacingX();
    }
    return pos_x.value();
}

double Sample::PositionY() const
{
    if (parent) {
        if (pos_y.has_value()) {
            return pos_y.value();
        }
        int i_row, i_col;
        std::tie(i_row, i_col) = parent->PosIDToRowCol(id);

        return parent->RefPositionY() + i_row * parent->SampleSpacingY();
    }
    return pos_y.value();
}

void Sample::SetPosition(double x, double y)
{
    if (parent) {
        throw std::runtime_error(
            "sample is part of an array, OverwritePosition() should be used to "
            "overwrite the position based on position id");
    }
    pos_x = x;
    pos_y = y;
}

void Sample::OverwritePosition(double x, double y)
{
    if (!parent) {
        throw std::runtime_error("sample is not part of an array, use "
                                 "SetPosition() to set the position");
    }
    pos_x = x;
    pos_y = y;
}

Site *Sample::NewSite(std::string id, std::string name)
{
    std::unique_lock<std::shared_mutex> lk(sites_mutex);
    if (id_set.contains(id)) {
        throw std::invalid_argument("id already exists");
    }
    Site *site = new Site(id, name, this);
    sites.push_back(site);
    id_set.insert(id);
    return site;
}

Site *Sample::NewSite(std::string id, std::string name, double delta_x,
                      double delta_y)
{
    std::unique_lock<std::shared_mutex> lk(sites_mutex);
    return newSite(id, name, delta_x, delta_y);
}

Site *Sample::newSite(std::string id, std::string name, double delta_x,
                      double delta_y)
{
    if (id_set.contains(id)) {
        throw std::invalid_argument("id already exists");
    }
    Site *site = new Site(id, name, this);
    site->SetRelativePosition(delta_x, delta_y);
    sites.push_back(site);
    id_set.insert(id);
    return site;
}

void Sample::CreateSitesOnCenteredGrid(int n_col, int n_row, double spacing_x,
                                       double spacing_y)
{
    if (n_col < 1) {
        throw std::invalid_argument("invalid n_col or n_row");
    }
    if (n_row < 1) {
        throw std::invalid_argument("invalid n_col or n_row");
    }

    double corner_x = -((double)(n_col - 1) / 2) * spacing_x;
    double corner_y = -((double)(n_row - 1) / 2) * spacing_y;

    int i_site = 0;
    int n_site = n_col * n_row;
    if (n_site >= 1000000) {
        throw std::invalid_argument(
            "too many sites, site_id formatter is not implemented");
    }

    std::unique_lock<std::shared_mutex> lk(sites_mutex);

    for (int i_row = 0; i_row < n_row; i_row++) {
        if (i_row % 2 == 0) {
            for (int i_col = 0; i_col < n_col; i_col++) {
                double x = corner_x + i_col * spacing_x;
                double y = corner_y + i_row * spacing_y;
                std::string site_id;
                if (n_site < 1000) {
                    site_id = fmt::format("{:03d}", i_site);
                } else if (n_site < 10000) {
                    site_id = fmt::format("{:04d}", i_site);
                } else if (n_site < 100000) {
                    site_id = fmt::format("{:05d}", i_site);
                } else if (n_site < 1000000) {
                    site_id = fmt::format("{:06d}", i_site);
                }
                newSite(site_id, "", x, y);
                i_site++;
            }
        } else {
            for (int i_col = n_col - 1; i_col >= 0; i_col--) {
                double x = corner_x + i_col * spacing_x;
                double y = corner_y + i_row * spacing_y;
                std::string site_id;
                if (n_site < 1000) {
                    site_id = fmt::format("{:03d}", i_site);
                } else if (n_site < 10000) {
                    site_id = fmt::format("{:04d}", i_site);
                } else if (n_site < 100000) {
                    site_id = fmt::format("{:05d}", i_site);
                } else if (n_site < 1000000) {
                    site_id = fmt::format("{:06d}", i_site);
                }
                newSite(site_id, "", x, y);
                i_site++;
            }
        }
    }
}

bool Sample::ChannelsValid() const
{
    if (!channels.empty()) {
        return true;
    } else if (parent) {
        return parent->ChannelsValid();
    } else {
        return false;
    }
}

std::vector<std::string> Sample::GetChannelNames() const
{
    if (!channels.empty()) {
        std::vector<std::string> names;
        for (const auto &channel : channels) {
            names.push_back(channel.preset_name);
        }
        return names;
    } else if (parent) {
        return parent->GetChannelNames();
    } else {
        return {};
    }
}

std::vector<Channel> Sample::GetChannels() const
{
    if (!channels.empty()) {
        return channels;
    } else if (parent) {
        return parent->GetChannels();
    } else {
        return {};
    }
}

Site::Site(std::string id, std::string name, const Sample *parent)
{
    if (id.empty()) {
        throw std::invalid_argument("id cannot be empty");
    }
    if (parent == nullptr) {
        throw std::invalid_argument("parent cannot be null");
    }

    this->id = id;
    this->name = name;
    this->parent = parent;
}

std::pair<double, double> Site::Position() const
{
    double sample_pos_x, sample_pos_y;
    std::tie(sample_pos_x, sample_pos_y) = parent->Position();
    return {
        sample_pos_x + delta_x.value(),
        sample_pos_y + delta_y.value(),
    };
}

bool Site::ChannelsValid() const
{
    if (!channels.empty()) {
        return true;
    } else if (parent) {
        return parent->ChannelsValid();
    } else {
        return false;
    }
}

std::vector<std::string> Site::GetChannelNames() const
{
    if (!channels.empty()) {
        std::vector<std::string> names;
        names.reserve(channels.size());
        for (const auto &channel : channels) {
            names.push_back(channel.preset_name);
        }
        return names;
    } else if (parent) {
        return parent->GetChannelNames();
    } else {
        return {};
    }
}

std::vector<Channel> Site::GetChannels() const
{
    if (!channels.empty()) {
        return channels;
    } else if (parent) {
        return parent->GetChannels();
    } else {
        return {};
    }
}

void to_json(json &j, const SampleArray &array)
{
    j["uuid"] = array.uuid;
    j["id"] = array.id;
    j["name"] = array.name;
    j["layout"] = array.layout;
    j["description"] = array.description;

    if (array.ref_pos_x.has_value() && array.ref_pos_y.has_value()) {
        j["ref_pos_x"] = array.ref_pos_x.value();
        j["ref_pos_y"] = array.ref_pos_y.value();
    }
    j["spacing_x"] = array.spacing_x;
    j["spacing_y"] = array.spacing_y;

    if (!array.channels.empty()) {
        for (const auto &ch : array.channels) {
            j["channels"].push_back({
                {"preset_name", ch.preset_name},
                {"exposure_ms", ch.exposure_ms},
                {"illumination_intensity", ch.illumination_intensity},
            });
        }
    }

    j["samples"] = json::array();
    for (auto &samples : array.samples) {
        j["samples"].push_back(json(*samples));
    }
}

void to_json(json &j, const Sample &sample)
{
    j["uuid"] = sample.uuid;
    j["id"] = sample.id;
    j["name"] = sample.name;
    j["description"] = sample.description;

    if (sample.pos_x.has_value() && sample.pos_y.has_value()) {
        j["pos_x"] = sample.pos_x.value();
        j["pos_y"] = sample.pos_y.value();
    }

    if (!sample.channels.empty()) {
        for (const auto &ch : sample.channels) {
            j["channels"].push_back({
                {"preset_name", ch.preset_name},
                {"exposure_ms", ch.exposure_ms},
                {"illumination_intensity", ch.illumination_intensity},
            });
        }
    }

    j["sites"] = json::array();
    for (auto &site : sample.sites) {
        j["sites"].push_back(json(*site));
    }
}

void to_json(json &j, const Site &site)
{
    j["id"] = site.id;
    j["name"] = site.name;
    j["description"] = site.description;

    if (site.delta_x.has_value() && site.delta_y.has_value()) {
        j["delta_x"] = site.delta_x.value();
        j["delta_y"] = site.delta_y.value();
    }

    if (!site.channels.empty()) {
        for (const auto &ch : site.channels) {
            j["channels"].push_back({
                {"preset_name", ch.preset_name},
                {"exposure_ms", ch.exposure_ms},
                {"illumination_intensity", ch.illumination_intensity},
            });
        }
    }
}
