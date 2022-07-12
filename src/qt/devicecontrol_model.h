#ifndef DEVICECONTROLMODEL_H
#define DEVICECONTROLMODEL_H

#include <future>

#include <QObject>

#include "device/devicehub.h"
#include "eventstream.h"

class DeviceControlModel : public QObject {
    Q_OBJECT
public:
    explicit DeviceControlModel(DeviceHub *dev, QObject *parent = nullptr);
    ~DeviceControlModel();
    void setPropertyValue(QString propertyPath, QString value);

signals:
    void propertyValueChanged(QString propertyPath, QString value);
    void deviceConnected(QString devName);
    void deviceDisconnected(QString devName);

private:
    DeviceHub *dev;
    EventStream eventStream;
    std::future<void> handleEventFuture;

    void handleEvents();
};

#endif
