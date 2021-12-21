#include "qt/devicecontrol_model.h"

#include "logging.h"

DeviceControlModel::DeviceControlModel(DeviceHub *hub, QObject *parent)
    : QObject(parent)
{
    this->hub = hub;
    handleEventFuture =
        std::async(std::launch::async, &DeviceControlModel::handleEvents, this);
    hub->SubscribeEvents(&eventStream);
}

DeviceControlModel::~DeviceControlModel()
{
    eventStream.Close();
    handleEventFuture.get();
}

void DeviceControlModel::setPropertyValue(QString propertyPath, QString value)
{
    hub->SetProperty(propertyPath.toStdString(), value.toStdString());
}

void DeviceControlModel::handleEvents()
{
    Event e;
    while (eventStream.Receive(&e)) {
        if (e.type == EventType::DevicePropertyValueUpdate) {
            emit propertyValueChanged(e.path.ToString().c_str(),
                                      e.value.c_str());
        } else if (e.type == EventType::DeviceConnectionStateChanged) {
            if (e.value == DeviceConnectionState::Connected) {
                emit deviceConnected(e.device.c_str());
            } else if (e.value == DeviceConnectionState::NotConnected) {
                emit deviceDisconnected(e.device.c_str());
            }
        }
    }
}
