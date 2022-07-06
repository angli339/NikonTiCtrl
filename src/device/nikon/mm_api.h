#ifndef MM_API
#define MM_API

#include <cstdint>
#include <string>

typedef void *MM_Session;

typedef enum
{
    MM_ErrOK = 0,
    MM_ErrGENERIC = 1, // unspecified error
    MM_ErrNoDevice = 2,
    MM_ErrSetPropertyFailed = 3,
    MM_ErrLibraryFunctionNotFound = 4,
    MM_ErrModuleVersionMismatch = 5,
    MM_ErrDeviceVersionMismatch = 6,
    MM_ErrUnknownModule = 7,
    MM_ErrLoadLibraryFailed = 8,
    MM_ErrCreateFailed = 9,
    MM_ErrCreateNotFound = 10,
    MM_ErrDeleteNotFound = 11,
    MM_ErrDeleteFailed = 12,
    MM_ErrUnexpectedDevice = 13,
    MM_ErrDeviceUnloadFailed = 14,
    MM_ErrCameraNotAvailable = 15,
    MM_ErrDuplicateLabel = 16,
    MM_ErrInvalidLabel = 17,
    MM_ErrInvalidStateDevice = 19,
    MM_ErrNoConfiguration = 20,
    MM_ErrInvalidConfigurationIndex = 21,
    MM_ErrDEVICE_GENERIC = 22,
    MM_ErrInvalidPropertyBlock = 23,
    MM_ErrUnhandledException = 24,
    MM_ErrDevicePollingTimeout = 25,
    MM_ErrInvalidShutterDevice = 26,
    MM_ErrInvalidSerialDevice = 27,
    MM_ErrInvalidStageDevice = 28,
    MM_ErrInvalidSpecificDevice = 29,
    MM_ErrInvalidXYStageDevice = 30,
    MM_ErrFileOpenFailed = 31,
    MM_ErrInvalidCFGEntry = 32,
    MM_ErrInvalidContents = 33,
    MM_ErrInvalidCoreProperty = 34,
    MM_ErrInvalidCoreValue = 35,
    MM_ErrNoConfigGroup = 36,
    MM_ErrCameraBufferReadFailed = 37,
    MM_ErrDuplicateConfigGroup = 38,
    MM_ErrInvalidConfigurationFile = 39,
    MM_ErrCircularBufferFailedToInitialize = 40,
    MM_ErrCircularBufferEmpty = 41,
    MM_ErrContFocusNotAvailable = 42,
    MM_ErrAutoFocusNotAvailable = 43,
    MM_ErrBadConfigName = 44,
    MM_ErrCircularBufferIncompatibleImage = 45,
    MM_ErrNotAllowedDuringSequenceAcquisition = 46,
    MM_ErrOutOfMemory = 47,
    MM_ErrInvalidImageSequence = 48,
    MM_ErrNullPointerException = 49,
    MM_ErrCreatePeripheralFailed = 50,
    MM_ErrPropertyNotInCache = 51,
    MM_ErrBadAffineTransform = 52
} MM_Status;

std::string MM_StatusToString(MM_Status error);

typedef enum
{
    MM_UnknownType = 0,
    MM_AnyType,
    MM_CameraDevice,
    MM_ShutterDevice,
    MM_StateDevice,
    MM_StageDevice,
    MM_XYStageDevice,
    MM_SerialDevice,
    MM_GenericDevice,
    MM_AutoFocusDevice,
    MM_CoreDevice,
    MM_ImageProcessorDevice,
    MM_SignalIODevice,
    MM_MagnifierDevice,
    MM_SLMDevice,
    MM_HubDevice,
    MM_GalvoDevice
} MM_DeviceType;

typedef enum
{
    MM_Undef,
    MM_String,
    MM_Float,
    MM_Integer
} MM_PropertyType;

// Event callback
struct MM_EventCallback {
    void (*onPropertiesChanged)(MM_Session mm);
    void (*onPropertyChanged)(MM_Session mm, const char *label,
                              const char *propName, const char *propValue);
    void (*onConfigGroupChanged)(MM_Session mm, const char *groupName,
                                 const char *newConfigName);
    void (*onSystemConfigurationLoaded)(MM_Session mm);
    void (*onPixelSizeChanged)(MM_Session mm, double newPixelSizeUm);
    void (*onStagePositionChanged)(MM_Session mm, char *label, double pos);
    void (*onXYStagePositionChanged)(MM_Session mm, char *label, double xpos,
                                     double ypos);
    void (*onExposureChanged)(MM_Session mm, char *label, double newExposure);
    void (*onSLMExposureChanged)(MM_Session mm, char *label,
                                 double newExposure);
};

struct MMCoreC {
    void (*MM_Open)(MM_Session *mm);
    void (*MM_Close)(MM_Session mm);
    void (*MM_GetVersionInfo)(MM_Session mm, char **info);
    void (*MM_GetAPIVersionInfo)(MM_Session mm, char **info);

    MM_Status (*MM_GetDeviceAdapterNames)(MM_Session mm, char ***names);

    void (*MM_StringFree)(char *str);
    void (*MM_StringListFree)(char **str_list);

    MM_Status (*MM_LoadDevice)(MM_Session mm, const char *label,
                              const char *module_name, const char *device_name);
    MM_Status (*MM_UnloadDevice)(MM_Session mm, const char *label);
    MM_Status (*MM_UnloadAllDevices)(MM_Session mm);
    MM_Status (*MM_InitializeDevice)(MM_Session mm, const char *label);
    void (*MM_RegisterCallback)(MM_Session mm,
                                struct MM_EventCallback *callback);
    MM_Status (*MM_GetProperty)(MM_Session mm, const char *label,
                               const char *prop_name, char **value);
    MM_Status (*MM_SetPropertyString)(MM_Session mm, const char *label,
                                     const char *prop_name, const char *value);
    MM_Status (*MM_SetFocusDevice)(MM_Session mm, const char *label);
    MM_Status (*MM_SetPosition)(MM_Session mm, const char *label,
                               double position);
    MM_Status (*MM_GetPosition)(MM_Session mm, const char *label,
                               double *position);
};

extern MMCoreC *mmcore;
void load_MMCoreC();

#endif
