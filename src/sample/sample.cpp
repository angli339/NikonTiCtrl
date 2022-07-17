#include "sample.h"

#include <fmt/format.h>

Plate::Plate(PlateType type, std::string id, std::string uuid)
{
    if (id.empty()) {
        throw std::invalid_argument("id cannot be empty");
    }

    this->type = type;
    this->id = id;

    if (uuid.empty()) {
        this->uuid = utils::uuid();
    } else {
        this->uuid = uuid;
    }
    
    int n_rows;
    int n_cols;
    double spacing_x;
    double spacing_y;

    switch (type) {
    case PlateType::Slide:
        n_rows = 1;
        n_cols = 1;
        spacing_x = 0;
        spacing_y = 0;
        break;
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

    if (n_rows * n_cols == 1) {
        addWell("", Pos2D{0, 0});
        return;
    }

    for (int i_row = 0; i_row < n_rows; i_row++) {
        for (int i_col = 0; i_col < n_cols; i_col++) {
            std::string col_id = fmt::format("{:02d}", i_col + 1);
            std::string row_id = fmt::format("{}", 'A' + i_row);
            std::string well_id = fmt::format("{}{}", row_id, col_id);
            
            Pos2D rel_pos;
            rel_pos.x = i_col * spacing_x;
            rel_pos.y = i_row * spacing_y;
            
            addWell(well_id, rel_pos);
        }
    }
}

Plate::~Plate()
{
    for (::Well *well : wells) {
        delete well;
    }
}

PlateType Plate::Type() const
{
    return type;
}


std::string Plate::ID() const
{
    return id;
}

std::string Plate::UUID() const
{
    return uuid;
}

std::string Plate::Name() const
{
    return name;
}

std::optional<Pos2D> Plate::PositionOrigin() const
{
    return pos_origin;
}

void Plate::SetName(std::string name)
{
    this->name = name;
}

void Plate::SetPositionOrigin(double x, double y)
{
    this->pos_origin = Pos2D{x, y};
}

::Well *Plate::Well(std::string id) const
{
    auto it = well_map.find(id);
    if (it == well_map.end()) {
        return nullptr;
    }
    return it->second;
}

std::vector<::Well *> Plate::Wells() const
{
    return wells;
}

Well *Plate::addWell(std::string id, Pos2D rel_pos)
{
    auto it = well_map.find(id);
    if (it != well_map.end()) {
        throw std::invalid_argument("id already exists");
    }
    
    ::Well *well = new ::Well(this, id, rel_pos);
    wells.push_back(well);
    well_map[id] = well;
    return well;
}

Well::Well(const ::Plate *plate, std::string id, Pos2D rel_pos, std::string UUID)
{
    if (plate == nullptr) {
        throw std::invalid_argument("plate cannot be null");
    }

    if (uuid.empty()) {
        this->uuid = utils::uuid();
    } else {
        this->uuid = uuid;
    }
    this->plate = plate;
    this->id = id;
}

Well::~Well()
{
    for (Site *site : sites) {
        delete site;
    }
}

std::string Well::ID() const
{
    return id;
}

std::string Well::UUID() const
{
    return uuid;
}

Pos2D Well::RelativePosition() const
{
    return this->rel_pos;
}

std::optional<Pos2D> Well::Position() const
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

bool Well::IsEnabled() const
{
    return this->enabled;
}

std::string Well::PresetName() const
{
    return this->preset_name;
}

void Well::Enable(bool enabled)
{
    this->enabled = enabled;
}

void Well::SetPresetName(std::string preset_name)
{
    this->preset_name = preset_name;
}

const ::Plate *Well::Plate() const
{
    return this->plate;
}

std::vector<Site *> Well::Sites()
{
    return sites;
}

int Well::NumSites() const
{
    return sites.size();
}

Site *Well::NewSite(std::string id, std::string name, Pos2D rel_pos)
{
    std::unique_lock<std::shared_mutex> lk(sites_mutex);
    if (site_id_set.contains(id)) {
        throw std::invalid_argument("id already exists");
    }
    Site *site = new Site(this, id, name, rel_pos);
    sites.push_back(site);
    site_id_set.insert(id);
    return site;
}

Site::Site(const ::Well *well, std::string id, std::string name, Pos2D rel_pos)
{
    if (well == nullptr) {
        throw std::invalid_argument("well cannot be null");
    }
    if (id.empty()) {
        throw std::invalid_argument("id cannot be empty");
    }
    
    this->well = well;
    this->id = id;
    this->name = name;
    this->rel_pos = rel_pos;
}

const Well *Site::Well() const
{
    return this->well;
}

std::string Site::ID() const
{
    return id;
}
std::string Site::Name() const
{
    return name;
}

Pos2D Site::RelativePosition() const
{
    return this->rel_pos;
}

std::optional<Pos2D> Site::Position() const
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
