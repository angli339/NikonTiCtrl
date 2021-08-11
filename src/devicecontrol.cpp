#include "devicecontrol.h"

#include <QtConcurrent>
#include <thread>

#include "config.h"
#include "logger.h"
#include "utils/string_utils.h"

DeviceControl::DeviceControl(QObject *parent) : QObject(parent)
{
    devNotify = new DevNotify;

    nikon = new NikonTi;
    connect(nikon, &NikonTi::propertyUpdated, [this](const std::string name, const std::string value) {
        emit this->propertyUpdated("NikonTi/" + name, value);
    });

    proscan = new PriorProscan("ASRL1::INSTR");
    connect(proscan, &PriorProscan::propertyUpdated, [this](const std::string name, const std::string value) {
        emit this->propertyUpdated("PriorProScan/" + name, value);
    });

    trigger_controller = new TriggerController("ASRL4::INSTR");
    connect(trigger_controller, &TriggerController::propertyUpdated, [this](const std::string name, const std::string value) {
        emit this->propertyUpdated("TriggerController/" + name, value);
    });

    hamamatsu = new HamamatsuDCAM;
    connect(hamamatsu, &HamamatsuDCAM::propertyUpdated, [this](const std::string name, const std::string value) {
        if ((name == "") && (value == "Connected")) {
            hamamatsu->setExposure(50);
            hamamatsu->setBitsPerChannel(16);
        }
        emit this->propertyUpdated("Hamamatsu/" + name, value);
    });
}

DeviceControl::~DeviceControl()
{
    delete devNotify;
    devNotify = nullptr;

    disconnectAll();

    delete hamamatsu;
    hamamatsu = nullptr;

    delete proscan;
    proscan = nullptr;

    delete nikon;
    nikon = nullptr;
}

#include "utils/wmi.h"

void DeviceControl::connectAll()
{
    LOG_INFO("DeviceControl: connecting all devices...");

    auto f1 = QtConcurrent::run(nikon, &NikonTi::connect);
    auto f2 = QtConcurrent::run(proscan, &PriorProscan::connect);
    auto f3 = QtConcurrent::run(hamamatsu, &HamamatsuDCAM::connect);
    auto f4 = QtConcurrent::run(trigger_controller, &TriggerController::connect);

    f1.waitForFinished();
    f2.waitForFinished();
    f3.waitForFinished();
    f4.waitForFinished();

    connected = true;
}

void DeviceControl::disconnectAll()
{
    LOG_INFO("DeviceControl: disconnecting all devices...");

    auto f1 = QtConcurrent::run(nikon, &NikonTi::disconnect);
    auto f2 = QtConcurrent::run(proscan, &PriorProscan::disconnect);
    auto f3 = QtConcurrent::run(hamamatsu, &HamamatsuDCAM::disconnect);
    auto f4 = QtConcurrent::run(trigger_controller, &TriggerController::disconnect);

    f1.waitForFinished();
    f2.waitForFinished();
    f3.waitForFinished();
    f4.waitForFinished();
}

void DeviceControl::setExposure(double exposure_ms)
{
    if (ext_trigger_enable) {
        trigger_controller->setTriggerPulseWidth(exposure_ms);
    } else {
        hamamatsu->setExposure(exposure_ms);
    }
}

double DeviceControl::getExposure()
{
    if (ext_trigger_enable) {
        return trigger_controller->getTriggerPulseWidth();
    } else {
        return hamamatsu->getExposure();
    }
}

void DeviceControl::setLevelTrigger(bool enable)
{
    ext_trigger_enable = enable;
    if (enable) {
        hamamatsu->setDeviceProperty("TRIGGER SOURCE", "EXTERNAL");
        hamamatsu->setDeviceProperty("TRIGGER ACTIVE", "LEVEL");
        hamamatsu->setDeviceProperty("TRIGGER POLARITY", "POSITIVE");
    } else {
        hamamatsu->setDeviceProperty("TRIGGER SOURCE", "INTERNAL");
    }
}

void DeviceControl::outputTriggerPulse()
{
    trigger_controller->outputTriggerPulse();
}

void DeviceControl::setBinning(std::string binning)
{
    hamamatsu->setDeviceProperty("BINNING", binning);
}

void DeviceControl::allocBuffer(int n_frame)
{
    return hamamatsu->allocBuffer(n_frame);
}

void DeviceControl::releaseBuffer()
{
    int n_attempts = 200;

    for (int i = 0; i < n_attempts; i++) {
        if (hamamatsu->isBusy()) {
            LOG_WARN("releaseBuffer: camera is busy, wait 10 ms and check again...");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            break;
        }

        if (i == n_attempts - 1) {
            LOG_ERROR("camera is still busy, fail");
        }
    }
    return hamamatsu->releaseBuffer();
}

void DeviceControl::startAcquisition()
{
    hamamatsu->startAcquisition();
}

void DeviceControl::startContinuousAcquisition()
{
    hamamatsu->startContinuousAcquisition();
}

void DeviceControl::stopAcquisition()
{
    hamamatsu->stopAcquisition();
}

