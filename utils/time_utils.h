#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <chrono>
#include <ctime> // std::tm, std::mktime
#include <fmt/chrono.h> // fmt::gmtime, fmt::localtime
#include <spdlog/stopwatch.h>
#include <string>

static inline double stopwatch_ms(spdlog::stopwatch sw)
{
    return sw.elapsed().count() * 1000;
}

namespace utils {

using stopwatch = spdlog::stopwatch;

class TimePoint {
public:
    TimePoint() {}
    TimePoint(std::chrono::system_clock::time_point time_point) { tp = time_point; }

    std::string FormatRFC3339() const;

    inline std::tm UTC() const { return fmt::gmtime(tp); }
    inline std::tm Local() const { return fmt::localtime(tp); }

    inline int Microseconds() const
    {
        using std::chrono::duration_cast;
        using std::chrono::seconds;
        using std::chrono::microseconds;

        auto duration = tp.time_since_epoch();
        auto secs = duration_cast<seconds>(duration);
        auto micros = duration_cast<microseconds>(duration) - duration_cast<microseconds>(secs);
        return micros.count();
    }

    inline int TZOffset() const
    {
        std::tm tm_utc = fmt::gmtime(tp);
        std::tm tm_local = fmt::localtime(tp);

        int tz_offset = (std::mktime(&tm_local) - std::mktime(&tm_utc)) / 60;
        if (tm_local.tm_isdst) {
            tz_offset += 60;
        }
        
        return tz_offset;
    }

private:
    std::chrono::system_clock::time_point tp;
};

inline TimePoint Now()
{
    return TimePoint(std::chrono::system_clock::now());
}

} // namespace utils

#endif
