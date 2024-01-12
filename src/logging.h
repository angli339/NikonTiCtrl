#ifndef LOGGER_H
#define LOGGER_H

#include <filesystem>
#include <fmt/color.h>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>

#include "utils/time_utils.h"

#define LOG_LEVEL_NAME_TRACE "trace"
#define LOG_LEVEL_NAME_DEBUG "debug"
#define LOG_LEVEL_NAME_INFO "info"
#define LOG_LEVEL_NAME_WARN "warn"
#define LOG_LEVEL_NAME_ERROR "error"
#define LOG_LEVEL_NAME_FATAL "fatal"

#define LOG_LEVEL_NAMES                                                        \
    {                                                                          \
        LOG_LEVEL_NAME_TRACE, LOG_LEVEL_NAME_DEBUG, LOG_LEVEL_NAME_INFO,       \
            LOG_LEVEL_NAME_WARN, LOG_LEVEL_NAME_ERROR, LOG_LEVEL_NAME_FATAL    \
    }

#define LOG_LEVEL_TEXT_STYLE_TRACE fmt::fg(fmt::color::gray)
#define LOG_LEVEL_TEXT_STYLE_DEBUG fmt::fg(fmt::color::gray)
#define LOG_LEVEL_TEXT_STYLE_INFO fmt::fg(fmt::terminal_color::cyan)
#define LOG_LEVEL_TEXT_STYLE_WARN fmt::fg(fmt::terminal_color::yellow)
#define LOG_LEVEL_TEXT_STYLE_ERROR fmt::fg(fmt::terminal_color::red)
#define LOG_LEVEL_TEXT_STYLE_FATAL fmt::fg(fmt::terminal_color::red)

#define LOG_LEVEL_TEXT_STYLES                                                  \
    {                                                                          \
        LOG_LEVEL_TEXT_STYLE_TRACE, LOG_LEVEL_TEXT_STYLE_DEBUG,                \
            LOG_LEVEL_TEXT_STYLE_INFO, LOG_LEVEL_TEXT_STYLE_WARN,              \
            LOG_LEVEL_TEXT_STYLE_ERROR, LOG_LEVEL_TEXT_STYLE_FATAL             \
    }

namespace slog {

namespace fs = std::filesystem;
using Fields = nlohmann::json;

namespace level {

enum LevelEnum {
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
    fatal = 5,
    n_levels
};

} // namespace level

struct Entry {
    utils::TimePoint time;
    level::LevelEnum level;
    std::string func;
    std::string message;
    Fields fields;

    std::string FormatJSON() const;
};

class Logger {
public:
    ~Logger();

    void SetFilename(fs::path filename);
    void SetFlushLevel(level::LevelEnum level) { flush_level = level; }
    void SetConsoleActiveLevel(level::LevelEnum level) { active_level = level; }

    void Log(level::LevelEnum level, const std::string func,
             const std::string message, Fields fields = nullptr);

private:
    level::LevelEnum active_level = level::debug;
    level::LevelEnum flush_level = level::info;

    std::mutex con_mutex;
    void formatToConsole(const Entry &entry);

    fs::path filename;
    std::mutex fd_mutex;
    std::ofstream file;
    void formatToFile(const Entry &entry);
    void flushFile();
};

void InitConsole();
Logger &DefaultLogger();

} // namespace slog

// https://stackoverflow.com/questions/23230003/something-between-func-and-pretty-function
std::string computeMethodName(const std::string &function,
                              const std::string &prettyFunction);

#ifdef _MSC_VER
#define __COMPACT_PRETTY_FUNCTION__ computeMethodName(__FUNCTION__, __FUNCSIG__)
#else
#define __COMPACT_PRETTY_FUNCTION__                                            \
    computeMethodName(__FUNCTION__, __PRETTY_FUNCTION__)
#endif

#define LOGGER_CALL(level, message, ...)                                       \
    slog::DefaultLogger().Log(level, __COMPACT_PRETTY_FUNCTION__, message,     \
                              ##__VA_ARGS__)

#define LOG_TRACE(message, ...)                                                \
    LOGGER_CALL(slog::level::trace, fmt::format((message), ##__VA_ARGS__))
#define LOG_DEBUG(message, ...)                                                \
    LOGGER_CALL(slog::level::debug, fmt::format((message), ##__VA_ARGS__))
#define LOG_INFO(message, ...)                                                 \
    LOGGER_CALL(slog::level::info, fmt::format((message), ##__VA_ARGS__))
#define LOG_WARN(message, ...)                                                 \
    LOGGER_CALL(slog::level::warn, fmt::format((message), ##__VA_ARGS__))
#define LOG_ERROR(message, ...)                                                \
    LOGGER_CALL(slog::level::error, fmt::format((message), ##__VA_ARGS__))
#define LOG_FATAL(message, ...)                                                \
    LOGGER_CALL(slog::level::fatal, fmt::format((message), ##__VA_ARGS__))

#define LOGFIELDS_TRACE(fields, message, ...)                                  \
    LOGGER_CALL(slog::level::trace, fmt::format((message), ##__VA_ARGS__),     \
                fields)
#define LOGFIELDS_DEBUG(fields, message, ...)                                  \
    LOGGER_CALL(slog::level::debug, fmt::format((message), ##__VA_ARGS__),     \
                fields)
#define LOGFIELDS_INFO(fields, message, ...)                                   \
    LOGGER_CALL(slog::level::info, fmt::format((message), ##__VA_ARGS__),      \
                fields)
#define LOGFIELDS_WARN(fields, message, ...)                                   \
    LOGGER_CALL(slog::level::warn, fmt::format((message), ##__VA_ARGS__),      \
                fields)
#define LOGFIELDS_ERROR(fields, message, ...)                                  \
    LOGGER_CALL(slog::level::error, fmt::format((message), ##__VA_ARGS__),     \
                fields)
#define LOGFIELDS_FATAL(fields, message, ...)                                  \
    LOGGER_CALL(slog::level::fatal, fmt::format((message), ##__VA_ARGS__),     \
                fields)

#endif
