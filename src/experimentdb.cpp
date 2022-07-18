#include "experimentdb.h"

#include <exception>
#include <fmt/format.h>

ExperimentDB::ExperimentDB(std::filesystem::path filename) {
    bool is_newfile = !std::filesystem::exists(filename);

    int rc = sqlite3_open_v2(filename.string().c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK){
        sqlite3_close(db);
        throw std::runtime_error(fmt::format("can't open database: {}\n", sqlite3_errmsg(db)));
    }
    this->filename = filename;

    if (is_newfile) {
        CreateSchema();
    }
}

ExperimentDB::~ExperimentDB(){
    sqlite3_close(db);
}

void ExperimentDB::CreateSchema(){
    
}

std::vector<PlateRow> ExperimentDB::GetAllPlates()
{
    std::string sql = "SELECT plate_id, type, pos_origin_x, pos_origin_y, metadata FROM Plate;";
    sqlite3_stmt *stmt;

    int rc;
    rc = sqlite3_prepare_v2(db, sql.c_str(), sql.size()+1, &stmt, 0);
    if (rc != SQLITE_OK){
        throw std::runtime_error(fmt::format("can't prepare statement: {}\n", sqlite3_errmsg(db)));
    }

    std::vector<PlateRow> results;
    for (;;) {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            break;
        }
        auto row = PlateRow{
            .plate_id = (const char *)(sqlite3_column_text(stmt, 0)),
            .type = (const char *)(sqlite3_column_text(stmt, 1)),
            .metadata = nlohmann::json::parse(sqlite3_column_text(stmt, 4)),
        };
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
            row.pos_origin_x = sqlite3_column_double(stmt, 2);
        }
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            row.pos_origin_y = sqlite3_column_double(stmt, 3);
        }
        results.push_back(row);
    }

    switch (rc) {
    case SQLITE_DONE:
        break;
    case SQLITE_BUSY:
    case SQLITE_ERROR:
        throw std::runtime_error(fmt::format("step: {}\n", sqlite3_errmsg(db)));
    default:
        throw std::runtime_error(fmt::format("step gets unexpected error code: {}\n", sqlite3_errmsg(db)));
    }

    rc = sqlite3_finalize(stmt);
    if (rc != SQLITE_OK){
        throw std::runtime_error(fmt::format("can't finalize statement: {}\n", sqlite3_errmsg(db)));
    }

    return results;
}

std::vector<WellRow> ExperimentDB::GetAllWells()
{
    std::string sql = "SELECT plate_id, well_id, rel_pos_x, rel_pos_y, preset_name, metadata FROM Well;";
    sqlite3_stmt *stmt;

    int rc;
    rc = sqlite3_prepare_v2(db, sql.c_str(), sql.size()+1, &stmt, 0);
    if (rc != SQLITE_OK){
        throw std::runtime_error(fmt::format("can't prepare statement: {}\n", sqlite3_errmsg(db)));
    }

    std::vector<WellRow> results;
    for (;;) {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            break;
        }
        results.emplace_back(WellRow{
            .plate_id = (const char *)(sqlite3_column_text(stmt, 0)),
            .well_id = (const char *)(sqlite3_column_text(stmt, 1)),
            .rel_pos_x = sqlite3_column_double(stmt, 2),
            .rel_pos_y = sqlite3_column_double(stmt, 3),
            .preset_name = (const char *)(sqlite3_column_text(stmt, 4)),
            .metadata = nlohmann::json::parse(sqlite3_column_text(stmt, 5)),
        });
    }

    switch (rc) {
    case SQLITE_DONE:
        break;
    case SQLITE_BUSY:
    case SQLITE_ERROR:
        throw std::runtime_error(fmt::format("step: {}\n", sqlite3_errmsg(db)));
    default:
        throw std::runtime_error(fmt::format("step gets unexpected error code: {}\n", sqlite3_errmsg(db)));
    }

    rc = sqlite3_finalize(stmt);
    if (rc != SQLITE_OK){
        throw std::runtime_error(fmt::format("can't finalize statement: {}\n", sqlite3_errmsg(db)));
    }

    return results;
}

