#include "mm_api.h"

#include <stdexcept>

#define NOMINMAX
#include <windows.h>

MMCoreC *mmcore = nullptr;

void load_MMCoreC()
{
    if (mmcore != nullptr) {
        return;
    }

    HMODULE hModule = LoadLibrary("MMCoreC.dll");
    if (hModule == NULL) {
        throw std::runtime_error("cannot load MMCoreC.dll");
    }

    mmcore = new MMCoreC;

    try {
        mmcore->MM_Open = (void (*)(MM_Session *))GetProcAddress(hModule, "MM_Open");
        if (mmcore->MM_Open == nullptr) {
            throw std::runtime_error("missing MM_Open in MMCoreC.dll");
        }

        mmcore->MM_Close = (void (*)(MM_Session))GetProcAddress(hModule, "MM_Close");
        if (mmcore->MM_Close == nullptr) {
            throw std::runtime_error("missing MM_Close in MMCoreC.dll");
        }

        mmcore->MM_GetVersionInfo = (void (*)(MM_Session, char **))GetProcAddress(hModule, "MM_GetVersionInfo");
        if (mmcore->MM_GetVersionInfo == nullptr) {
            throw std::runtime_error("missing MM_GetVersionInfo in MMCoreC.dll");
        }

        mmcore->MM_GetAPIVersionInfo = (void (*)(MM_Session, char **))GetProcAddress(hModule, "MM_GetAPIVersionInfo");
        if (mmcore->MM_GetAPIVersionInfo == nullptr) {
            throw std::runtime_error("missing MM_GetAPIVersionInfo in MMCoreC.dll");
        }

        mmcore->MM_StringFree = (void (*)(char *))GetProcAddress(hModule, "MM_StringFree");
        if (mmcore->MM_StringFree == nullptr) {
            throw std::runtime_error("missing MM_StringFree in MMCoreC.dll");
        }

        mmcore->MM_LoadDevice = (MM_Status (*)(MM_Session, const char *, const char *, const char *))GetProcAddress(hModule, "MM_LoadDevice");
        if (mmcore->MM_LoadDevice == nullptr) {
            throw std::runtime_error("missing MM_LoadDevice in MMCoreC.dll");
        }

        mmcore->MM_UnloadDevice = (MM_Status (*)(MM_Session, const char *))GetProcAddress(hModule, "MM_UnloadDevice");
        if (mmcore->MM_UnloadDevice == nullptr) {
            throw std::runtime_error("missing MM_UnloadDevice in MMCoreC.dll");
        }

        mmcore->MM_UnloadAllDevices = (MM_Status (*)(MM_Session))GetProcAddress(hModule, "MM_UnloadAllDevices");
        if (mmcore->MM_UnloadAllDevices == nullptr) {
            throw std::runtime_error("missing MM_UnloadAllDevices in MMCoreC.dll");
        }
        
        mmcore->MM_InitializeDevice = (MM_Status (*)(MM_Session, const char *))GetProcAddress(hModule, "MM_InitializeDevice");
        if (mmcore->MM_InitializeDevice == nullptr) {
            throw std::runtime_error("missing MM_InitializeDevice in MMCoreC.dll");
        }
        
        mmcore->MM_RegisterCallback = (void (*)(MM_Session, struct MM_EventCallback *))GetProcAddress(hModule, "MM_RegisterCallback");
        if (mmcore->MM_RegisterCallback == nullptr) {
            throw std::runtime_error("missing MM_RegisterCallback in MMCoreC.dll");
        }
        
        mmcore->MM_GetProperty = (MM_Status (*)(MM_Session, const char *, const char *, char**))GetProcAddress(hModule, "MM_GetProperty");
        if (mmcore->MM_GetProperty == nullptr) {
            throw std::runtime_error("missing MM_GetProperty in MMCoreC.dll");
        }
        
        mmcore->MM_SetPropertyString = (MM_Status (*)(MM_Session, const char *, const char *, const char *))GetProcAddress(hModule, "MM_SetPropertyString");
        if (mmcore->MM_SetPropertyString == nullptr) {
            throw std::runtime_error("missing MM_SetPropertyString in MMCoreC.dll");
        }
        
        mmcore->MM_SetFocusDevice = (MM_Status (*)(MM_Session, const char *))GetProcAddress(hModule, "MM_SetFocusDevice");
        if (mmcore->MM_SetFocusDevice == nullptr) {
            throw std::runtime_error("missing MM_SetFocusDevice in MMCoreC.dll");
        }
        
        mmcore->MM_SetPosition = (MM_Status (*)(MM_Session, const char *, double))GetProcAddress(hModule, "MM_SetPosition");
        if (mmcore->MM_SetPosition == nullptr) {
            throw std::runtime_error("missing MM_SetPosition in MMCoreC.dll");
        }
        
        mmcore->MM_GetPosition = (MM_Status (*)(MM_Session, const char *, double*))GetProcAddress(hModule, "MM_GetPosition");
        if (mmcore->MM_GetPosition == nullptr) {
            throw std::runtime_error("missing MM_GetPosition in MMCoreC.dll");
        }
    } catch (std::runtime_error) {
        delete mmcore;
        mmcore = nullptr;
        throw;
    }
}
