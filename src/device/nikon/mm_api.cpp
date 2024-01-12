#include "mm_api.h"

#include <stdexcept>

#define NOMINMAX
#include <windows.h>

#include <fmt/format.h>

MMCoreC *mmcore = nullptr;

void load_MMCoreC()
{
    if (mmcore != nullptr) {
        return;
    }

    HMODULE hModule = LoadLibraryA("MMCoreC.dll");
    if (hModule == NULL) {
        throw std::runtime_error("cannot load MMCoreC.dll");
    }

    mmcore = new MMCoreC;

    try {
        mmcore->MM_Open =
            (void (*)(MM_Session *))GetProcAddress(hModule, "MM_Open");
        if (mmcore->MM_Open == nullptr) {
            throw std::runtime_error("missing MM_Open in MMCoreC.dll");
        }

        mmcore->MM_Close =
            (void (*)(MM_Session))GetProcAddress(hModule, "MM_Close");
        if (mmcore->MM_Close == nullptr) {
            throw std::runtime_error("missing MM_Close in MMCoreC.dll");
        }

        mmcore->MM_GetVersionInfo =
            (void (*)(MM_Session, char **))GetProcAddress(hModule,
                                                          "MM_GetVersionInfo");
        if (mmcore->MM_GetVersionInfo == nullptr) {
            throw std::runtime_error(
                "missing MM_GetVersionInfo in MMCoreC.dll");
        }

        mmcore->MM_GetAPIVersionInfo =
            (void (*)(MM_Session, char **))GetProcAddress(
                hModule, "MM_GetAPIVersionInfo");
        if (mmcore->MM_GetAPIVersionInfo == nullptr) {
            throw std::runtime_error(
                "missing MM_GetAPIVersionInfo in MMCoreC.dll");
        }

        mmcore->MM_GetDeviceAdapterNames =
            (MM_Status(*)(MM_Session, char ***))GetProcAddress(
                hModule, "MM_GetDeviceAdapterNames");
        if (mmcore->MM_GetDeviceAdapterNames == nullptr) {
            throw std::runtime_error(
                "missing MM_GetDeviceAdapterNames in MMCoreC.dll");
        }

        mmcore->MM_StringFree =
            (void (*)(char *))GetProcAddress(hModule, "MM_StringFree");
        if (mmcore->MM_StringFree == nullptr) {
            throw std::runtime_error("missing MM_StringFree in MMCoreC.dll");
        }

        mmcore->MM_StringListFree =
            (void (*)(char **))GetProcAddress(hModule, "MM_StringListFree");
        if (mmcore->MM_StringListFree == nullptr) {
            throw std::runtime_error(
                "missing MM_StringListFree in MMCoreC.dll");
        }

        mmcore->MM_LoadDevice =
            (MM_Status(*)(MM_Session, const char *, const char *, const char *))
                GetProcAddress(hModule, "MM_LoadDevice");
        if (mmcore->MM_LoadDevice == nullptr) {
            throw std::runtime_error("missing MM_LoadDevice in MMCoreC.dll");
        }

        mmcore->MM_UnloadDevice =
            (MM_Status(*)(MM_Session, const char *))GetProcAddress(
                hModule, "MM_UnloadDevice");
        if (mmcore->MM_UnloadDevice == nullptr) {
            throw std::runtime_error("missing MM_UnloadDevice in MMCoreC.dll");
        }

        mmcore->MM_UnloadAllDevices = (MM_Status(*)(MM_Session))GetProcAddress(
            hModule, "MM_UnloadAllDevices");
        if (mmcore->MM_UnloadAllDevices == nullptr) {
            throw std::runtime_error(
                "missing MM_UnloadAllDevices in MMCoreC.dll");
        }

        mmcore->MM_InitializeDevice =
            (MM_Status(*)(MM_Session, const char *))GetProcAddress(
                hModule, "MM_InitializeDevice");
        if (mmcore->MM_InitializeDevice == nullptr) {
            throw std::runtime_error(
                "missing MM_InitializeDevice in MMCoreC.dll");
        }

        mmcore->MM_RegisterCallback =
            (void (*)(MM_Session, struct MM_EventCallback *))GetProcAddress(
                hModule, "MM_RegisterCallback");
        if (mmcore->MM_RegisterCallback == nullptr) {
            throw std::runtime_error(
                "missing MM_RegisterCallback in MMCoreC.dll");
        }

        mmcore->MM_GetProperty =
            (MM_Status(*)(MM_Session, const char *, const char *,
                          char **))GetProcAddress(hModule, "MM_GetProperty");
        if (mmcore->MM_GetProperty == nullptr) {
            throw std::runtime_error("missing MM_GetProperty in MMCoreC.dll");
        }

        mmcore->MM_SetPropertyString =
            (MM_Status(*)(MM_Session, const char *, const char *, const char *))
                GetProcAddress(hModule, "MM_SetPropertyString");
        if (mmcore->MM_SetPropertyString == nullptr) {
            throw std::runtime_error(
                "missing MM_SetPropertyString in MMCoreC.dll");
        }

        mmcore->MM_SetFocusDevice =
            (MM_Status(*)(MM_Session, const char *))GetProcAddress(
                hModule, "MM_SetFocusDevice");
        if (mmcore->MM_SetFocusDevice == nullptr) {
            throw std::runtime_error(
                "missing MM_SetFocusDevice in MMCoreC.dll");
        }

        mmcore->MM_SetPosition =
            (MM_Status(*)(MM_Session, const char *, double))GetProcAddress(
                hModule, "MM_SetPosition");
        if (mmcore->MM_SetPosition == nullptr) {
            throw std::runtime_error("missing MM_SetPosition in MMCoreC.dll");
        }

        mmcore->MM_GetPosition =
            (MM_Status(*)(MM_Session, const char *, double *))GetProcAddress(
                hModule, "MM_GetPosition");
        if (mmcore->MM_GetPosition == nullptr) {
            throw std::runtime_error("missing MM_GetPosition in MMCoreC.dll");
        }
    } catch (std::runtime_error) {
        delete mmcore;
        mmcore = nullptr;
        throw;
    }
}

