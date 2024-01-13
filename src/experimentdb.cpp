#include "experimentdb.h"

#include <exception>
#include <fmt/format.h>

std::string sql_create_tables = R"(
CREATE TABLE "Plate" (
  "index" INTEGER NOT NULL,
  "uuid" TEXT NOT NULL,
  "plate_id" TEXT NOT NULL PRIMARY KEY,
  "type" TEXT NOT NULL,
  "pos_origin_x" REAL,
  "pos_origin_y" REAL,
  "metadata" JSON
);

CREATE TABLE "Well" (
  "index" INTEGER NOT NULL,
  "uuid" TEXT NOT NULL,
  "plate_id" TEXT NOT NULL REFERENCES "Plate" ("plate_id") ON DELETE CASCADE,
  "well_id" TEXT,
  "rel_pos_x" REAL,
  "rel_pos_y" REAL,
  "enabled" BOOLEAN,
  "metadata" JSON,
  PRIMARY KEY ("plate_id", "well_id")
);

CREATE TABLE "Site" (
  "index" INTEGER NOT NULL,
  "uuid" TEXT NOT NULL,
  "plate_id" TEXT NOT NULL,
  "well_id" TEXT,
  "site_id" TEXT NOT NULL,
  "rel_pos_x" REAL NOT NULL,
  "rel_pos_y" REAL NOT NULL,
  "enabled" BOOLEAN,
  "metadata" JSON,
  PRIMARY KEY ("plate_id", "well_id", "site_id"),
  FOREIGN KEY ("plate_id", "well_id") REFERENCES "Well" ("plate_id", "well_id") ON DELETE CASCADE
);

CREATE TABLE "NDImage" (
  "index" INTEGER NOT NULL,
  "name" TEXT NOT NULL PRIMARY KEY,
  "ch_names" JSON NOT NULL,
  "width" INTEGER NOT NULL,
  "height" INTEGER NOT NULL,
  "n_ch" INTEGER NOT NULL,
  "n_z" INTEGER NOT NULL,
  "n_t" INTEGER NOT NULL,
  "plate_id" TEXT,
  "well_id" TEXT,
  "site_id" TEXT,
  FOREIGN KEY ("plate_id", "well_id", "site_id") REFERENCES "Site" ("plate_id", "well_id", "site_id") ON DELETE SET NULL
);

CREATE INDEX "idx_ndimage__plate_id_well_id_site_id" ON "NDImage" ("plate_id", "well_id", "site_id");

CREATE TABLE "Image" (
  "ndimage_name" TEXT NOT NULL REFERENCES "NDImage" ("name") ON DELETE CASCADE,
  "ch_name" TEXT NOT NULL,
  "i_z" INTEGER NOT NULL,
  "i_t" INTEGER NOT NULL,
  "path" TEXT NOT NULL,
  "exposure_ms" REAL NOT NULL,
  "pos_x" REAL,
  "pos_y" REAL,
  "pos_z" REAL,
  PRIMARY KEY ("ndimage_name", "ch_name", "i_z", "i_t")
);
)";

std::string sql_check_schema = R"(
SELECT "Image"."ndimage_name", "Image"."ch_name", "Image"."i_z", "Image"."i_t", "Image"."path", "Image"."exposure_ms", "Image"."pos_x", "Image"."pos_y", "Image"."pos_z"
FROM "Image" "Image"
WHERE 0 = 1;

SELECT "NDImage"."name", "NDImage"."ch_names", "NDImage"."width", "NDImage"."height", "NDImage"."n_ch", "NDImage"."n_z", "NDImage"."n_t", "NDImage"."plate_id", "NDImage"."well_id", "NDImage"."site_id"
FROM "NDImage" "NDImage"
WHERE 0 = 1;

SELECT "Plate"."plate_id", "Plate"."type", "Plate"."pos_origin_x", "Plate"."pos_origin_y", "Plate"."metadata"
FROM "Plate" "Plate"
WHERE 0 = 1;

SELECT "Site"."plate_id", "Site"."well_id", "Site"."site_id", "Site"."rel_pos_x", "Site"."rel_pos_y", "Site"."enabled", "Site"."metadata"
FROM "Site" "Site"
WHERE 0 = 1;

