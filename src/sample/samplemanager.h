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

    void LoadFromDB();

    ::Plate *NewPlate(PlateType type, std::string id, std::string uuid="");
    ::Plate *Plate(std::string id);
    std::vector<::Plate *> Plates();

private:
    ExperimentControl *exp;

    std::shared_mutex plate_mutex;
    std::vector<::Plate *> plates;
    std::map<std::string, ::Plate *> plate_map;
};

#endif
