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
    Slide,
    Wellplate96,
    Wellplate384
};

PlateType PlateTypeFromString(std::string value);
std::string PlateTypeToString(PlateType plate_type);

struct Pos2D {
    double x;
    double y;
};

class Plate {
    friend class SampleManager;
public:
    Plate(PlateType plate_type, std::string plate_id);
    ~Plate();
    
    int Index() const;
    std::string UUID() const;
    PlateType Type() const;
    std::string ID() const;
    const std::optional<Pos2D> PositionOrigin() const;
    const nlohmann::ordered_json Metadata() const;

    ::Well *Well(std::string well_id) const;
    const std::vector<::Well *> Wells() const;
    int NumWells() const;
    int NumEnabledWells() const;

private:
    Plate() {}

    int index;
    std::string uuid;
    PlateType type;
    std::string id;
    std::optional<Pos2D> pos_origin;
    nlohmann::ordered_json metadata;

    std::vector<::Well *> wells;
    std::map<std::string, ::Well *> well_map;
    void createWells(PlateType plate_type);
    void addWell(::Well *well);
};

class Well {
    friend class Plate;
    friend class SampleManager;
public:
    Well(::Plate *plate, std::string id, Pos2D rel_pos);
    ~Well();

    int Index() const;
    std::string ID() const;
    std::string UUID() const;
    Pos2D RelativePosition() const;
    const std::optional<Pos2D> Position() const;
    bool Enabled() const;
    const nlohmann::ordered_json Metadata() const;

    ::Plate *Plate() const;
    const std::vector<::Site *> Sites() const;
    ::Site *Site(std::string site_id);
    int NumSites() const;
    int NumEnabledSites() const;

private:
    Well() {}
    ::Plate *plate = nullptr;

    int index;
    std::string uuid;
    std::string id;
    Pos2D rel_pos;
    bool enabled = false;
    nlohmann::ordered_json metadata;

    std::shared_mutex sites_mutex;
    std::vector<::Site *> sites;
    std::map<std::string, ::Site *> site_map;
    void addSite(::Site *site);
    void clearSites();
};

class Site {
    friend class Well;
    friend class SampleManager;
public:
    Site(::Well *well, std::string id, Pos2D rel_pos);

    int Index() const;
    std::string UUID() const;
    std::string ID() const;
    Pos2D RelativePosition() const;
    const std::optional<Pos2D> Position() const;
    bool Enabled() const;
    const nlohmann::ordered_json Metadata() const;

    ::Well *Well() const;

private:
    Site() {}
    ::Well *well = nullptr;

    int index;
    std::string uuid;
    std::string id;
    std::string name;
    Pos2D rel_pos;
    bool enabled = true;
    nlohmann::ordered_json metadata;
};

#endif
