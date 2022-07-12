#include "sample/samplemanager.h"

#include "experimentcontrol.h"

SampleManager::SampleManager(ExperimentControl *exp)
{
    this->exp = exp;
}

SampleManager::~SampleManager()
{
    std::unique_lock<std::shared_mutex> lk(items_mutex);
    for (auto &item : items) {
        if (std::holds_alternative<Sample *>(item)) {
            Sample *sample = std::get<Sample *>(item);
            delete sample;
        } else if (std::holds_alternative<SampleArray *>(item)) {
            SampleArray *array = std::get<SampleArray *>(item);
            delete array;
        }
    }
}

SampleArray *SampleManager::NewSampleArray(std::string id, std::string name,
                                           SampleArrayLayout layout)
{
    std::unique_lock<std::shared_mutex> lk(items_mutex);
    if (id_set.contains(id)) {
        throw std::invalid_argument("id already exists");
    }
    SampleArray *array = new SampleArray(id, name, layout);
    items.push_back(array);
    id_set.insert(id);
    id_array_map[id] = array;
    return array;
}

Sample *SampleManager::NewSample(std::string id, std::string name)
{
    std::unique_lock<std::shared_mutex> lk(items_mutex);
    if (id_set.contains(id)) {
        throw std::invalid_argument("id already exists");
    }
    Sample *sample = new Sample(id, name);
    items.push_back(sample);
    id_set.insert(id);
    id_sample_map[id] = sample;
    return sample;
}

SampleArray *SampleManager::GetSampleArray(std::string id)
{
    std::shared_lock<std::shared_mutex> lk(items_mutex);
    auto it = id_array_map.find(id);
    if (it == id_array_map.end()) {
        return nullptr;
    }
    return it->second;
}

Sample *SampleManager::GetSample(std::string id)
{
    std::shared_lock<std::shared_mutex> lk(items_mutex);
    auto it = id_sample_map.find(id);
    if (it == id_sample_map.end()) {
        return nullptr;
    }
    return it->second;
}

Sample *SampleManager::GetSample(std::string array_id, std::string pos_id)
{
    std::shared_lock<std::shared_mutex> lk(items_mutex);
    auto it = id_array_map.find(array_id);
    if (it == id_array_map.end()) {
        return nullptr;
    }
    SampleArray *array = it->second;
    return array->GetSample(pos_id);
}
