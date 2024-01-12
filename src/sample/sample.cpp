#include "sample.h"

#include <fmt/format.h>

PlateType PlateTypeFromString(std::string value)
{
    if (value == "slide") {
        return PlateType::Slide;
    } else if (value == "wellplate96") {
        return PlateType::Wellplate96;
    } else if (value == "wellplate384") {
        return PlateType::Wellplate384;
    } else {
        throw std::invalid_argument(
            fmt::format("invalid PlateType '{}'", value));
    }
}

std::string PlateTypeToString(PlateType plate_type)
{
    switch (plate_type) {
    case PlateType::Slide:
        return "slide";
    case PlateType::Wellplate96:
        return "wellplate96";
    case PlateType::Wellplate384:
        return "wellplate384";
    default:
        throw std::invalid_argument("invalid PlateType");
    }
}

Plate::Plate(PlateType plate_type, std::string plate_id)
{
    if (plate_id.empty()) {
        throw std::invalid_argument("plate_id cannot be empty");
    }

    this->uuid = utils::uuid();
    this->type = plate_type;
    this->id = plate_id;
    createWells(plate_type);
}

Plate::~Plate()
{
    for (::Well *well : wells) {
        delete well;
    }
    wells.clear();
    well_map.clear();
}

int Plate::Index() const { return index; }

std::string Plate::UUID() const { return uuid; }

PlateType Plate::Type() const { return type; }


std::string Plate::ID() const { return id; }

const std::optional<Pos2D> Plate::PositionOrigin() const { return pos_origin; }

const nlohmann::ordered_json Plate::Metadata() const { return metadata; }

::Well *Plate::Well(std::string well_id) const
{
    auto it = well_map.find(well_id);
    if (it == well_map.end()) {
        return nullptr;
    }
    return it->second;
}

const std::vector<::Well *> Plate::Wells() const { return wells; }

const std::vector<::Well *> Plate::EnabledWells() const
{
    std::vector<::Well *> results;
    for (const auto &well : wells) {
        if (well->Enabled()) {
            results.push_back(well);
        }
    }
    return results;
}

int Plate::NumWells() const { return wells.size(); }

int Plate::NumEnabledWells() const
{
    int n_enabled = 0;
    for (const auto &well : wells) {
        if (well->Enabled()) {
            n_enabled++;
        }
    }
    return n_enabled;
}

void Plate::createWells(PlateType plate_type)
{
    if (plate_type == PlateType::Slide) {
        addWell(new ::Well(this, "", Pos2D{0, 0}));
        return;
    }

    int n_rows;
    int n_cols;
    double spacing_x;
    double spacing_y;

    switch (type) {
    case PlateType::Wellplate96:
        n_rows = 8;
        n_cols = 12;
        spacing_x = -9000;
        spacing_y = -9000;
        break;
    case PlateType::Wellplate384:
        n_rows = 16;
        n_cols = 24;
        spacing_x = -4500;
        spacing_y = -4500;
        break;
    default:
        throw std::invalid_argument("invalid plate type");
    }

    for (int i_row = 0; i_row < n_rows; i_row++) {
        for (int i_col = 0; i_col < n_cols; i_col++) {
            std::string col_id = fmt::format("{:02d}", i_col + 1);
            std::string row_id = fmt::format("{:c}", 'A' + i_row);
            std::string well_id = fmt::format("{}{}", row_id, col_id);

            Pos2D rel_pos;
            rel_pos.x = i_col * spacing_x;
            rel_pos.y = i_row * spacing_y;

            addWell(new ::Well(this, well_id, rel_pos));
        }
    }
}

void Plate::addWell(::Well *well)
{
    auto it = well_map.find(well->ID());
    if (it != well_map.end()) {
        throw std::invalid_argument(
            fmt::format("cannot add well with duplicated id '{}'", well->ID()));
    }
    well->index = wells.size();
    wells.push_back(well);
    well_map[well->ID()] = well;
    return;
}

Well::Well(::Plate *plate, std::string id, Pos2D rel_pos)
{
    if (plate == nullptr) {
        throw std::invalid_argument("plate cannot be null");
    }
    this->plate = plate;

    this->uuid = utils::uuid();
    this->id = id;
    this->rel_pos = rel_pos;
    this->enabled = false;
}

Well::~Well() { clearSites(); }

int Well::Index() const { return index; }

std::string Well::UUID() const { return uuid; }

std::string Well::ID() const { return id; }

Pos2D Well::RelativePosition() const { return this->rel_pos; }

const std::optional<Pos2D> Well::Position() const
{
    std::optional<Pos2D> plate_pos = plate->PositionOrigin();
    if (!plate_pos.has_value()) {
        return std::nullopt;
    }
    Pos2D pos;
    pos.x = plate_pos.value().x + rel_pos.x;
    pos.y = plate_pos.value().y + rel_pos.y;
    return pos;
}

bool Well::Enabled() const { return this->enabled; }

const nlohmann::ordered_json Well::Metadata() const { return metadata; }

::Plate *Well::Plate() const { return this->plate; }

::Site *Well::Site(std::string site_id)
{
    std::shared_lock<std::shared_mutex> lk(sites_mutex);

    auto it = site_map.find(site_id);
    if (it == site_map.end()) {
        return nullptr;
    }
    return it->second;
}

const std::vector<::Site *> Well::Sites() const { return sites; }

int Well::NumSites() const { return sites.size(); }

int Well::NumEnabledSites() const
{
    int n_enabled = 0;
    for (const auto &site : sites) {
        if (site->Enabled()) {
            n_enabled++;
        }
    }
    return n_enabled;
}

void Well::addSite(::Site *site)
{
    std::unique_lock<std::shared_mutex> lk(sites_mutex);
    auto it = site_map.find(site->ID());
    if (it != site_map.end()) {
        throw std::invalid_argument(
            fmt::format("cannot add site with duplicated id '{}'", site->ID()));
    }
    site->index = sites.size();
    sites.push_back(site);
    site_map[site->ID()] = site;
    return;
}

void Well::clearSites()
{
    for (::Site *site : sites) {
        delete site;
    }
    sites.clear();
    site_map.clear();
}

Site::Site(::Well *well, std::string id, Pos2D rel_pos)
{
    if (well == nullptr) {
        throw std::invalid_argument("well cannot be null");
    }
    if (id.empty()) {
        throw std::invalid_argument("id cannot be empty");
    }

    this->well = well;

    this->uuid = utils::uuid();
    this->id = id;
    this->rel_pos = rel_pos;
}

int Site::Index() const { return index; }

std::string Site::UUID() const { return uuid; }

std::string Site::ID() const { return id; }

Pos2D Site::RelativePosition() const { return this->rel_pos; }

const std::optional<Pos2D> Site::Position() const
{
    std::optional<Pos2D> well_pos = well->Position();
    if (!well_pos.has_value()) {
        return std::nullopt;
    }
    Pos2D pos;
    pos.x = well_pos.value().x + rel_pos.x;
    pos.y = well_pos.value().y + rel_pos.y;
    return pos;
}

bool Site::Enabled() const { return this->enabled; }

const nlohmann::ordered_json Site::Metadata() const { return this->metadata; }

Well *Site::Well() const { return this->well; }
