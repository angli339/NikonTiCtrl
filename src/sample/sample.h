#ifndef SAMPLE_H
#define SAMPLE_H

#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
using json = nlohmann::ordered_json;

#include "channel.h"
#include "utils/uuid.h"

class SampleArray;
class Sample;
class Site;

enum class SampleArrayLayout
{
    Unknown,
    Wellplate96,
    Wellplate384
};

NLOHMANN_JSON_SERIALIZE_ENUM(
    SampleArrayLayout, {
                           {SampleArrayLayout::Unknown, nullptr},
                           {SampleArrayLayout::Wellplate96, "Wellplate96"},
                           {SampleArrayLayout::Wellplate384, "Wellplate384"},
                       })

class SampleArray {
public:
    SampleArray(std::string id, std::string name, SampleArrayLayout layout);
    ~SampleArray();

    std::string UUID() const
    {
        return uuid;
    }
    std::string ID() const
    {
        return id;
    }
    std::string Name() const
    {
        return name;
    }
    void SetName(std::string name)
    {
        this->name = name;
    }
    SampleArrayLayout Layout() const
    {
        return layout;
    }
    std::string Description() const
    {
        return description;
    }
    void SetDescription(std::string description)
    {
        this->description = description;
    }

    bool PosIDValid(std::string pos_id) const;
    std::pair<int, int> PosIDToRowCol(std::string pos_id) const;
    std::string RowColToPosID(int i_row, int i_col) const;

    Sample *NewSample(std::string pos_id, std::string name);
    void CreateSamplesRange(std::string pos_id, int n_row, int n_col);

    Sample *GetSample(std::string pos_id);
    std::vector<Sample *> GetSamples()
    {
        return samples;
    }
    int NumSamples() const
    {
        return samples.size();
    }
    int TotalNumSites() const;

    void SetRefPosition(double x, double y)
    {
        ref_pos_x = x;
        ref_pos_y = y;
    }
    bool RefPositionValid() const
    {
        return ref_pos_x.has_value() && ref_pos_y.has_value();
    }
    std::pair<double, double> RefPosition() const
    {
        return {ref_pos_x.value(), ref_pos_y.value()};
    }
    double RefPositionX() const
    {
        return ref_pos_x.value();
    }
    double RefPositionY() const
    {
        return ref_pos_y.value();
    }

    std::pair<double, double> SampleSpacing() const
    {
        return {spacing_x, spacing_y};
    }
    double SampleSpacingX() const
    {
        return spacing_x;
    }
    double SampleSpacingY() const
    {
        return spacing_y;
    }

    void SetChannels(std::vector<Channel> channels)
    {
        this->channels = channels;
    };
    bool ChannelsValid() const
    {
        return !this->channels.empty();
    }
    std::vector<std::string> GetChannelNames() const;
    std::vector<Channel> GetChannels() const
    {
        return this->channels;
    }

    friend void to_json(json &j, const SampleArray &array);

private:
    std::string uuid;
    std::string id;
    std::string name;
    SampleArrayLayout layout;
    std::string description;

    std::optional<double> ref_pos_x;
    std::optional<double> ref_pos_y;
    double spacing_x;
    double spacing_y;

    std::vector<Channel> channels;

    std::shared_mutex samples_mutex;
    // data container
    std::vector<Sample *> samples;
    // index
    std::set<std::string> id_set;
    std::map<std::string, Sample *> id_sample_map;
    // add sample without locking the mutex
    Sample *newSample(std::string pos_id, std::string name);
};

class Sample {
public:
    Sample(std::string id, std::string name);
    Sample(std::string pos_id, std::string name, const SampleArray *parent);
    ~Sample();

    std::string UUID() const
    {
        return uuid;
    }
    std::string FullID() const;
    std::string ID() const
    {
        return id;
    }
    std::string Name() const
    {
        return name;
    }
    void SetName(std::string name)
    {
        this->name = name;
    }
    std::string Description() const
    {
        return description;
    }
    void SetDescription(std::string description)
    {
        this->description = description;
    }

    bool PositionValid() const;
    std::pair<double, double> Position() const;
    double PositionX() const;
    double PositionY() const;
    void SetPosition(double x, double y); // if Sample is not part of an array
    void OverwritePosition(double x, double y); // if Sample is part of an array

    Site *NewSite(std::string id, std::string name);
    Site *NewSite(std::string id, std::string name, double delta_x,
                  double delta_y);
    void CreateSitesOnCenteredGrid(int n_col, int n_row, double spacing_x,
                                   double spacing_y);

    const SampleArray *Parent() const
    {
        return parent;
    }
    int NumSites() const
    {
        return sites.size();
    }
    std::vector<Site *> GetSites()
    {
        return sites;
    }

    void SetChannels(std::vector<Channel> channels)
    {
        this->channels = channels;
    }
    bool ChannelsValid() const;
    std::vector<std::string> GetChannelNames() const;
    std::vector<Channel> GetChannels() const;

    friend void to_json(json &j, const Sample &sample);

private:
    // metadata
    std::string uuid;
    std::string id; // well_id in the case of a wellplate
    std::string name;
    std::string description;

    // position, if Sample is not part of an array
    // overwriting position, if Sample is part of an array
    std::optional<double> pos_x;
    std::optional<double> pos_y;

    // channels
    std::vector<Channel> channels;

    const SampleArray *parent = nullptr;

    std::shared_mutex sites_mutex;
    // data container
    std::vector<Site *> sites;
    // index
    std::set<std::string> id_set;
    // add site without locking the mutex
    Site *newSite(std::string id, std::string name, double delta_x,
                  double delta_y);
};

class Site {
public:
    Site(std::string id, std::string name, const Sample *parent);

    std::string FullID() const
    {
        return parent->FullID() + "-" + id;
    }
    std::string ID() const
    {
        return id;
    }
    std::string Name() const
    {
        return name;
    }
    void SetName(std::string name)
    {
        this->name = name;
    }
    std::string Description() const
    {
        return description;
    }
    void SetDescription(std::string description)
    {
        this->description = description;
    }

    void SetRelativePosition(double delta_x, double delta_y)
    {
        this->delta_x = delta_x;
        this->delta_y = delta_y;
    }
    bool RelativePositionValid() const
    {
        return delta_x.has_value() && delta_y.has_value();
    }
    std::pair<double, double> RelativePosition() const
    {
        return {delta_x.value(), delta_y.value()};
    }
    double RelativePositionX() const
    {
        return delta_x.value();
    }
    double RelativePositionY() const
    {
        return delta_y.value();
    }

    bool PositionValid() const
    {
        return parent->PositionValid() && RelativePositionValid();
    }
    std::pair<double, double> Position() const;
    double PositionX() const
    {
        return parent->PositionX() + delta_x.value();
    }
    double PositionY() const
    {
        return parent->PositionY() + delta_y.value();
    }

    void SetChannels(std::vector<Channel> channels)
    {
        this->channels = channels;
    }
    bool ChannelsValid() const;
    std::vector<std::string> GetChannelNames() const;
    std::vector<Channel> GetChannels() const;

    friend void to_json(json &j, const Site &site);

private:
    std::string id;
    std::string name;
    std::string description;

    std::optional<double> delta_x;
    std::optional<double> delta_y;

    std::vector<Channel> channels;

    const Sample *parent;
};

#endif
