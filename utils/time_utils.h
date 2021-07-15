#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <chrono>
#include <string>


std::string fmtRFC3339(struct timespec *ts);

inline std::string getTimestamp() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return fmtRFC3339(&ts);
}

#endif // TIME_UTILS_H
