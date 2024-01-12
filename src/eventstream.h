#ifndef EVENTSTREAM_H
#define EVENTSTREAM_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "device/propertypath.h"

enum class EventType {
    DeviceConnectionStateChanged,
    DevicePropertyValueUpdate,
    DeviceOperationComplete,

    TaskStateChanged,
    TaskChannelChanged,
    TaskMessage,

    ExperimentOpened,
    ExperimentClosed,

    PlateCreated,
    PlateModified,
    CurrentPlateChanged,
    NDImageCreated,
    NDImageChanged,

    QuantificationCompleted,
};

std::string EventTypeToString(EventType t);

namespace DeviceConnectionState {
const std::string NotConnected = "not_connected";
const std::string Connecting = "connecting";
const std::string Connected = "connected";
const std::string ConnectionLost = "connection_lost";
const std::string Disconnecting = "disconnecting";
} // namespace DeviceConnectionState

struct Event {
    EventType type;
    std::string device;
    PropertyPath path;
    std::string value;
};

class EventStream {
public:
    bool Send(Event e);
    bool Receive(Event *e);
    void Close();

private:
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<Event> queue;

    std::atomic<bool> closed = false;
};

class EventSender {
public:
    virtual void SubscribeEvents(EventStream *stream);
    virtual void SubscribeEvents(EventStream *stream,
                                 std::function<void(Event &)> middleware);

protected:
    void SendEvent(Event e);

private:
    struct EventSubscriber {
        EventStream *stream;
        std::optional<std::function<void(Event &)>> middleware;
    };

    std::vector<EventSubscriber> subscriber_list;
};

#endif
