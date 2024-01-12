#include "utils/uuid.h"

#include <array>

#include <objbase.h>

#include <fmt/format.h>

namespace utils {

std::string uuid()
{
    GUID newId;
    ::CoCreateGuid(&newId);

    return fmt::format("{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:"
                       "02x}{:02x}{:02x}",
                       newId.Data1, newId.Data2, newId.Data3, newId.Data4[0],
                       newId.Data4[1], newId.Data4[2], newId.Data4[3],
                       newId.Data4[4], newId.Data4[5], newId.Data4[6],
                       newId.Data4[7]);
}

} // namespace utils
