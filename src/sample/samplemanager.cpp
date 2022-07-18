#include "sample/samplemanager.h"

#include "experimentcontrol.h"

SampleManager::SampleManager(ExperimentControl *exp)
{
    this->exp = exp;
}

SampleManager::~SampleManager()
{
    std::unique_lock<std::shared_mutex> lk(plate_mutex);
    for (::Plate *plate : plates) {
        delete plate;
    }
    plate_map.clear();
}

void SampleManager::LoadFromDB()
{
    std::unique_lock<std::shared_mutex> lk(plate_mutex);
    for (::Plate *plate : plates) {
        delete plate;
    }
    plate_map.clear();

    for (const auto& plate_row : exp->DB()->GetAllPlates()) {
        ::Plate *plate = new ::Plate;
        if (plate_row.type == "Slide") {
            plate->type = PlateType::Slide;
        } else if (plate_row.type == "Wellplate96") {
            plate->type = PlateType::Wellplate96;
        } else if (plate_row.type == "Wellplate384") {
            plate->type = PlateType::Wellplate384;
        } else {
            plate->type = PlateType::Unknown;
        }
        
        plate->id = plate_row.plate_id;
        if (plate_row.pos_origin_x.has_value() && plate_row.pos_origin_y.has_value()) {
            plate->pos_origin = {plate_row.pos_origin_x.value(), plate_row.pos_origin_y.value()};
        }

        plates.push_back(plate);
        plate_map[plate->id] = plate;
    }

    for (const auto& well_row : exp->DB()->GetAllWells()) {
        ::Plate *plate = plate_map[well_row.plate_id];
        ::Well *well = new ::Well;
        well->plate = plate;
        well->id = well_row.well_id;
        well->rel_pos = {well_row.rel_pos_x, well_row.rel_pos_y};
        well->preset_name = well_row.preset_name;
        well->enabled = true;

        plate->wells.push_back(well);
        plate->well_map[well->id] = well;
    }

    for (const auto& site_row : exp->DB()->GetAllSites()) {
        ::Plate *plate = plate_map[site_row.plate_id];
        ::Well *well = plate->well_map[site_row.well_id];
        ::Site *site = new ::Site;
        site->well = well;
        site->id = site_row.site_id;
        site->rel_pos = {site_row.rel_pos_x, site_row.rel_pos_y};
    }
}

Plate *SampleManager::NewPlate(PlateType type, std::string id, std::string uuid)
{
    std::unique_lock<std::shared_mutex> lk(plate_mutex);
    auto it = plate_map.find(id);
    if (it != plate_map.end()) {
        throw std::invalid_argument("id already exists");
    }
    
    ::Plate *plate = new ::Plate(type, id, uuid);
    plates.push_back(plate);
    plate_map[id] = plate;
    return plate;
}

::Plate *SampleManager::Plate(std::string id)
{
    std::shared_lock<std::shared_mutex> lk(plate_mutex);
    auto it = plate_map.find(id);
    if (it == plate_map.end()) {
        return nullptr;
    }
    return it->second;
}

std::vector<Plate *> SampleManager::Plates()
{
    return plates;
}