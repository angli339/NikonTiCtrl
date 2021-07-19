#include "time_utils.h"

#include <fmt/chrono.h>
#include <fmt/format.h>

namespace utils {

std::string TimePoint::FormatRFC3339() const
{   
    int tz_total_min = TZOffset();
    char sign;
    if (tz_total_min < 0) {
        sign = '-';
        tz_total_min = -tz_total_min;
    } else {
        sign = '+';
    }
    int tz_hr = tz_total_min / 60;
    int tz_min = tz_total_min % 60;

    return fmt::format("{:%Y-%m-%dT%H:%M:%S}.{:06d}{}{:02d}{:02d}",
        Local(), Microseconds(), sign, tz_hr, tz_min);
}

} // namespace utils
