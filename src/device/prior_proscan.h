#ifndef PRIORPROSCAN_H
#define PRIORPROSCAN_H

#include <mutex>
#include <QObject>
#include <string>
#include <thread>
#include <unordered_map>
#include <visatype.h>

#include "propertystatus.h"

class PriorProscan : public QObject
{
    Q_OBJECT
public:
    explicit PriorProscan(std::string name, QObject *parent = nullptr);
    ~PriorProscan();

    bool detectDevice();
    void connect();
    void disconnect();
    bool isConnected() { return connected; }

    std::vector<std::string> listDeviceProperty();
    std::string getDeviceProperty(const std::string name, bool force_update = true, std::string caller="");
    void setDeviceProperty(const std::string name, const std::string value, std::string caller="");
    bool waitDeviceProperty(const std::vector<std::string> name, const std::chrono::milliseconds timeout);
    std::string setGetDeviceProperty(const std::string name, const std::string value, std::string caller="");

    std::unordered_map<std::string, std::string> getCachedDeviceProperty();

signals:
    void propertyUpdated(const std::string name, const std::string value);

private:
    std::atomic<bool> connected = false;
    std::atomic<bool> polling = false;
    std::thread threadPolling;

    std::mutex mu;
    std::string portName;
    ViSession rm = 0;
    ViSession dev = 0;

    uint8_t lumenOutputIntensity = 100;

    bool checkCommunication(int n_retry);
    void switchBaudrate();

    uint32_t clearReadBuffer();
    void write(const std::string command, std::string caller="");
    std::string readline(std::string caller="");
    std::string query(const std::string command, std::string caller="");

    const double xyResolution = 0.1;
    std::string xyPositionFromRaw(const std::string raw_xy_position);
    std::string xyPositionToRaw(const std::string xy_position);

    std::unordered_map<std::string, PropertyStatus *> propertyCache;
    void processPropertyEvent(PropertyEvent* event);
};

#endif // PRIORPROSCAN_H
