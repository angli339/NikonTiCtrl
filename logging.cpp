#include "logging.h"

#include <cstdio>
#include <memory>

#include <sqlite3.h>
#include "utils/time_utils.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

sqlite3 *log_db;

const char *log_db_schema = R"(
CREATE TABLE "io" (
	"timestamp"	TEXT,
	"module"	TEXT,
    "function"  TEXT,
	"device"	TEXT,
	"operation"	TEXT,
	"request"	TEXT,
	"response"	TEXT,
    "status"	TEXT
);
)";

const char *sql_insert_io_log = "INSERT INTO \"io\" VALUES(?,?,?,?,?,?,?,?);";
sqlite3_stmt *stmt_insert_io_log;

void init_logger()
{
    std::time_t t = std::time(nullptr);
    std::string log_path = fmt::format("logs/nikon_ti_ctrl-{:%Y%m%d-%H%M%S}.log", *std::localtime(&t));
    std::string log_db_path = fmt::format("logs/nikon_ti_ctrl-{:%Y%m%d-%H%M%S}.sqlite3", *std::localtime(&t));

    try {
        std::vector<spdlog::sink_ptr> sinks;

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);
        console_sink->set_pattern("%^[%5!l]%$[%Y-%m-%dT%H:%M:%S.%f%z][TID=%t] %v");
        sinks.push_back(console_sink);

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path, true);
        file_sink->set_level(spdlog::level::trace);
        file_sink->set_pattern("[%5!l][%Y-%m-%dT%H:%M:%S.%f%z][TID=%t] %v");
        sinks.push_back(file_sink);

        std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>("default", begin(sinks), end(sinks));
        spdlog::set_default_logger(logger);

        spdlog::set_level(spdlog::level::trace);
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::printf("Log init failed: %s\n", ex.what());
        exit(1);
    }

    int rc;
    rc = sqlite3_open(log_db_path.c_str(), &log_db);
    if (rc) {
        SPDLOG_ERROR("Log init failed: Can't open database '%s'", log_db_path);
        sqlite3_close(log_db);
        exit(1);
    }

    rc = sqlite3_exec(log_db, log_db_schema, NULL, NULL, NULL);
    if (rc) {
        SPDLOG_ERROR("Log init failed: Can't create table");
        sqlite3_close(log_db);
        exit(1);
    }

    rc = sqlite3_prepare_v2(log_db, sql_insert_io_log, -1, &stmt_insert_io_log, 0);
    if (rc) {
        SPDLOG_ERROR("Log init failed: Can't prepare stmt");
        sqlite3_close(log_db);
        exit(1);
    }
}

void deinit_logger()
{
    sqlite3_finalize(stmt_insert_io_log);
    sqlite3_close(log_db);
    return;
}

void log_io(std::string module, std::string function, std::string device, std::string method, std::string request, std::string response, std::string status)
{
    std::string timestamp = getTimestamp();
    std::vector<std::string> values = {timestamp, module, function, device, method, request, response, status};

    int rc;
    rc = sqlite3_reset(stmt_insert_io_log);
    if (rc != SQLITE_OK) {
        SPDLOG_ERROR("log_io: sqlite3_reset failed");
        return;
    }

    for (int i = 0; i < values.size(); i++) {
        rc = sqlite3_bind_text(stmt_insert_io_log, i+1, values[i].c_str(), -1, SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            SPDLOG_ERROR("log_io: sqlite3_bind_text failed");
            return;
        }
    }

    rc = sqlite3_step(stmt_insert_io_log);
    if (rc != SQLITE_DONE) {
        SPDLOG_ERROR("log_io: sqlite3_step failed");
        return;
    }
    return;
}