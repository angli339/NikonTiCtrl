#ifndef HAMAMATSUDCAM_H
#define HAMAMATSUDCAM_H

#include <cstdint>
#include <string>
#include <mutex>
#include <unordered_map>
#include <QObject>

typedef struct tag_dcam* HDCAM;
struct HamamatsuPropInfo;
typedef struct DCAMWAIT* HDCAMWAIT;

class HamamatsuDCAM : public QObject
{
    Q_OBJECT
public:
    explicit HamamatsuDCAM(QObject *parent = nullptr);
    ~HamamatsuDCAM();

    bool detectDevice();
    void connect();
    void disconnect();
    bool isConnected() { return connected; }

    std::vector<std::string> listDeviceProperty();
    std::string getDeviceProperty(const std::string name, bool force_update=true);
    void setDeviceProperty(const std::string name, const std::string value);
    std::unordered_map<std::string, std::string> getAllDeviceProperty();

    void setBitsPerChannel(uint8_t bits_per_channel);
    uint8_t getBitsPerChannel();
    uint8_t getNumChannels();

    uint8_t getBytesPerPixel();
    uint16_t getImageWidth();
    uint16_t getImageHeight();

    void setExposure(double exposure_ms);
    double getExposure();

    bool isBusy();
    void allocBuffer(int bufferSize);
    void releaseBuffer();

    void startAcquisition();
    void startContinuousAcquisition();
    void stopAcquisition();

    bool isAcquisitionReady();
    bool isAcquisitionRunning();

    void waitExposureEnd(int32_t timeout_ms);
    void waitFrameReady(int32_t timeout_ms);

    uint8_t *getFrame();

signals:
    void propertyUpdated(const std::string name, const std::string value);

private:
    std::mutex hdcam_mutex;
    HDCAM hdcam = nullptr;
    bool connected = false;

    bool bufferOutdated = false;
    int bufferSize = 0;

    HDCAMWAIT pending_hwait = nullptr;

    std::unordered_map<std::string, struct HamamatsuPropInfo*> propInfo;
};

#endif // HAMAMATSUDCAM_H