SELECT "Well"."plate_id", "Well"."well_id", "Well"."rel_pos_x", "Well"."rel_pos_y", "Well"."enabled", "Well"."metadata"
FROM "Well" "Well"
WHERE 0 = 1;
)";

ExperimentDB::ExperimentDB(std::filesystem::path filename)
{
    bool is_new_file = !std::filesystem::exists(filename);

    int rc = sqlite3_open_v2(filename.string().c_str(), &db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error(
            fmt::format("can't open database: {}\n", sqlite3_errmsg(db)));
    }
    this->filename = filename;

    if (is_new_file) {
        createTables();
    } else {
        checkSchema();
    }
}

ExperimentDB::~ExperimentDB() { sqlite3_close(db); }

std::vector<PlateRow> ExperimentDB::GetAllPlates()
{
    std::vector<PlateRow> results;

    sqlite3_stmt *stmt = prepare(
        R"(SELECT "index", uuid, plate_id, type, metadata, pos_origin_x, pos_origin_y FROM Plate ORDER BY "index")");
    while (step(stmt)) {
        auto row = PlateRow{
            .index = sqlite3_column_int(stmt, 0),
            .uuid = (const char *)(sqlite3_column_text(stmt, 1)),
            .plate_id = (const char *)(sqlite3_column_text(stmt, 2)),
            .type = (const char *)(sqlite3_column_text(stmt, 3)),
            .metadata = nlohmann::json::parse(sqlite3_column_text(stmt, 4)),
        };
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
            row.pos_origin_x = sqlite3_column_double(stmt, 5);
        }
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            row.pos_origin_y = sqlite3_column_double(stmt, 6);
        }
        results.push_back(row);
    }
    finalize(stmt);

    return results;
}

std::vector<WellRow> ExperimentDB::GetAllWells()
{
    std::vector<WellRow> results;

    sqlite3_stmt *stmt = prepare(
        R"(SELECT "index", uuid, plate_id, well_id, rel_pos_x, rel_pos_y, enabled, metadata FROM Well ORDER BY plate_id, "index")");
    while (step(stmt)) {
        results.emplace_back(WellRow{
            .index = sqlite3_column_int(stmt, 0),
            .uuid = (const char *)(sqlite3_column_text(stmt, 1)),
            .plate_id = (const char *)(sqlite3_column_text(stmt, 2)),
            .well_id = (const char *)(sqlite3_column_text(stmt, 3)),
            .rel_pos_x = sqlite3_column_double(stmt, 4),
            .rel_pos_y = sqlite3_column_double(stmt, 5),
            .enabled = (sqlite3_column_int(stmt, 6) == 1),
            .metadata = nlohmann::json::parse(sqlite3_column_text(stmt, 7)),
        });
    }
    finalize(stmt);

    return results;
}

std::vector<SiteRow> ExperimentDB::GetAllSites()
{
    std::vector<SiteRow> results;

    sqlite3_stmt *stmt = prepare(
        R"(SELECT "index", uuid, plate_id, well_id, site_id, rel_pos_x, rel_pos_y, enabled, metadata FROM Site ORDER BY plate_id, well_id, "index")");
    while (step(stmt)) {
        results.emplace_back(SiteRow{
            .index = sqlite3_column_int(stmt, 0),
            .uuid = (const char *)(sqlite3_column_text(stmt, 1)),
            .plate_id = (const char *)(sqlite3_column_text(stmt, 2)),
            .well_id = (const char *)(sqlite3_column_text(stmt, 3)),
            .site_id = (const char *)(sqlite3_column_text(stmt, 4)),
            .rel_pos_x = sqlite3_column_double(stmt, 5),
            .rel_pos_y = sqlite3_column_double(stmt, 6),
            .enabled = (sqlite3_column_int(stmt, 7) == 1),
            .metadata = nlohmann::json::parse(sqlite3_column_text(stmt, 8)),
        });
    }
    finalize(stmt);

    return results;
}

