#include "sample/samplemanager.h"

#include "experimentcontrol.h"

SampleManager::SampleManager(ExperimentControl *exp) { this->exp = exp; }

SampleManager::~SampleManager()
{
    std::unique_lock<std::shared_mutex> lk(plate_mutex);
    for (::Plate *plate : plates) {
        delete plate;
    }
    plates.clear();
    plate_map.clear();
}

void SampleManager::LoadFromDB()
{
    std::unique_lock<std::shared_mutex> lk(plate_mutex);
    for (::Plate *plate : plates) {
        delete plate;
    }
    plates.clear();
    plate_map.clear();

    for (const auto &plate_row : exp->DB()->GetAllPlates()) {
        ::Plate *plate = new ::Plate;
        plate->uuid = plate_row.uuid;
        plate->type = PlateTypeFromString(plate_row.type);
        plate->id = plate_row.plate_id;
        if (plate_row.pos_origin_x.has_value() &&
            plate_row.pos_origin_y.has_value())
        {
            plate->pos_origin = {plate_row.pos_origin_x.value(),
                                 plate_row.pos_origin_y.value()};
        }

        plates.push_back(plate);
        plate_map[plate->id] = plate;
    }

    for (const auto &well_row : exp->DB()->GetAllWells()) {
        ::Plate *plate = plate_map[well_row.plate_id];
        ::Well *well = new ::Well;
        well->plate = plate;
        well->uuid = well_row.uuid;
        well->id = well_row.well_id;
        well->rel_pos = {well_row.rel_pos_x, well_row.rel_pos_y};
        well->enabled = well_row.enabled;
        well->metadata = well_row.metadata;

        plate->wells.push_back(well);
        plate->well_map[well->id] = well;
    }

    for (const auto &site_row : exp->DB()->GetAllSites()) {
        ::Plate *plate = plate_map[site_row.plate_id];
        ::Well *well = plate->well_map[site_row.well_id];
        ::Site *site = new ::Site;
        site->well = well;
        site->uuid = site_row.uuid;
        site->id = site_row.site_id;
        site->rel_pos = {site_row.rel_pos_x, site_row.rel_pos_y};
        site->enabled = site_row.enabled;
        site->metadata = site_row.metadata;

        well->sites.push_back(site);
        well->site_map[site->id] = site;
    }
}

void SampleManager::writePlateRow(const ::Plate *plate)
{
    PlateRow row = PlateRow{
        .index = plate->Index(),
        .uuid = plate->UUID(),
        .plate_id = plate->ID(),
        .type = PlateTypeToString(plate->Type()),
        .metadata = plate->Metadata(),
    };
    auto pos_origin = plate->PositionOrigin();
    if (pos_origin.has_value()) {
        row.pos_origin_x = pos_origin.value().x;
        row.pos_origin_y = pos_origin.value().y;
    }
    exp->DB()->InsertOrReplaceRow(row);
}

void SampleManager::writeWellRow(const ::Well *well)
{
    exp->DB()->InsertOrReplaceRow(WellRow{
        .index = well->Index(),
        .uuid = well->UUID(),
        .plate_id = well->Plate()->ID(),
        .well_id = well->ID(),
        .rel_pos_x = well->RelativePosition().x,
        .rel_pos_y = well->RelativePosition().y,
        .enabled = well->Enabled(),
        .metadata = well->Metadata(),
    });
}

void SampleManager::writeSiteRow(const ::Site *site)
{
    exp->DB()->InsertOrReplaceRow(SiteRow{
        .index = site->Index(),
        .uuid = site->UUID(),
        .plate_id = site->Well()->Plate()->ID(),
        .well_id = site->Well()->ID(),
        .site_id = site->ID(),
        .rel_pos_x = site->RelativePosition().x,
        .rel_pos_y = site->RelativePosition().y,
        .enabled = site->Enabled(),
        .metadata = site->Metadata(),
    });
}

