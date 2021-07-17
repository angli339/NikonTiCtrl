#ifndef DEVICECONTROL_H
#define DEVICECONTROL_H

#include <QObject>
#include <mutex>

#include "datamanager.h"
#include "device/hamamatsu_dcam.h"
#include "device/micromanager_camera.h"
#include "device/nikon_ti.h"
#include "device/prior_proscan.h"
#include "device/trigger_controller.h"
#include "utils/devnotify.h"

class DeviceControl : public QObject
{
    Q_OBJECT
public:
    explicit DeviceControl(QObject *parent = nullptr);
    ~DeviceControl();
    void setDataManager(DataManager *dataManager) { this->dataManager = dataManager; }

    void connectAll();
    void disconnectAll();
    bool isConnected() { return connected; }

    // Generic Device Property
    std::vector<std::string> listDeviceProperty(const std::string name);
    std::string getDeviceProperty(const std::string name, bool force_update=true);
    bool waitDeviceProperty(const std::vector<std::string> nameList, const std::chrono::milliseconds timeout);

    std::map<std::string, std::string> getCachedDeviceProperty();

    // Camera Control
    void setExposure(double exposure_ms);
    double getExposure();
    void setBinning(std::string binning);
    void setLevelTrigger(bool enable);
    void outputTriggerPulse();

    void allocBuffer(int n_frame);
    void releaseBuffer();
    
    void startAcquisition();
    void startContinuousAcquisition();
    void stopAcquisition();

    bool isAcquisitionReady();
    bool isAcquisitionRunning();

    void waitExposureEnd(int32_t timeout_ms);
    void waitFrameReady(int32_t timeout_ms);

    uint8_t *getFrame(uint32_t *buf_size, uint16_t *width, uint16_t *height);

public slots:
    void setDeviceProperty(const std::string name, const std::string value);

signals:
    void propertyUpdated(std::string name, std::string value);

private:
    std::atomic<bool> connected;
    bool ext_trigger_enable = false;

    DevNotify *devNotify;
    DataManager *dataManager;

    HamamatsuDCAM *hamamatsu = nullptr;
    PriorProscan *proscan = nullptr;
    NikonTi *nikon = nullptr;
    TriggerController *trigger_controller = nullptr;

    int bufferSize = 0;
};

#endif // DEVICECONTROL_H
