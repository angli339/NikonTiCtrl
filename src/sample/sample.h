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

#include "channel.h"
#include "utils/uuid.h"

class SampleManager;
class Plate;
class Well;
class Site;

enum class PlateType
{
    Unknown,
    Slide,
    Wellplate96,
    Wellplate384
};

NLOHMANN_JSON_SERIALIZE_ENUM(
    PlateType, {
                           {PlateType::Unknown, nullptr},
                           {PlateType::Slide, "Slide"},
                           {PlateType::Wellplate96, "Wellplate96"},
                           {PlateType::Wellplate384, "Wellplate384"},
                       })

struct Pos2D {
    double x;
    double y;
};

class Plate {
    friend class SampleManager;
public:
    Plate(PlateType type, std::string id, std::string uuid="");
    ~Plate();
    
    PlateType Type() const;
    std::string ID() const;
    std::string UUID() const;
    std::string Name() const;
    std::optional<Pos2D> PositionOrigin() const;

    void SetName(std::string name);
    void SetPositionOrigin(double x, double y);

    ::Well *Well(std::string id) const;
    std::vector<::Well *> Wells() const;

private:
    Plate() {}
    PlateType type;
    std::string id;
    std::string uuid;
    std::string name;
    std::optional<Pos2D> pos_origin;

    std::vector<::Well *> wells;
    std::map<std::string, ::Well *> well_map;
    ::Well *addWell(std::string id, Pos2D rel_pos);
};

class Well {
    friend class SampleManager;
public:
    Well(const ::Plate *plate, std::string id, Pos2D rel_pos, std::string uuid="");
    ~Well();

    std::string ID() const;
    std::string UUID() const;
    Pos2D RelativePosition() const;
    std::optional<Pos2D> Position() const;
    bool IsEnabled() const;
    std::string PresetName() const;

    void Enable(bool enabled=true);
    void SetPresetName(std::string preset_name);

    const ::Plate *Plate() const;
    std::vector<Site *> Sites();
    int NumSites() const;
    Site *NewSite(std::string id, std::string name, Pos2D rel_pos);

private:
    Well() {}
    const ::Plate *plate;
    std::string id;
    std::string uuid;
    Pos2D rel_pos;

    bool enabled;
    std::string preset_name;

    std::shared_mutex sites_mutex;
    std::vector<Site *> sites;
    std::set<std::string> site_id_set;
};

class Site {
    friend class SampleManager;
public:
    Site(const ::Well *well, std::string id, std::string name, Pos2D rel_pos);

    std::string ID() const;
    std::string Name() const;
    Pos2D RelativePosition() const;
    std::optional<Pos2D> Position() const;

    const ::Well *Well() const;

private:
    Site() {}
    const ::Well *well;
    std::string id;
    std::string name;
    Pos2D rel_pos;
};

#endif