void SampleManager::AddPlate(PlateType plate_type, std::string plate_id)
{
    if (!exp->is_open()) {
        throw std::invalid_argument("no open experiment");
    }

    std::unique_lock<std::shared_mutex> lk(plate_mutex);

    if (get_plate(plate_id) != nullptr) {
        throw std::invalid_argument("id already exists");
    }

    // Create plate
    ::Plate *plate = new ::Plate(plate_type, plate_id);
    plate->index = plates.size();
    plates.push_back(plate);
    plate_map[plate_id] = plate;

    // Write to DB
    exp->DB()->BeginTransaction();
    try {
        writePlateRow(plate);
        for (const auto &well : plate->wells) {
            writeWellRow(well);
        }
        exp->DB()->Commit();
    } catch (std::exception &e) {
        plates.pop_back();
        plate_map.erase(plate_id);
        exp->DB()->Rollback();
        throw std::runtime_error(
            fmt::format("cannot write to DB: {}, rolled back", e.what()));
    }

    lk.unlock();

    SendEvent({
        .type = EventType::PlateCreated,
        .value = plate_id,
    });

    if (plates.size() == 1) {
        SetCurrentPlate(plate_id);
    }
}

void SampleManager::SetPlatePositionOrigin(std::string plate_id, double x,
                                           double y)
{
    if (!exp->is_open()) {
        throw std::invalid_argument("no open experiment");
    }

    std::unique_lock<std::shared_mutex> lk(plate_mutex);

    ::Plate *plate = get_plate(plate_id);
    if (plate == nullptr) {
        throw std::invalid_argument(
            fmt::format("plate {} does not exists", plate_id));
    }

    plate->pos_origin = Pos2D{x, y};

    // Write to DB
    exp->DB()->BeginTransaction();
    try {
        writePlateRow(plate);
        exp->DB()->Commit();
    } catch (std::exception &e) {
        plate->pos_origin.reset();
        exp->DB()->Rollback();
        throw std::runtime_error(
            fmt::format("cannot write to DB: {}, rolled back", e.what()));
    }

    lk.unlock();

    SendEvent({
        .type = EventType::PlateModified,
        .value = plate_id,
    });
}

void SampleManager::SetPlateMetadata(std::string plate_id, std::string key,
                                     nlohmann::ordered_json value)
{
    if (!exp->is_open()) {
        throw std::invalid_argument("no open experiment");
    }

    std::unique_lock<std::shared_mutex> lk(plate_mutex);

    ::Plate *plate = get_plate(plate_id);
    if (plate == nullptr) {
        throw std::invalid_argument(
            fmt::format("plate {} does not exists", plate_id));
    }

    nlohmann::ordered_json old_metadata = plate->metadata;
    plate->metadata[key] = value;

    // Write to DB
    exp->DB()->BeginTransaction();
    try {
        writePlateRow(plate);
        exp->DB()->Commit();
    } catch (std::exception &e) {
        plate->metadata = old_metadata;
        exp->DB()->Rollback();
        throw std::runtime_error(
            fmt::format("cannot write to DB: {}, rolled back", e.what()));
    }

    lk.unlock();

    SendEvent({
        .type = EventType::PlateModified,
        .value = plate_id,
    });
}

void SampleManager::SetWellsEnabled(std::string plate_id,
                                    std::vector<std::string> well_ids,
                                    bool enabled)
{
    if (!exp->is_open()) {
        throw std::invalid_argument("no open experiment");
    }

    std::unique_lock<std::shared_mutex> lk(plate_mutex);

    ::Plate *plate = get_plate(plate_id);
    if (plate == nullptr) {
        throw std::invalid_argument(
            fmt::format("plate {} does not exists", plate_id));
    }

    std::vector<::Well *> wells;
    for (const auto &well_id : well_ids) {
        ::Well *well = plate->Well(well_id);
        if (well == nullptr) {
            throw std::invalid_argument(fmt::format(
                "well {} does not exists in plate {}", well_id, plate_id));
        }
        wells.push_back(well);
    }

    std::map<std::string, bool> old_enabled;
    for (const auto &well : wells) {
        old_enabled[well->id] = well->enabled;
        well->enabled = enabled;
    }

    // Write to DB
    exp->DB()->BeginTransaction();
    try {
        for (const auto &well : wells) {
            writeWellRow(well);
        }
        exp->DB()->Commit();
    } catch (std::exception &e) {
        for (const auto &well : wells) {
            well->enabled = old_enabled[well->id];
        }
        exp->DB()->Rollback();
        throw std::runtime_error(
            fmt::format("cannot write to DB: {}, rolled back", e.what()));
    }

    lk.unlock();

    SendEvent({
        .type = EventType::PlateModified,
        .value = plate_id,
    });
}

