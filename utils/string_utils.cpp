#include "string_utils.h"

namespace util {

bool hasPrefix(const std::string s, const std::string prefix)
{
    return s.size() >= prefix.size() && 0 == s.compare(0, prefix.size(), prefix);
}

std::string trimPrefix(const std::string s, const std::string prefix)
{
    if (hasPrefix(s, prefix)) {
        return s.substr(prefix.size());
    }
    return s;
}

bool hasSuffix(const std::string s, const std::string suffix)
{
    return s.size() >= suffix.size() && 0 == s.compare(s.size()-suffix.size(), suffix.size(), suffix);
}

std::string trimSuffix(const std::string s, const std::string suffix)
{
    if (hasSuffix(s, suffix)) {
        return s.substr(0, s.size()-suffix.size());
    }
    return s;
}

}