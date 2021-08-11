#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>

namespace util {
    bool hasPrefix(const std::string s, const std::string prefix);
    bool hasSuffix(const std::string s, const std::string suffix);
    std::string trimPrefix(const std::string s, const std::string prefix);
    std::string trimSuffix(const std::string s, const std::string suffix);
    // std::vector<std::string> split(const std::string s, const std::string sep);
};

#endif // STRING_UTILS_H