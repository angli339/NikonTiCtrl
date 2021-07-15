#ifndef PROPERTYSTATUS_H
#define PROPERTYSTATUS_H

#include <chrono>
#include <list>
#include <string>
#include <mutex>
#include <condition_variable>

enum prop_event_type {
    PROP_EVENT_GET = 1,
    PROP_EVENT_SET = 2,
    PROP_EVENT_UPDATED = 3,
};

struct PropertyEvent
{
    prop_event_type type;
    std::string name;
    std::string value;
    std::chrono::steady_clock::time_point tStart;
    std::chrono::steady_clock::time_point tEnd;

    inline PropertyEvent(prop_event_type type, std::string name, std::string value = "") {
        this->type = type;
        this->name = name;
        this->value = value;
        this->tStart = std::chrono::steady_clock::now();
        if (type == PROP_EVENT_UPDATED) {
            this->tEnd = this->tStart;
        }
    }

    inline void completed(std::string value = "") {
        if (value != "") {
            this->value = value;
        }
        this->tEnd = std::chrono::steady_clock::now();
    }
};

struct PropertyStatus
{
    std::mutex mu;
    std::list<PropertyEvent *> eventLog;
    PropertyEvent *current = nullptr;
    PropertyEvent *pendingSet = nullptr;
    std::condition_variable setCompleted;
};

#endif // PROPERTYSTATUS_H