bool DeviceControl::isAcquisitionReady()
{
    return hamamatsu->isAcquisitionReady();
}

bool DeviceControl::isAcquisitionRunning()
{
    return hamamatsu->isAcquisitionRunning();
}

void DeviceControl::waitExposureEnd(int32_t timeout_ms)
{
    hamamatsu->waitExposureEnd(timeout_ms);
}

void DeviceControl::waitFrameReady(int32_t timeout_ms)
{
    hamamatsu->waitFrameReady(timeout_ms);
}

uint8_t *DeviceControl::getFrame(uint32_t *buf_size, uint16_t *width, uint16_t *height)
{
    if (buf_size != nullptr) {
        *buf_size = hamamatsu->getBytesPerPixel() * hamamatsu->getImageWidth() * hamamatsu->getImageHeight();
    }
    if (width != nullptr) {
        *width = hamamatsu->getImageWidth();
    }
    if (height != nullptr) {
        *height = hamamatsu->getImageHeight();
    }
    return hamamatsu->getFrame();
}

std::vector<std::string> DeviceControl::listDeviceProperty(const std::string name)
{
    if (name == "") {
        return {"NikonTi", "Hamamatsu", "PriorProScan"};
    }

    std::string prefix;
    std::vector<std::string> list;

    if (name == "NikonTi") {
        prefix = "NikonTi/";
        list = nikon->listDeviceProperty();
    } else if (name == "Hamamatsu") {
        prefix = "Hamamatsu/";
        list = hamamatsu->listDeviceProperty();
    } else if (name == "PriorProScan") {
        prefix = "PriorProScan/";
        list = proscan->listDeviceProperty();
    } else {
        throw std::invalid_argument("invalid name");
    }

    for (auto& name : list) {
        name = prefix + name;
    }
    return list;
}

std::string DeviceControl::getDeviceProperty(const std::string name, bool force_update)
{
    const std::string nikon_prefix = "NikonTi/";
    const std::string hamamatsu_prefix = "Hamamatsu/";
    const std::string proscan_prefix = "PriorProScan/";

    if (util::hasPrefix(name, nikon_prefix)) {
        return nikon->getDeviceProperty(util::trimPrefix(name, nikon_prefix), force_update);
    }
    if (util::hasPrefix(name, hamamatsu_prefix)) {
        return hamamatsu->getDeviceProperty(util::trimPrefix(name, hamamatsu_prefix), force_update);
    }
    if (util::hasPrefix(name, proscan_prefix)) {
        return proscan->getDeviceProperty(util::trimPrefix(name, proscan_prefix), force_update);
    }
    throw std::invalid_argument("invalid name");
}

void DeviceControl::setDeviceProperty(const std::string name, const std::string value)
{
    const std::string nikon_prefix = "NikonTi/";
    const std::string hamamatsu_prefix = "Hamamatsu/";
    const std::string proscan_prefix = "PriorProScan/";

    if (util::hasPrefix(name, nikon_prefix)) {
        return nikon->setDeviceProperty(util::trimPrefix(name, nikon_prefix), value);
    }
    if (util::hasPrefix(name, hamamatsu_prefix)) {
        return hamamatsu->setDeviceProperty(util::trimPrefix(name, hamamatsu_prefix), value);
    }
    if (util::hasPrefix(name, proscan_prefix)) {
        return proscan->setDeviceProperty(util::trimPrefix(name, proscan_prefix), value);
    }
    throw std::invalid_argument("invalid name");
}

bool DeviceControl::waitDeviceProperty(const std::vector<std::string> nameList, const std::chrono::milliseconds timeout)
{
    std::vector<std::string> nikonList;
    for (const auto& name : nameList) {
        if (name.rfind("NikonTi/", 0) == 0) {
            nikonList.push_back(name.substr(std::string("NikonTi/").length()));
        }
    }

    std::vector<std::string> proscanList;
    for (const auto& name : nameList) {
        if (name.rfind("PriorProScan/", 0) == 0) {
            proscanList.push_back(name.substr(std::string("PriorProScan/").length()));
        }
    }

    bool nikonStatus;
    bool proscanStatus;
    std::thread waitNikon([&nikonStatus, this, nikonList, timeout]{
        nikonStatus = nikon->waitDeviceProperty(nikonList, timeout);
    });

    std::thread waitProscan([&proscanStatus, this, proscanList, timeout]{
        proscanStatus = proscan->waitDeviceProperty(proscanList, timeout);
    });

    waitNikon.join();
    waitProscan.join();

    return nikonStatus & proscanStatus;
}

std::map<std::string, std::string> DeviceControl::getCachedDeviceProperty()
{
    std::map<std::string, std::string> cachedProperty;
    if (nikon->isConnected()) {
        for (const auto& [name, value] : nikon->getCachedDeviceProperty()) {
            cachedProperty["NikonTi/" + name] = value;
        }
    }
    if (proscan->isConnected()) {
        for (const auto& [name, value] : proscan->getCachedDeviceProperty()) {
            cachedProperty["PriorProScan/" + name] = value;
        }
    }
    return cachedProperty;
}
