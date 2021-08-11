#ifndef NIKONTI_H
#define NIKONTI_H

#include <QObject>
#include <QTimer>
#include <QMutex>

#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

#include "propertystatus.h"

typedef void *MM_Session;

class NikonTi : public QObject
{
    Q_OBJECT
public:
    explicit NikonTi(QObject *parent = nullptr);
    ~NikonTi();

    bool detectDevice();
    void connect();
    void disconnect();
    bool isConnected() { return connected; }

    std::vector<std::string> listDeviceProperty();
    std::string getDevicePropertyDescription(const std::string name);

    std::string getDeviceProperty(const std::string name, bool force_update = true);
    void setDeviceProperty(const std::string name, const std::string value);
    bool waitDeviceProperty(const std::vector<std::string> name, const std::chrono::milliseconds timeout);

    std::unordered_map<std::string, std::string> getCachedDeviceProperty();

    void onPropertyChangedCallback(MM_Session mmc, const char* label, const char* property, const char* value);
    void onStagePositionChangedCallback(MM_Session mmc, char* label, double pos);

signals:
    void propertyUpdated(const std::string name, const std::string value);

private:
    std::atomic<bool> connected = false;

    std::mutex mu_mmc;
    MM_Session mmc = nullptr;
    std::vector<std::string> loadedModules;
    std::vector<std::string> propertyList;
    bool isModuleLoaded(const std::string module);

    std::unordered_map<std::string, PropertyStatus *> propertyCache;
    void processPropertyEvent(PropertyEvent* event);
};

#endif // NIKONTI_H
