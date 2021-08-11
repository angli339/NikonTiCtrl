#ifndef NIKON_TI_PROPERTY_H
#define NIKON_TI_PROPERTY_H

#include <cstdint>
#include <string>
#include <functional>

struct MMValueConvertor {
    std::function<std::string(std::string)> valueFromMMValue;
    std::function<std::string(std::string)> valueToMMValue;
};

struct PropInfoNikonTi {
    std::string description;
    std::string mmLabel;
    std::string mmProperty;
    bool readonly;
    MMValueConvertor *mmValueConverter;
};

extern std::unordered_map<std::string, PropInfoNikonTi> propInfoNikonTi;

#endif // NIKON_TI_PROPERTY_H