std::string MM_StatusToString(MM_Status error)
{
    switch (error) {
    case 1:
        return "MMERR_GENERIC(1)";
    case 2:
        return "MMERR_NoDevice(2)";
    case 3:
        return "MMERR_SetPropertyFailed(3)";
    case 4:
        return "MMERR_LibraryFunctionNotFound(4)";
    case 5:
        return "MMERR_ModuleVersionMismatch(5)";
    case 6:
        return "MMERR_DeviceVersionMismatch(6)";
    case 7:
        return "MMERR_UnknownModule(7)";
    case 8:
        return "MMERR_LoadLibraryFailed(8)";
    case 9:
        return "MMERR_CreateFailed(9)";
    case 10:
        return "MMERR_CreateNotFound(10)";
    case 11:
        return "MMERR_DeleteNotFound(11)";
    case 12:
        return "MMERR_DeleteFailed(12)";
    case 13:
        return "MMERR_UnexpectedDevice(13)";
    case 14:
        return "MMERR_DeviceUnloadFailed(14)";
    case 15:
        return "MMERR_CameraNotAvailable(15)";
    case 16:
        return "MMERR_DuplicateLabel(16)";
    case 17:
        return "MMERR_InvalidLabel(17)";
    case 19:
        return "MMERR_InvalidStateDevice(19)";
    case 20:
        return "MMERR_NoConfiguration(20)";
    case 21:
        return "MMERR_InvalidConfigurationIndex(21)";
    case 22:
        return "MMERR_DEVICE_GENERIC(22)";
    case 23:
        return "MMERR_InvalidPropertyBlock(23)";
    case 24:
        return "MMERR_UnhandledException(24)";
    case 25:
        return "MMERR_DevicePollingTimeout(25)";
    case 26:
        return "MMERR_InvalidShutterDevice(26)";
    case 27:
        return "MMERR_InvalidSerialDevice(27)";
    case 28:
        return "MMERR_InvalidStageDevice(28)";
    case 29:
        return "MMERR_InvalidSpecificDevice(29)";
    case 30:
        return "MMERR_InvalidXYStageDevice(30)";
    case 31:
        return "MMERR_FileOpenFailed(31)";
    case 32:
        return "MMERR_InvalidCFGEntry(32)";
    case 33:
        return "MMERR_InvalidContents(33)";
    case 34:
        return "MMERR_InvalidCoreProperty(34)";
    case 35:
        return "MMERR_InvalidCoreValue(35)";
    case 36:
        return "MMERR_NoConfigGroup(36)";
    case 37:
        return "MMERR_CameraBufferReadFailed(37)";
    case 38:
        return "MMERR_DuplicateConfigGroup(38)";
    case 39:
        return "MMERR_InvalidConfigurationFile(39)";
    case 40:
        return "MMERR_CircularBufferFailedToInitialize(40)";
    case 41:
        return "MMERR_CircularBufferEmpty(41)";
    case 42:
        return "MMERR_ContFocusNotAvailable(42)";
    case 43:
        return "MMERR_AutoFocusNotAvailable(43)";
    case 44:
        return "MMERR_BadConfigName(44)";
    case 45:
        return "MMERR_CircularBufferIncompatibleImage(45)";
    case 46:
        return "MMERR_NotAllowedDuringSequenceAcquisition(46)";
    case 47:
        return "MMERR_OutOfMemory(47)";
    case 48:
        return "MMERR_InvalidImageSequence(48)";
    case 49:
        return "MMERR_NullPointerException(49)";
    case 50:
        return "MMERR_CreatePeripheralFailed(50)";
    case 51:
        return "MMERR_PropertyNotInCache(51)";
    case 52:
        return "MMERR_BadAffineTransforn(52)";
    default:
        return fmt::format("MMErr({:d})", int(error));
    }
}