void SampleManager::SetWellsMetadata(std::string plate_id,
                                     std::vector<std::string> well_ids,
                                     std::string key,
                                     nlohmann::ordered_json value)
{
    if (!exp->is_open()) {
        throw std::invalid_argument("no open experiment");
    }

    std::unique_lock<std::shared_mutex> lk(plate_mutex);

    ::Plate *plate = get_plate(plate_id);
    if (plate == nullptr) {
        throw std::invalid_argument(
            fmt::format("plate {} does not exists", plate_id));
    }

    std::vector<::Well *> wells;
    for (const auto &well_id : well_ids) {
        ::Well *well = plate->Well(well_id);
        if (well == nullptr) {
            throw std::invalid_argument(fmt::format(
                "well {} does not exists in plate {}", well_id, plate_id));
        }
        wells.push_back(well);
    }

    std::map<std::string, nlohmann::ordered_json> old_metadata;
    for (const auto &well : wells) {
        old_metadata[well->id] = well->metadata;
        well->metadata[key] = value;
    }

    // Write to DB
    exp->DB()->BeginTransaction();
    try {
        for (const auto &well : wells) {
            writeWellRow(well);
        }
        exp->DB()->Commit();
    } catch (std::exception &e) {
        for (const auto &well : wells) {
            well->metadata = old_metadata[well->id];
        }
        exp->DB()->Rollback();
        throw std::runtime_error(
            fmt::format("cannot write to DB: {}, rolled back", e.what()));
    }

    lk.unlock();

    SendEvent({
        .type = EventType::PlateModified,
        .value = plate_id,
    });
}

void SampleManager::CreateSitesOnCenteredGrid(std::string plate_id,
                                              std::vector<std::string> well_ids,
                                              int n_x, int n_y,
                                              double spacing_x,
                                              double spacing_y)
{
    if (!exp->is_open()) {
        throw std::invalid_argument("no open experiment");
    }

    std::unique_lock<std::shared_mutex> lk(plate_mutex);

    ::Plate *plate = get_plate(plate_id);
    if (plate == nullptr) {
        throw std::invalid_argument(
            fmt::format("plate {} does not exists", plate_id));
    }

    std::vector<::Well *> wells;
    for (const auto &well_id : well_ids) {
        ::Well *well = plate->Well(well_id);
        if (well == nullptr) {
            throw std::invalid_argument(fmt::format(
                "well {} does not exists in plate {}", well_id, plate_id));
        }
        if (well->NumSites() != 0) {
            throw std::runtime_error(
                fmt::format("sites already created in well {}", well->ID()));
        }
        wells.push_back(well);
    }

    if ((n_x < 1) || (n_y < 1)) {
        throw std::invalid_argument("invalid n_x or n_y");
    }
    if ((spacing_x == 0) || (spacing_y == 0)) {
        throw std::invalid_argument("invalid spacing");
    }

    double corner_x = -((double)(n_x - 1) / 2) * spacing_x;
    double corner_y = -((double)(n_y - 1) / 2) * spacing_y;

    int n_site = n_x * n_y;
    if (n_site >= 10000) {
        throw std::invalid_argument(
            "too many sites, site_id formatter is not implemented");
    }

    for (const auto &well : wells) {
        int i_site = 0;
        for (int i_y = 0; i_y < n_y; i_y++) {
            if (i_y % 2 == 0) {
                for (int i_x = 0; i_x < n_x; i_x++) {
                    double x = corner_x + i_x * spacing_x;
                    double y = corner_y + i_y * spacing_y;
                    std::string site_id;
                    if (n_site < 1000) {
                        site_id = fmt::format("{:03d}", i_site);
                    } else if (n_site < 10000) {
                        site_id = fmt::format("{:04d}", i_site);
                    }
                    ::Site *site = new ::Site(well, site_id, Pos2D{x, y});
                    site->metadata["name"] = fmt::format("{},{}", i_x, i_y);
                    well->addSite(site);
                    i_site++;
                }
            } else {
                for (int i_x = n_x - 1; i_x >= 0; i_x--) {
                    double x = corner_x + i_x * spacing_x;
                    double y = corner_y + i_y * spacing_y;
                    std::string site_id;
                    if (n_site < 1000) {
                        site_id = fmt::format("{:03d}", i_site);
                    } else if (n_site < 10000) {
                        site_id = fmt::format("{:04d}", i_site);
                    } else if (n_site < 100000) {
                        site_id = fmt::format("{:05d}", i_site);
                    } else if (n_site < 1000000) {
                        site_id = fmt::format("{:06d}", i_site);
                    }
                    ::Site *site = new ::Site(well, site_id, Pos2D{x, y});
                    site->metadata["name"] = fmt::format("{},{}", i_x, i_y);
                    well->addSite(site);
                    i_site++;
                }
            }
        }
    }

    // Write to DB
    exp->DB()->BeginTransaction();
    try {
        for (const auto &well : wells) {
            for (const auto &site : well->sites) {
                writeSiteRow(site);
            }
        }
        exp->DB()->Commit();
    } catch (std::exception &e) {
        for (const auto &well : wells) {
            well->clearSites();
        }
        exp->DB()->Rollback();
        throw std::runtime_error(
            fmt::format("cannot write to DB: {}, rolled back", e.what()));
    }

    lk.unlock();

    SendEvent({
        .type = EventType::PlateModified,
        .value = plate_id,
    });
}

