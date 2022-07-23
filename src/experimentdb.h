#ifndef EXPERIMENTDB_H
#define EXPERIMENTDB_H

#include <string>
#include <optional>
#include <vector>
#include <filesystem>
#include <mutex>
#include <sqlite3.h>
#include <nlohmann/json.hpp>

struct PlateRow {
    int index;
    std::string uuid;
    std::string plate_id;
    std::string type;
    std::optional<double> pos_origin_x;
    std::optional<double> pos_origin_y;
    nlohmann::ordered_json metadata;
};

struct WellRow {
    int index;
    std::string uuid;
    std::string plate_id;
    std::string well_id;
    double rel_pos_x;
    double rel_pos_y;
    bool enabled;
    nlohmann::ordered_json metadata;
};

struct SiteRow {
    int index;
    std::string uuid;
    std::string plate_id;
    std::string well_id;
    std::string site_id;
    double rel_pos_x;
    double rel_pos_y;
    bool enabled;
    nlohmann::ordered_json metadata;
};

struct NDImageRow {
    int index;
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

    std::vector<PlateRow> GetAllPlates();
    std::vector<WellRow> GetAllWells();
    std::vector<SiteRow> GetAllSites();
    std::vector<NDImageRow> GetAllNDImages();
    std::vector<ImageRow> GetAllImages();

    void InsertOrReplaceRow(PlateRow row);
    void InsertOrReplaceRow(WellRow row);
    void InsertOrReplaceRow(SiteRow row);
    void InsertOrReplaceRow(NDImageRow row);
    void InsertOrReplaceRow(ImageRow row);

    void BeginTransaction();
    void Commit();
    void Rollback();

private:
    std::filesystem::path filename;
    
    std::mutex db_mutex;
    sqlite3 *db;

    void createTables();
    void checkSchema();

    void exec(std::string sql);
    sqlite3_stmt *prepare(std::string sql);
    bool step(sqlite3_stmt *stmt);
    void finalize(sqlite3_stmt *stmt);

    void bind(sqlite3_stmt *stmt, int index, const std::string& value);
    void bind(sqlite3_stmt *stmt, int index, const double value);
    void bind(sqlite3_stmt *stmt, int index, const int value);
    void bind(sqlite3_stmt *stmt, int index, const bool value);
    void bind(sqlite3_stmt *stmt, int index, std::optional<double> value);
    
    template<class ...Args>
    void bind(sqlite3_stmt *stmt, const Args& ... args);
};

#endif
