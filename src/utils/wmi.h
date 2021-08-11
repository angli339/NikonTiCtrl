#ifndef WMI_H
#define WMI_H

#include <vector>
#include <string>

struct IWbemLocator;
struct IWbemServices;

class WMI
{
public:
    WMI();
    ~WMI();

    std::vector<std::string> listUSBDeviceID(std::string vid = "", std::string pid = "");
    std::vector<std::string> list1394DeviceID(std::string vendor = "");

private:
    IWbemLocator *pLoc = nullptr;
    IWbemServices *pSvc = nullptr;

    std::vector<std::string> listDeviceID(std::string query);
};

#endif // WMI_H
