#include "logging.h"

#include <windows.h> // SetConsoleMode
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

namespace slog {

namespace level {

static std::string level_strings[] LOG_LEVEL_NAMES;

static fmt::text_style level_text_styles[] LOG_LEVEL_TEXT_STYLES;

inline const std::string &to_string(LevelEnum l) { return level_strings[l]; }

inline const fmt::text_style &to_text_style(LevelEnum l)
{
    return level_text_styles[l];
}

} // namespace level

std::string Entry::FormatJSON() const
{
    nlohmann::ordered_json j;
    j["@timestamp"] =
        time.FormatRFC3339_Milli_UTC(); // Elasticsearch only supports timestamp
                                        // with millisecond resolution
    j["time"] = time.FormatRFC3339_Local();
    j["level"] = level::to_string(level);
    j["func"] = func;
    j["message"] = message;

    for (auto &[key, value] : fields.items()) {
        j[key] = value;
    }

    return j.dump();
}

Logger::~Logger()
{
    std::lock_guard<std::mutex> lk(fd_mutex);
    if (file.is_open()) {
        file.close();
    }
}

void Logger::SetFilename(fs::path filename)
{
    auto parent_path = filename.parent_path();
    bool exists = fs::exists(parent_path);
    if (!exists) {
        fs::create_directories(parent_path);
    }

    std::lock_guard<std::mutex> lk(fd_mutex);
    if (file.is_open()) {
        file.close();
    }
    file.open(filename.string(), std::ios::app);
    this->filename = filename;
}

void Logger::Log(level::LevelEnum level, const std::string func,
                 const std::string message, Fields fields)
{
    Entry entry;
    entry.time = utils::Now();
    entry.level = level;
    entry.func = func;
    entry.message = message;
    entry.fields = fields;

    if (file.is_open()) {
        formatToFile(entry);
        if (level >= flush_level) {
            flushFile();
        }
    }

    if (level >= active_level) {
        formatToConsole(entry);
    }
}

void Logger::formatToConsole(const Entry &entry)
{
    std::lock_guard<std::mutex> lk(con_mutex);

    fmt::print(level::to_text_style(entry.level), "[{:>5}]",
               level::to_string(entry.level));
    fmt::print("[{}] {:<80}", entry.time.FormatRFC3339_Local(), entry.message);

    fmt::print(level::to_text_style(entry.level), "    func");
    fmt::print("={}", entry.func);

    for (auto &[key, value] : entry.fields.items()) {
        fmt::print(level::to_text_style(entry.level), " {}", key);
        fmt::print("={}", value.dump());
    }
    fmt::print("\n");
}

void Logger::formatToFile(const Entry &entry)
{
    std::string buf = entry.FormatJSON();

    std::lock_guard<std::mutex> lk(fd_mutex);
    if (file.is_open()) {
        file.write(buf.c_str(), buf.size());
        file.put('\n');
    }
}

void Logger::flushFile()
{
    std::lock_guard<std::mutex> lk(fd_mutex);
    if (file.is_open()) {
        file.flush();
    }
}

void InitConsole()
{
    DWORD dwMode = 0;
    GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), dwMode);
}

Logger default_logger;

Logger &DefaultLogger() { return default_logger; }

} // namespace slog

// https://stackoverflow.com/questions/23230003/something-between-func-and-pretty-function
std::string computeMethodName(const std::string &function,
                              const std::string &prettyFunction)
{
    size_t locFunName = prettyFunction.find(
        function); // If the input is a constructor, it gets the beginning of
                   // the class name, not of the method. That's why later on we
                   // have to search for the first parenthesys
    size_t begin = prettyFunction.rfind(" ", locFunName) + 1;
    size_t end = prettyFunction.find(
        "(",
        locFunName +
            function.length()); // Adding function.length() make this faster and
                                // also allows to handle operator parenthesys!
    return prettyFunction.substr(begin, end - begin);
}
