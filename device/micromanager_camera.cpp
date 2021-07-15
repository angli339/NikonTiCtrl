#include "micromanager_camera.h"

#include <stdexcept>
#include <string>

#include "MMCoreC.h"

MicroManagerCamera::MicroManagerCamera(std::string moduleName, QObject *parent) : QObject(parent)
{
    MM_Open(&mmc);

    MM_Status status;
    const char *path[] = {
        "C:\\Program Files\\Micro-Manager-1.4",
        NULL,
    };
    MM_SetDeviceAdapterSearchPaths(mmc, path);

    char **deviceAdapters;
    status = MM_GetDeviceAdapterNames(mmc, &deviceAdapters);
    if (status != 0) {
        MM_Close(mmc);
        mmc = nullptr;
    }

    for (char **adapter = deviceAdapters; *adapter; adapter++) {
        if (moduleName == *adapter) {
            MM_StringListFree(deviceAdapters);
            this->moduleName = moduleName;
            return;
        }
    }
    MM_StringListFree(deviceAdapters);

    throw std::invalid_argument("module not found");
}

MicroManagerCamera::~MicroManagerCamera() {
    uint8_t running;
    MM_IsSequenceRunning(mmc, &running);
    if (running) {
        MM_StopSequenceAcquisition(mmc);
    }
    if (mmc != nullptr) {
        MM_Close(mmc);
        qDebug("Camera: MMCore Close: done.");
        mmc = nullptr;
    }
}

std::vector<std::string> MicroManagerCamera::listDevices() {
    MM_Status status;
    char **deviceNames;
    status = MM_GetAvailableDevices(mmc, moduleName.c_str(), &deviceNames);
    if (status != 0) {
        throw std::runtime_error(std::string("MM_GetAvailableDevices: MMCore Error ") + std::to_string(status));
    }

    std::vector<std::string> devices;
    for (char **name = deviceNames; *name; name++) {
        devices.push_back(std::string(*name));
    }
    MM_StringListFree(deviceNames);

    return devices;
}

void MicroManagerCamera::openDevice(std::string deviceName) {
    MM_Status status;
    status = MM_LoadDevice(mmc, deviceName.c_str(), moduleName.c_str(), deviceName.c_str());
    if (status != 0) {
        throw std::runtime_error(std::string("MM_LoadDevice: MMCore Error ") + std::to_string(status));
    }
    status = MM_InitializeDevice(mmc, deviceName.c_str());
    if (status != 0) {
        throw std::runtime_error(std::string("MM_InitializeDevice: MMCore Error ") + std::to_string(status));
    }

    status = MM_SetCameraDevice(mmc, deviceName.c_str());
    if (status != 0) {
        throw std::runtime_error(std::string("MM_SetCameraDevice: MMCore Error ") + std::to_string(status));
    }

    this->deviceName = deviceName;
    return;
}

void MicroManagerCamera::setBitsPerPixel(uint8_t bits_per_pixel) {
    MM_Status status;
    status = MM_SetPropertyInt(mmc, deviceName.c_str(), "Bits per Channel", bits_per_pixel);
    if (status != 0) {
        qWarning("MicroManagerCamera: MM_SetProperty Bits per Channel. Err=%d", status);
    }
}

void MicroManagerCamera::setExposure(double exposure_ms) {
    MM_Status status;
    status = MM_SetExposure(mmc, exposure_ms);
    if (status != 0) {
        throw std::runtime_error(std::string("MM_SetExposure: MMCore Error ") + std::to_string(status));
    }
    return;
}

double MicroManagerCamera::getExposure() {
    MM_Status status;
    double exposure_ms;
    status = MM_GetExposure(mmc, &exposure_ms);
    if (status != 0) {
        throw std::runtime_error(std::string("MM_GetExposure: MMCore Error ") + std::to_string(status));
    }
    return exposure_ms;
}

void MicroManagerCamera::snapImage() {
    MM_Status status;
    status = MM_SnapImage(mmc);
    if (status != 0) {
        throw std::runtime_error(std::string("MM_SnapImage: MMCore Error ") + std::to_string(status));
    }
}

uint8_t *MicroManagerCamera::getImage() {
    MM_Status status;
    uint8_t *buffer;
    status = MM_GetImage(mmc, &buffer);
    if (status != 0) {
        throw std::runtime_error(std::string("MM_GetImage: MMCore Error ") + std::to_string(status));
    }
    return buffer;
}

uint16_t MicroManagerCamera::getImageWidth() {
    uint16_t width;
    MM_GetImageWidth(mmc, &width);
    return width;
}

uint16_t MicroManagerCamera::getImageHeight() {
    uint16_t height;
    MM_GetImageHeight(mmc, &height);
    return height;
}

uint8_t MicroManagerCamera::getBytesPerPixel() {
    uint8_t bytes;
    MM_GetBytesPerPixel(mmc, &bytes);
    return bytes;
}

void MicroManagerCamera::startContinuousSequenceAcquisition() {
    MM_Status status;
    status = MM_StartContinuousSequenceAcquisition(mmc, 0);
    if (status != 0) {
        throw std::runtime_error(std::string("MM_StartContinuousSequenceAcquisition: MMCore Error ") + std::to_string(status));
    }
}

void MicroManagerCamera::stopSequenceAcquisition() {
    MM_Status status;
    status = MM_StopSequenceAcquisition(mmc);
    if (status != 0) {
        throw std::runtime_error(std::string("MM_StopSequenceAcquisition: MMCore Error ") + std::to_string(status));
    }
}

bool MicroManagerCamera::isSequenceRunning() {
    uint8_t running;
    MM_IsSequenceRunning(mmc, &running);
    if (running == 0) {
        return false;
    }
    return true;
}


uint8_t *MicroManagerCamera::getNextImage() {
    int16_t count;

    for (;;) {
        uint8_t running;
        MM_IsSequenceRunning(mmc, &running);
        if (running == 0) {
            return nullptr;
        }

        MM_GetRemainingImageCount(mmc, &count);

        if (count > 0) {
            uint8_t *buf;
            MM_Status status;
            status = MM_PopNextImage(mmc, &buf);
            if (status != 0) {
                throw std::runtime_error(std::string("MM_PopNextImage: MMCore Error ") + std::to_string(status));
            }
            return buf;
        }
    }
}
