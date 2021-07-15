#ifndef TRIGGER_CONTROLLER_H
#define TRIGGER_CONTROLLER_H

#include <string>
#include <mutex>

#include <QObject>

#include "propertystatus.h"

#include "visatype.h"

class TriggerController : public QObject
{
    Q_OBJECT
public:
    explicit TriggerController(std::string name, QObject *parent = nullptr);
    ~TriggerController();

    void connect();
    void disconnect();
    bool isConnected() { return connected; }

    void setTriggerPulseWidth(double duration_ms);
    double getTriggerPulseWidth();
    void outputTriggerPulse();

signals:
    void propertyUpdated(const std::string name, const std::string value);

private:
    std::atomic<bool> connected = false;

    std::mutex mu;
    std::string portName;
    ViSession rm = 0;
    ViSession dev = 0;

    uint32_t clearReadBuffer();
    void write(const std::string command, std::string caller="");
    std::string readline(std::string caller="");
    std::string query(const std::string command, std::string caller="");
};

#endif
