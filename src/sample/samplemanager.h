#ifndef SAMPLEMANAGER_H
#define SAMPLEMANAGER_H

#include <map>
#include <set>
#include <shared_mutex>
#include <string>
#include <variant>
#include <vector>

#include "device/devicehub.h"
#include "eventstream.h"
#include "sample/sample.h"

class ExperimentControl;

class SampleManager : public EventSender {
public:
    SampleManager(ExperimentControl *exp);
    ~SampleManager();

    void LoadFromDB();

    //
    // Set up samples
    //
    void AddPlate(PlateType plate_type, std::string plate_id);
    void SetPlatePositionOrigin(std::string plate_id, double x, double y);
    void SetPlateMetadata(std::string plate_id, std::string key,
                          nlohmann::ordered_json value);

    void SetWellsEnabled(std::string plate_id,
                         std::vector<std::string> well_ids,
                         bool enabled = true);
    void SetWellsMetadata(std::string plate_id,
                          std::vector<std::string> well_ids, std::string key,
                          nlohmann::ordered_json value);

    void CreateSitesOnCenteredGrid(std::string plate_id,
                                   std::vector<std::string> well_ids, int n_x,
                                   int n_y, double spacing_x, double spacing_y);

    //
    // Switch sample
    //
    void SetCurrentPlate(std::string plate_id);
    ::Plate *CurrentPlate();

    //
    // Get samples
    //
    std::vector<::Plate *> Plates();
    ::Plate *Plate(std::string plate_id);
    ::Well *Well(std::string plate_id, std::string well_id);
    ::Site *Site(std::string plate_id, std::string well_id,
                 std::string site_id);

    //
    // Get sample by uuid (for remote API)
    //
    ::Plate *PlateByUUID(std::string uuid);
    ::Well *WellByUUID(std::string uuid);
    ::Site *SiteByUUID(std::string uuid);

private:
    ExperimentControl *exp;

    // Get samples without locking
    ::Plate *get_plate(std::string plate_id);
    ::Well *get_well(std::string plate_id, std::string well_id);
    ::Site *get_site(std::string plate_id, std::string well_id,
                     std::string site_id);

    std::shared_mutex plate_mutex;
    std::vector<::Plate *> plates;
    std::map<std::string, ::Plate *> plate_map;

    ::Plate *current_plate = nullptr;

    void writePlateRow(const ::Plate *plate);
    void writeWellRow(const ::Well *well);
    void writeSiteRow(const ::Site *well);
};

#endif
