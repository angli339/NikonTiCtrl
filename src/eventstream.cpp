#include "eventstream.h"

std::string EventTypeToString(EventType t)
{
    switch (t) {
    case EventType::DeviceConnectionStateChanged:
        return "DeviceConnectionStateChanged";
    case EventType::DevicePropertyValueUpdate:
        return "DevicePropertyValueUpdate";
    case EventType::DeviceOperationComplete:
        return "DeviceOperationComplete";
    default:
        return "";
    }
}

bool EventStream::Send(Event e)
{
    if (closed) {
        return false;
    }

    std::unique_lock<std::mutex> lk(mutex);
    queue.push_back(e);
    lk.unlock();

    cv.notify_all();
    return true;
}

bool EventStream::Receive(Event *e)
{
    std::unique_lock<std::mutex> lk(mutex);
    cv.wait(lk, [this] { return !queue.empty() || closed; });
    if (!queue.empty()) {
        *e = queue.front();
        queue.pop_front();
        return true;
    }
    if (closed) {
        return false;
    }
    return true;
}

void EventStream::Close()
{
    closed = true;
    cv.notify_all();
}

void EventSender::SubscribeEvents(EventStream *stream)
{
    subscriber_list.emplace_back(EventSubscriber{
        .stream = stream,
    });
}

void EventSender::SubscribeEvents(EventStream *stream,
                                  std::function<void(Event &)> middleware)
{
    subscriber_list.emplace_back(EventSubscriber{
        .stream = stream,
        .middleware = middleware,
    });
}

void EventSender::SendEvent(Event e)
{
    for (const auto &[stream, middleware] : subscriber_list) {
        if (middleware.has_value()) {
            (*middleware)(e);
        }
        stream->Send(e);
    }
}
