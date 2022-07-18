#ifndef EXPERIMENTDB_H
#define EXPERIMENTDB_H

#include <string>
#include <optional>
#include <vector>
#include <filesystem>
#include <sqlite3.h>
#include <nlohmann/json.hpp>

struct PlateRow {
    std::string plate_id;
    std::string type;
    std::optional<double> pos_origin_x;
    std::optional<double> pos_origin_y;
    nlohmann::json metadata;
};

struct WellRow {
    std::string plate_id;
    std::string well_id;
    double rel_pos_x;
    double rel_pos_y;
    std::string preset_name;
    nlohmann::json metadata;
};

struct SiteRow {
    std::string plate_id;
    std::string well_id;
    std::string site_id;
    double rel_pos_x;
    double rel_pos_y;
};

struct NDImageRow {
    std::string name;
    nlohmann::json ch_names;
    int width;
    int height;
    int n_ch;
    int n_z;
    int n_t;
    std::string plate_id;
    std::string well_id;
    std::string site_id;
};

struct ImageRow {
    std::string ndimage_name;
    std::string ch_name;
    int i_z;
    int i_t;
    std::string path;
    double exposure_ms;
    std::optional<double> pos_x;
    std::optional<double> pos_y;
    std::optional<double> pos_z;
};

class ExperimentDB {
public:
    ExperimentDB(std::filesystem::path filename);
    ~ExperimentDB();

    void CreateSchema();

    std::vector<PlateRow> GetAllPlates();
    std::vector<WellRow> GetAllWells();
    std::vector<SiteRow> GetAllSites();
    std::vector<NDImageRow> GetAllNDImages();
    std::vector<ImageRow> GetAllImages();

private:
    std::filesystem::path filename;

    sqlite3 *db;
};

#endif
