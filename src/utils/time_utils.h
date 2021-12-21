#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <chrono>
#include <ctime> // std::tm, std::mktime
#include <string>

#include <fmt/chrono.h> // fmt::gmtime, fmt::localtime
#include <nlohmann/json.hpp>

namespace utils {

class StopWatch {
    using clock = std::chrono::steady_clock;

public:
    StopWatch(const StopWatch &sw)
    {
        tp_start = sw.tp_start;
    }
    StopWatch()
    {
        tp_start = clock::now();
    }
    void Reset()
    {
        tp_start = clock::now();
    }
    double Milliseconds() const
    {
        return std::chrono::duration<double>(clock::now() - tp_start).count() *
               1000;
    }

private:
    clock::time_point tp_start;
};

class TimePoint {
public:
    TimePoint() {}
    TimePoint(std::string rfc3339_str) {}
    TimePoint(std::chrono::system_clock::time_point time_point)
    {
        tp = time_point;
    }

    std::string FormatRFC3339_Milli_UTC() const;
    std::string FormatRFC3339_Local() const;

    inline std::tm UTC() const
    {
        return fmt::gmtime(tp);
    }
    inline std::tm Local() const
    {
        return fmt::localtime(tp);
    }

    inline uint32_t Microseconds() const
    {
        using std::chrono::duration_cast;
        using std::chrono::microseconds;
        using std::chrono::seconds;

        auto duration = tp.time_since_epoch();
        auto secs = duration_cast<seconds>(duration);
        auto micros = duration_cast<microseconds>(duration) -
                      duration_cast<microseconds>(secs);
        return (uint32_t)(micros.count());
    }

    inline uint16_t Milliseconds() const
    {
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;
        using std::chrono::seconds;

        auto duration = tp.time_since_epoch();
        auto secs = duration_cast<seconds>(duration);
        auto millis = duration_cast<milliseconds>(duration) -
                      duration_cast<milliseconds>(secs);
        return (uint16_t)(millis.count());
    }

    inline int16_t TZOffset() const
    {
        std::tm tm_utc = fmt::gmtime(tp);
        std::tm tm_local = fmt::localtime(tp);

        int16_t tz_offset =
            (int16_t)((std::mktime(&tm_local) - std::mktime(&tm_utc)) / 60);
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


inline void to_json(nlohmann::json &j, const TimePoint &timepoint)
{
    j = timepoint.FormatRFC3339_Local();
}

inline void from_json(const nlohmann::json &j, TimePoint &timepoint)
{
    timepoint = TimePoint(j.get<std::string>());
}

} // namespace utils

#endif
