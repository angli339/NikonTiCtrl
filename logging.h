#ifndef LOGGING_H
#define LOGGING_H

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <fmt/chrono.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <ghc/filesystem.hpp>

namespace fs = ghc::filesystem;

void init_logger(fs::path log_dir);
void deinit_logger();

void log_io(std::string module, std::string function, std::string device, std::string method, std::string request, std::string response, std::string status);
static inline double stopwatch_ms(spdlog::stopwatch sw)
{
    return sw.elapsed().count() * 1000;
}

#endif // LOGGING_H
