#ifndef MICROMANAGERCAMERA_H
#define MICROMANAGERCAMERA_H

#include <string>
#include <stdint.h>

#include <QObject>

typedef void *MM_Session;

class MicroManagerCamera : public QObject
{
    Q_OBJECT
public:
    explicit MicroManagerCamera(std::string moduleName, QObject *parent = nullptr);
    ~MicroManagerCamera();

    std::vector<std::string> listDevices();
    void openDevice(std::string deviceName);

    void setBitsPerPixel(uint8_t bits_per_pixel);
    uint8_t getBitsPerPixel();

    void setExposure(double exposure_ms);
    double getExposure();

    uint16_t getImageWidth();
    uint16_t getImageHeight();
    uint8_t getBytesPerPixel();

    void snapImage();
    uint8_t *getImage();

    void startContinuousSequenceAcquisition();
    void stopSequenceAcquisition();
    uint8_t *getNextImage();
    bool isSequenceRunning();

private:
    MM_Session mmc = nullptr;
    std::string moduleName;
    std::string deviceName;
};

#endif // MICROMANAGERCAMERA_H
