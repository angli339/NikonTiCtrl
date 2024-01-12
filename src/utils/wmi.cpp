#include "wmi.h"

#include "logging.h"
#include <cstdint>
#include <fmt/format.h>
#include <stdexcept>

#define NOMINMAX
#include <windows.h>

#define _WIN32_DCOM
#include <Wbemidl.h>
#include <comdef.h>

std::string widechar_to_utf8(const wchar_t *wchar_c_str);

WMI::WMI()
{
    HRESULT hres;

    // Initialize COM.
    //   COINIT_MULTITHREADED gives 0x80010106 error,
    //   COINIT_APARTMENTTHREADED somehow works.
    hres = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (FAILED(hres)) {
        LOG_ERROR("WMI: CoInitializeEx: Error {:#10x}", (uint32_t)hres);
        throw std::runtime_error(
            fmt::format("CoInitializeEx: Error {:#10x}", (uint32_t)hres));
    }

    // Initialize
    hres = CoInitializeSecurity(NULL,
                                -1,   // COM negotiates service
                                NULL, // Authentication services
                                NULL, // Reserved
                                RPC_C_AUTHN_LEVEL_DEFAULT,   // authentication
                                RPC_C_IMP_LEVEL_IMPERSONATE, // Impersonation
                                NULL,      // Authentication info
                                EOAC_NONE, // Additional capabilities
                                NULL       // Reserved
    );


    // Ignore RPC_E_TOO_LATE error
    if (hres == RPC_E_TOO_LATE) {
        LOG_DEBUG("WMI: CoInitializeSecurity: Ignore Error {:#10x}",
                  (uint32_t)hres);
    } else if (FAILED(hres)) {
        LOG_ERROR("WMI: CoInitializeSecurity: Error {:#10x}", (uint32_t)hres);
        CoUninitialize();
        throw std::runtime_error(
            fmt::format("CoInitializeSecurity: Error {:#10x}", (uint32_t)hres));
    }

    // Obtain the initial locator to Windows Management
    // on a particular host computer.
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                            IID_IWbemLocator, (LPVOID *)&pLoc);

    if (FAILED(hres)) {
        LOG_ERROR("WMI: CoCreateInstance: Error {:#10x}", (uint32_t)hres);
        CoUninitialize();
        throw std::runtime_error(
            fmt::format("CoCreateInstance: Error {:#10x}", (uint32_t)hres));
    }

    // Connect to the root\cimv2 namespace with the
    // current user and obtain pointer pSvc
    // to make IWbemServices calls.

    BSTR bstr_namespace = SysAllocString(L"ROOT\\CIMV2");

    hres = pLoc->ConnectServer(bstr_namespace, // WMI namespace
                               NULL,           // User name
                               NULL,           // User password
                               NULL,           // Locale
                               0,              // Security flags
                               0,              // Authority
                               0,              // Context object
                               &pSvc           // IWbemServices proxy
    );

    if (FAILED(hres)) {
        LOG_ERROR("WMI: ConnectServer: Error {:#10x}", (uint32_t)hres);
        pLoc->Release();
        pLoc = nullptr;

        CoUninitialize();
        throw std::runtime_error(
            fmt::format("ConnectServer: Error {:#10x}", (uint32_t)hres));
    }


    // Set the IWbemServices proxy so that impersonation
    // of the user (client) occurs.
    hres = CoSetProxyBlanket(pSvc,                   // the proxy to set
                             RPC_C_AUTHN_WINNT,      // authentication service
                             RPC_C_AUTHZ_NONE,       // authorization service
                             NULL,                   // Server principal name
                             RPC_C_AUTHN_LEVEL_CALL, // authentication level
                             RPC_C_IMP_LEVEL_IMPERSONATE, // impersonation level
                             NULL,                        // client identity
                             EOAC_NONE                    // proxy capabilities
    );

    if (FAILED(hres)) {
        LOG_ERROR("WMI: CoSetProxyBlanket: Error {:#10x}", (uint32_t)hres);
        pSvc->Release();
        pSvc = nullptr;

        pLoc->Release();
        pLoc = nullptr;

        CoUninitialize();
        throw std::runtime_error(
            fmt::format("CoSetProxyBlanket: Error {:#10x}", (uint32_t)hres));
    }
}

