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