std::vector<NDImageRow> ExperimentDB::GetAllNDImages()
{
    std::vector<NDImageRow> results;

    sqlite3_stmt *stmt = prepare(
        R"(SELECT "index", name, ch_names, width, height, n_ch, n_z, n_t, plate_id, well_id, site_id FROM NDImage ORDER BY "index")");
    while (step(stmt)) {
        results.emplace_back(NDImageRow{
            .index = sqlite3_column_int(stmt, 0),
            .name = (const char *)(sqlite3_column_text(stmt, 1)),
            .ch_names = nlohmann::json::parse(sqlite3_column_text(stmt, 2)),
            .width = sqlite3_column_int(stmt, 3),
            .height = sqlite3_column_int(stmt, 4),
            .n_ch = sqlite3_column_int(stmt, 5),
            .n_z = sqlite3_column_int(stmt, 6),
            .n_t = sqlite3_column_int(stmt, 7),
            .plate_id = (const char *)(sqlite3_column_text(stmt, 8)),
            .well_id = (const char *)(sqlite3_column_text(stmt, 9)),
            .site_id = (const char *)(sqlite3_column_text(stmt, 10)),
        });
    }
    finalize(stmt);

    return results;
}

std::vector<ImageRow> ExperimentDB::GetAllImages()
{
    std::vector<ImageRow> results;

    sqlite3_stmt *stmt =
        prepare("SELECT ndimage_name, ch_name, i_z, i_t, path, exposure_ms, "
                "pos_x, pos_y, pos_z FROM Image");
    while (step(stmt)) {
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
    finalize(stmt);

    return results;
}

void ExperimentDB::createTables()
{
    BeginTransaction();
    try {
        exec(sql_create_tables);
        checkSchema();
        Commit();
    } catch (std::exception &e) {
        Rollback();
        throw std::runtime_error(fmt::format("create tables: {}", e.what()));
    }
}

void ExperimentDB::checkSchema()
{
    try {
        exec(sql_check_schema);
    } catch (std::exception &e) {
        throw std::runtime_error(
            fmt::format("unexpected db schema: {}", e.what()));
    }
}

void ExperimentDB::InsertOrReplaceRow(PlateRow row)
{
    sqlite3_stmt *stmt = prepare(R"(
        INSERT OR REPLACE INTO "Plate" ("index", uuid, plate_id, type, pos_origin_x, pos_origin_y, metadata)
        VALUES (?,?,?,?,?,?,?)
        )");
    std::string metadata = row.metadata.dump();
    bind(stmt, row.index, row.uuid, row.plate_id, row.type, row.pos_origin_x,
         row.pos_origin_y, metadata);
    step(stmt);
    finalize(stmt);
}

void ExperimentDB::InsertOrReplaceRow(WellRow row)
{
    sqlite3_stmt *stmt = prepare(R"(
        INSERT OR REPLACE INTO "Well" ("index", uuid, plate_id, well_id, rel_pos_x, rel_pos_y, enabled, metadata)
        VALUES (?,?,?,?,?,?,?,?)
        )");
    std::string metadata = row.metadata.dump();
    bind(stmt, row.index, row.uuid, row.plate_id, row.well_id, row.rel_pos_x,
         row.rel_pos_y, row.enabled, metadata);
    step(stmt);
    finalize(stmt);
}

void ExperimentDB::InsertOrReplaceRow(SiteRow row)
{
    sqlite3_stmt *stmt = prepare(R"(
        INSERT OR REPLACE INTO "Site" ("index", uuid, plate_id, well_id, site_id, rel_pos_x, rel_pos_y, enabled, metadata)
        VALUES (?,?,?,?,?,?,?,?,?)
        )");
    std::string metadata = row.metadata.dump();
    bind(stmt, row.index, row.uuid, row.plate_id, row.well_id, row.site_id,
         row.rel_pos_x, row.rel_pos_y, row.enabled, metadata);
    step(stmt);
    finalize(stmt);
}

void ExperimentDB::InsertOrReplaceRow(NDImageRow row)
{
    sqlite3_stmt *stmt = prepare(R"(
        INSERT OR REPLACE INTO "NDImage" ("index", name, ch_names, width, height, n_ch, n_z, n_t, plate_id, well_id, site_id)
        VALUES (?,?,?,?,?,?,?,?,?,?,?)
        )");
    std::string ch_names = row.ch_names.dump();
    bind(stmt, row.index, row.name, ch_names, row.width, row.height, row.n_ch,
         row.n_z, row.n_t, row.plate_id, row.well_id, row.site_id);
    step(stmt);
    finalize(stmt);
}

void ExperimentDB::InsertOrReplaceRow(ImageRow row)
{
    sqlite3_stmt *stmt = prepare(R"(
        INSERT OR REPLACE INTO "Image" (ndimage_name, ch_name, i_z, i_t, path, exposure_ms, pos_x, pos_y, pos_z)
        VALUES (?,?,?,?,?,?,?,?,?)
        )");
    bind(stmt, row.ndimage_name, row.ch_name, row.i_z, row.i_t, row.path,
         row.exposure_ms, row.pos_x, row.pos_y, row.pos_z);
    step(stmt);
    finalize(stmt);
}

void ExperimentDB::BeginTransaction()
{
    db_mutex.lock();
    try {
        exec("BEGIN");
    } catch (...) {
        db_mutex.unlock();
        throw;
    }
}

void ExperimentDB::Commit()
{
    try {
        exec("COMMIT");
    } catch (...) {
        db_mutex.unlock();
        throw;
    }
    db_mutex.unlock();
}

void ExperimentDB::Rollback()
{
    try {
        exec("ROLLBACK");
    } catch (...) {
        db_mutex.unlock();
        throw;
    }
    db_mutex.unlock();
}

void ExperimentDB::exec(std::string sql)
{
    char *errmsg;
    int rc = sqlite3_exec(db, sql.c_str(), NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        std::string err = fmt::format("exec: {}\n", errmsg);
        sqlite3_free(errmsg);
        throw std::runtime_error(err);
    }
}

sqlite3_stmt *ExperimentDB::prepare(std::string sql)
{
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), sql.size(), &stmt, 0);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(
            fmt::format("prepare: {}\n", sqlite3_errmsg(db)));
    }
    return stmt;
}