WMI::~WMI()
{
    pSvc->Release();
    pSvc = nullptr;

    pLoc->Release();
    pLoc = nullptr;

    CoUninitialize();
}

std::vector<std::string> WMI::listDeviceID(std::string queryWhere)
{
    // Use the IWbemServices pointer to make requests of WMI.

    HRESULT hres;
    IEnumWbemClassObject *pEnumerator = NULL;
    BSTR bstr_wql = SysAllocString(L"WQL");

    std::wstring query =
        std::wstring(L"SELECT DeviceID FROM Win32_PnPEntity WHERE ") +
        std::wstring(queryWhere.begin(), queryWhere.end());
    BSTR bstr_sql = SysAllocString(query.c_str());

    hres =
        pSvc->ExecQuery(bstr_wql, bstr_sql,
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                        NULL, &pEnumerator);

    if (FAILED(hres)) {
        LOG_ERROR("WMI: ExecQuery: Error {:#10x}", (uint32_t)hres);
        throw std::runtime_error(
            fmt::format("ExecQuery: Error {:#10x}", (uint32_t)hres));
    }

    IWbemClassObject *pclsObj;
    ULONG uReturn = 0;
    std::vector<std::string> deviceIDList;
    while (pEnumerator) {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if (uReturn == 0) {
            break;
        }

        VARIANT vtProp;
        hres = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);

        std::string deviceID = widechar_to_utf8((wchar_t *)vtProp.bstrVal);
        deviceIDList.push_back(deviceID);

        VariantClear(&vtProp);
        pclsObj->Release();
        pclsObj = NULL;
    }

    return deviceIDList;
}

std::vector<std::string> WMI::listUSBDeviceID(std::string vid, std::string pid)
{
    if ((vid == "") && (pid == "")) {
        return listDeviceID(
            "(DeviceID LIKE 'USB\\\\VID_%') and (Service != 'usbhub')");
    } else if ((vid != "") && (pid == "")) {
        return listDeviceID("(DeviceID LIKE 'USB\\\\VID_" + vid +
                            "&PID_%') and (Service != 'usbhub')");
    } else if ((vid != "") && (pid != "")) {
        return listDeviceID("(DeviceID LIKE 'USB\\\\VID_" + vid + "&PID_" +
                            pid + "%') and (Service != 'usbhub')");
    } else {
        throw std::invalid_argument("vid == '', pid != '' is not allowed");
    }
}

std::vector<std::string> WMI::list1394DeviceID(std::string vendor)
{
    if (vendor == "") {
        return listDeviceID("DeviceID LIKE '1394\\\\%'");
    } else {
        return listDeviceID("DeviceID LIKE '1394\\\\" + vendor + "&%'");
    }
}

std::string widechar_to_utf8(const wchar_t *wchar_c_str)
{
    int utf8_str_length = ::WideCharToMultiByte(CP_UTF8,     // CodePage
                                                0,           // dwFlags
                                                wchar_c_str, // lpWideCharStr
                                                -1,          // cchWideChar
                                                NULL,        // lpMultiByteStr
                                                0,           // cbMultiByte
                                                NULL,        // lpDefaultChar
                                                NULL // lpUsedDefaultChar
    );
    if (utf8_str_length == 0) {
        return "";
    }

    std::string utf8_str;
    utf8_str.reserve(utf8_str_length);

    utf8_str_length =
        ::WideCharToMultiByte(CP_UTF8,                  // CodePage
                              0,                        // dwFlags
                              wchar_c_str,              // lpWideCharStr
                              -1,                       // cchWideChar
                              utf8_str.data(),          // lpMultiByteStr
                              (int)utf8_str.capacity(), // cbMultiByte
                              NULL,                     // lpDefaultChar
                              NULL                      // lpUsedDefaultChar
        );

    utf8_str.resize(utf8_str_length);
    return utf8_str;
}
