#include "time_utils.h"

#include <cstdio>
#include <cmath>

// Format timespec as RFC3339 string based on current timezone.
std::string fmtRFC3339(struct timespec *ts)
{
    struct tm tm_utc;
    struct tm tm_local;

    gmtime_s(&tm_utc, &ts->tv_sec);
    localtime_s(&tm_local, &ts->tv_sec);

    auto tz_offset = std::chrono::seconds(mktime(&tm_local) - mktime(&tm_utc) + (tm_local.tm_isdst ? 3600 : 0));
    int tz_hr = std::chrono::duration_cast<std::chrono::hours>(tz_offset).count();
    int tz_min = std::abs(std::chrono::duration_cast<std::chrono::minutes>(tz_offset).count() % 60);

    long t_us = ts->tv_nsec / 1000;

    char buf[32+1];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%06ld%+03d:%02d",
                  tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday,
                  tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec,
                  t_us, tz_hr, tz_min);
    return std::string(buf);
}