bool ExperimentDB::step(sqlite3_stmt *stmt)
{
    int rc = sqlite3_step(stmt);
    switch (rc) {
    case SQLITE_ROW:
        return true;
    case SQLITE_DONE:
        return false;
    default:
        throw std::runtime_error(fmt::format("step: {}\n", sqlite3_errmsg(db)));
    }
}

void ExperimentDB::finalize(sqlite3_stmt *stmt)
{
    int rc = sqlite3_finalize(stmt);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(
            fmt::format("finalize: {}\n", sqlite3_errmsg(db)));
    }
}

void ExperimentDB::bind(sqlite3_stmt *stmt, int index, const std::string &value)
{
    int rc = sqlite3_bind_text(stmt, index, value.c_str(), value.size(), NULL);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(fmt::format("bind: {}\n", sqlite3_errmsg(db)));
    }
}

void ExperimentDB::bind(sqlite3_stmt *stmt, int index, const double value)
{
    int rc = sqlite3_bind_double(stmt, index, value);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(fmt::format("bind: {}\n", sqlite3_errmsg(db)));
    }
}

void ExperimentDB::bind(sqlite3_stmt *stmt, int index, const int value)
{
    int rc = sqlite3_bind_int(stmt, index, value);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(fmt::format("bind: {}\n", sqlite3_errmsg(db)));
    }
}

void ExperimentDB::bind(sqlite3_stmt *stmt, int index,
                        std::optional<double> value)
{
    int rc;
    if (value.has_value()) {
        rc = sqlite3_bind_double(stmt, index, value.value());
    } else {
        rc = sqlite3_bind_null(stmt, index);
    }
    if (rc != SQLITE_OK) {
        throw std::runtime_error(fmt::format("bind: {}\n", sqlite3_errmsg(db)));
    }
}

void ExperimentDB::bind(sqlite3_stmt *stmt, int index, const bool value)
{
    int rc = sqlite3_bind_int(stmt, index, value ? 1 : 0);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(fmt::format("bind: {}\n", sqlite3_errmsg(db)));
    }
}

template <class... Args>
void ExperimentDB::bind(sqlite3_stmt *stmt, const Args &...args)
{
    int index = 1;
    (void)std::initializer_list<int>{
        ((void)bind(stmt, index++, std::forward<decltype(args)>(args)), 0)...};
}