void SampleManager::SetCurrentPlate(std::string plate_id)
{
    if (!exp->is_open()) {
        throw std::invalid_argument("no open experiment");
    }

    std::unique_lock<std::shared_mutex> lk(plate_mutex);
    if (plate_id.empty()) {
        this->current_plate = nullptr;
        return;
    }

    ::Plate *plate = get_plate(plate_id);
    if (plate == nullptr) {
        throw std::invalid_argument(
            fmt::format("plate {} not found", plate_id));
    }
    this->current_plate = plate;
    lk.unlock();

    SendEvent({
        .type = EventType::CurrentPlateChanged,
        .value = plate_id,
    });
}

::Plate *SampleManager::CurrentPlate()
{
    std::shared_lock<std::shared_mutex> lk(plate_mutex);
    return this->current_plate;
}

::Plate *SampleManager::Plate(std::string plate_id)
{
    std::shared_lock<std::shared_mutex> lk(plate_mutex);
    return get_plate(plate_id);
}

::Well *SampleManager::Well(std::string plate_id, std::string well_id)
{
    std::shared_lock<std::shared_mutex> lk(plate_mutex);
    return get_well(plate_id, well_id);
}

::Site *SampleManager::Site(std::string plate_id, std::string well_id,
                            std::string site_id)
{
    std::shared_lock<std::shared_mutex> lk(plate_mutex);
    return get_site(plate_id, well_id, site_id);
}

::Plate *SampleManager::get_plate(std::string plate_id)
{
    if (!exp->is_open()) {
        throw std::invalid_argument("no open experiment");
    }

    auto it = plate_map.find(plate_id);
    if (it == plate_map.end()) {
        return nullptr;
    }
    return it->second;
}

::Well *SampleManager::get_well(std::string plate_id, std::string well_id)
{
    ::Plate *plate = Plate(plate_id);
    if (!plate) {
        return nullptr;
    }
    return plate->Well(well_id);
}

::Site *SampleManager::get_site(std::string plate_id, std::string well_id,
                                std::string site_id)
{
    ::Well *well = Well(plate_id, well_id);
    if (!well) {
        return nullptr;
    }
    return well->Site(site_id);
}

::Plate *SampleManager::PlateByUUID(std::string uuid)
{
    if (uuid.empty()) {
        return nullptr;
    }

    std::shared_lock<std::shared_mutex> lk(plate_mutex);
    for (const auto &plate : plates) {
        if (plate->uuid == uuid) {
            return plate;
        }
    }
    return nullptr;
}

::Well *SampleManager::WellByUUID(std::string uuid)
{
    if (uuid.empty()) {
        return nullptr;
    }
    // For now, just search it...
    std::shared_lock<std::shared_mutex> lk(plate_mutex);
    for (const auto &plate : plates) {
        for (const auto &well : plate->wells) {
            if (well->uuid == uuid) {
                return well;
            }
        }
    }
    return nullptr;
}

::Site *SampleManager::SiteByUUID(std::string uuid)
{
    if (uuid.empty()) {
        return nullptr;
    }
    // For now, just search it...
    std::shared_lock<std::shared_mutex> lk(plate_mutex);
    for (const auto &plate : plates) {
        for (const auto &well : plate->wells) {
            for (const auto &site : well->sites) {
                if (site->uuid == uuid) {
                    return site;
                }
            }
        }
    }
    return nullptr;
}

std::vector<Plate *> SampleManager::Plates()
{
    std::shared_lock<std::shared_mutex> lk(plate_mutex);
    return plates;
}