std::vector<SiteRow> ExperimentDB::GetAllSites()
{
    std::string sql = "SELECT plate_id, well_id, site_id, rel_pos_x, rel_pos_y FROM Site;";
    sqlite3_stmt *stmt;

    int rc;
    rc = sqlite3_prepare_v2(db, sql.c_str(), sql.size()+1, &stmt, 0);
    if (rc != SQLITE_OK){
        throw std::runtime_error(fmt::format("can't prepare statement: {}\n", sqlite3_errmsg(db)));
    }

    std::vector<SiteRow> results;
    for (;;) {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            break;
        }
        results.emplace_back(SiteRow{
            .plate_id = (const char *)(sqlite3_column_text(stmt, 0)),
            .well_id = (const char *)(sqlite3_column_text(stmt, 1)),
            .site_id = (const char *)(sqlite3_column_text(stmt, 2)),
            .rel_pos_x = sqlite3_column_double(stmt, 3),
            .rel_pos_y = sqlite3_column_double(stmt, 4),
        });
    }

    switch (rc) {
    case SQLITE_DONE:
        break;
    case SQLITE_BUSY:
    case SQLITE_ERROR:
        throw std::runtime_error(fmt::format("step: {}\n", sqlite3_errmsg(db)));
    default:
        throw std::runtime_error(fmt::format("step gets unexpected error code: {}\n", sqlite3_errmsg(db)));
    }

    rc = sqlite3_finalize(stmt);
    if (rc != SQLITE_OK){
        throw std::runtime_error(fmt::format("can't finalize statement: {}\n", sqlite3_errmsg(db)));
    }

    return results;
}

std::vector<NDImageRow> ExperimentDB::GetAllNDImages()
{
    std::string sql = "SELECT name, ch_names, width, height, n_ch, n_z, n_t, plate_id, well_id, site_id FROM NDImage;";
    sqlite3_stmt *stmt;

    int rc;
    rc = sqlite3_prepare_v2(db, sql.c_str(), sql.size()+1, &stmt, 0);
    if (rc != SQLITE_OK){
        throw std::runtime_error(fmt::format("can't prepare statement: {}\n", sqlite3_errmsg(db)));
    }

    std::vector<NDImageRow> results;
    for (;;) {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            break;
        }
        results.emplace_back(NDImageRow{
            .name = (const char *)(sqlite3_column_text(stmt, 0)),
            .ch_names = nlohmann::json::parse(sqlite3_column_text(stmt, 1)),
            .width = sqlite3_column_int(stmt, 2),
            .height = sqlite3_column_int(stmt, 3),
            .n_ch = sqlite3_column_int(stmt, 4),
            .n_z = sqlite3_column_int(stmt, 5),
            .n_t = sqlite3_column_int(stmt, 6),
            .plate_id = (const char *)(sqlite3_column_text(stmt, 7)),
            .well_id = (const char *)(sqlite3_column_text(stmt, 8)),
            .site_id = (const char *)(sqlite3_column_text(stmt, 9)),
        });
    }

    switch (rc) {
    case SQLITE_DONE:
        break;
    case SQLITE_BUSY:
    case SQLITE_ERROR:
        throw std::runtime_error(fmt::format("step: {}\n", sqlite3_errmsg(db)));
    default:
        throw std::runtime_error(fmt::format("step gets unexpected error code: {}\n", sqlite3_errmsg(db)));
    }

    rc = sqlite3_finalize(stmt);
    if (rc != SQLITE_OK){
        throw std::runtime_error(fmt::format("can't finalize statement: {}\n", sqlite3_errmsg(db)));
    }

    return results;
}

std::vector<ImageRow> ExperimentDB::GetAllImages()
{
    std::string sql = "SELECT ndimage_name, ch_name, i_z, i_t, path, exposure_ms, pos_x, pos_y, pos_z FROM Image;";
    sqlite3_stmt *stmt;

    int rc;
    rc = sqlite3_prepare_v2(db, sql.c_str(), sql.size()+1, &stmt, 0);
    if (rc != SQLITE_OK){
        throw std::runtime_error(fmt::format("can't prepare statement: {}\n", sqlite3_errmsg(db)));
    }

    std::vector<ImageRow> results;
    for (;;) {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            break;
        }
        auto row = ImageRow{
            .ndimage_name = (const char *)(sqlite3_column_text(stmt, 0)),
            .ch_name = (const char *)(sqlite3_column_text(stmt, 1)),
            .i_z = sqlite3_column_int(stmt, 2),
            .i_t = sqlite3_column_int(stmt, 3),
            .path = (const char *)(sqlite3_column_text(stmt, 4)),
            .exposure_ms = sqlite3_column_double(stmt, 5),
        };
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            row.pos_x = sqlite3_column_double(stmt, 6);
        }
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
            row.pos_y = sqlite3_column_double(stmt, 7);
        }
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
            row.pos_z = sqlite3_column_double(stmt, 8);
        }
        results.push_back(row);
    }

    switch (rc) {
    case SQLITE_DONE:
        break;
    case SQLITE_BUSY:
    case SQLITE_ERROR:
        throw std::runtime_error(fmt::format("step: {}\n", sqlite3_errmsg(db)));
    default:
        throw std::runtime_error(fmt::format("step gets unexpected error code: {}\n", sqlite3_errmsg(db)));
    }

    rc = sqlite3_finalize(stmt);
    if (rc != SQLITE_OK){
        throw std::runtime_error(fmt::format("can't finalize statement: {}\n", sqlite3_errmsg(db)));
    }

    return results;
}
