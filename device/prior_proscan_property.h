#ifndef PRIOR_PROSCAN_PROPERTY_H
#define PRIOR_PROSCAN_PROPERTY_H

#include <string>
#include <unordered_map>

struct PropInfoProScan {
    std::string name;
    std::string description;

    std::string getCommand;
    std::string setCommand;
    std::string setResponse;

    bool isVolatile;
};

extern std::unordered_map<std::string, std::string> errorCodeProScan;
extern std::unordered_map<std::string, PropInfoProScan> propInfoProScan;

#endif // PRIOR_PROSCAN_PROPERTY_H
