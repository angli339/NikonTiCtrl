#ifndef SAMPLEMANAGER_H
#define SAMPLEMANAGER_H

#include <map>
#include <set>
#include <shared_mutex>
#include <string>
#include <variant>
#include <vector>

#include "eventstream.h"
#include "device/devicehub.h"
#include "sample/sample.h"

class ExperimentControl;

class SampleManager: public EventSender {
public:
    SampleManager(ExperimentControl *exp);
    ~SampleManager();

    SampleArray *NewSampleArray(std::string id, std::string name,
                                SampleArrayLayout layout);
    Sample *NewSample(std::string id, std::string name);

    SampleArray *GetSampleArray(std::string id);
    Sample *GetSample(std::string id);
    Sample *GetSample(std::string array_id, std::string pos_id);
    std::vector<std::variant<Sample *, SampleArray *>> GetItems()
    {
        return items;
    }


private:
    ExperimentControl *exp;

    std::shared_mutex items_mutex;
    // data container
    std::vector<std::variant<Sample *, SampleArray *>> items;
    // index
    std::set<std::string> id_set;
    std::map<std::string, SampleArray *> id_array_map;
    std::map<std::string, Sample *> id_sample_map;
};

#endif
