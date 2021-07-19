#include "nikon_ti.h"
#include "nikon_ti_property.h"

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <stdexcept>
#include <unordered_map>
#include <thread>
#include <functional>

#include <fmt/format.h>
#include "logger.h"
#include "utils/wmi.h"
#include "propertystatus.h"

#include "MMCoreC.h"

NikonTi *g_nikon_ti = nullptr;

void onPropertyChanged(MM_Session mmc, const char* label, const char* property, const char* value) {
    if (g_nikon_ti == nullptr) {
        return;
    }
    g_nikon_ti->onPropertyChangedCallback(mmc, label, property, value);
}

void onStagePositionChanged(MM_Session mmc, char* label, double pos) {
    if (g_nikon_ti == nullptr) {
        return;
    }
    g_nikon_ti->onStagePositionChangedCallback(mmc, label, pos);
}

struct MM_EventCallback mmCallback = {
    .onPropertyChanged = onPropertyChanged,
    .onStagePositionChanged = onStagePositionChanged,
};

NikonTi::NikonTi(QObject *parent) : QObject(parent)
{
}

NikonTi::~NikonTi()
{
    if (connected) {
        disconnect();
    }

    for (const auto& kv : propertyCache) {
        PropertyStatus *prop = kv.second;

        std::lock_guard<std::mutex> lk(prop->mu);

        for (PropertyEvent *event : prop->eventLog) {
            if (event == prop->pendingSet) {
                prop->pendingSet = nullptr;
            }
            if (event == prop->current) {
                prop->current = nullptr;
            }
            delete event;
        }

        if (prop->pendingSet != nullptr) {
            delete prop->pendingSet;
        }
        if (prop->current != nullptr) {
            delete prop->current;
        }

        delete prop;
    }
}

bool NikonTi::detectDevice()
{
    WMI w;
    std::vector<std::string> usbList;

    try {
        usbList = w.listUSBDeviceID("04B0", "7832");
    } catch (std::runtime_error &e) {
        LOG_DEBUG("NikonTi::detectDevice failed: %s", e.what());
        throw e;
    }

    if (usbList.size() > 0) {
        LOG_DEBUG("NikonTi: detected {}", usbList[0]);
    }
    return usbList.size() != 0;
}

void NikonTi::connect()
{
    if (!detectDevice()) {
        LOG_WARN("NikonTi: USB connection not detected...");
        emit propertyUpdated("", "Disconnected");
        return;
    }

    LOG_INFO("NikonTi: connecting...");
    emit propertyUpdated("", "Connecting");
    utils::stopwatch sw;
    std::unique_lock<std::mutex> lk(mu_mmc);

    MM_Open(&mmc);

    MM_Status status;

    //
    // Initialize microscope hub
    //
    status = MM_LoadDevice(mmc, "TIScope", "NikonTI", "TIScope");
    if (status != 0) {
        LOG_ERROR("NikonTi: Failed to load TIScope. Err={}", status);

        MM_Close(mmc);
        mmc = nullptr;
        throw std::runtime_error("failed to load TIScope");
    }
    status = MM_InitializeDevice(mmc, "TIScope");
    if (status != 0) {
        LOG_ERROR("NikonTi: Failed to init TIScope. Err={}", status);

        MM_Close(mmc);
        mmc = nullptr;
        throw std::runtime_error("failed to init TIScope");
    }

    loadedModules.push_back("TIScope");

    //
    // Initialize modules
    //
    std::vector<std::string> modules = {
        "TIFilterBlock1",
        "TIZDrive",
        "TIDiaShutter",
        "TINosePiece",
        "TILightPath",
        "TIPFSOffset",
        "TIPFSStatus",
    };

    for(const auto& module : modules) {
        status = MM_LoadDevice(mmc, module.c_str(), "NikonTI", module.c_str());
        if (status != 0) {
            LOG_ERROR("NikonTi: Failed to load module {}. Err={}", module.c_str(), status);
            continue;
        }
        status = MM_InitializeDevice(mmc, module.c_str());
        if (status != 0) {
            LOG_ERROR("NikonTi: Failed to init module {}. Err={}", module.c_str(), status);
            continue;
        }

        loadedModules.push_back(module);

        // Set focus device so that we can get Z focus with GetPosition()
        if (module == "TIZDrive") {
            status = MM_SetFocusDevice(mmc, "TIZDrive");
            if (status != 0) {
                LOG_ERROR("NikonTi: MM_SetFocusDevice(TIZDrive) failed. Err={}. Unloading...", status);
                status = MM_UnloadDevice(mmc, module.c_str());
                if (status != 0) {
                    LOG_ERROR("NikonTi: MM_UnloadDevice(TIZDrive) failed. Err={}", status);
                } else {
                    LOG_INFO("NikonTi: TIZDrive unloaded");
                    loadedModules.pop_back();
                }
            }
        }
    }

    //
    // List properties from all loaded modules
    //
    for (const auto& kv : propInfoNikonTi) {
        std::string name = kv.first;
        auto& info = kv.second;

        if (std::find(loadedModules.begin(),
                      loadedModules.end(),
                      info.mmLabel) != loadedModules.end()) {
             propertyList.push_back(name);
        }
    }

    //
    // Init property cache
    //
    for (const auto& propName : propertyList) {
        propertyCache.insert({propName, new PropertyStatus});
    }

    g_nikon_ti = this;
    MM_RegisterCallback(mmc, &mmCallback);

//    QObject::connect(&volatilePropertyUpdateTimer, &QTimer::timeout, this, &NikonTi::updateVolatileProperties);
//    volatilePropertyUpdateTimer.start(100);

    
    connected = true;
    lk.unlock();

    //
    // Enumerate properties
    //
    for (const auto& name : propertyList) {
        auto value = getDeviceProperty(name);
        LOG_INFO("NikonTi: [Init] {}='{}'", name, value);
    }

    emit propertyUpdated("", "Connected");
    LOG_INFO("NikonTi: connected in {:.3f}ms", stopwatch_ms(sw));
}

void NikonTi::disconnect()
{
    std::lock_guard<std::mutex> lk(mu_mmc);

    if (mmc == nullptr) {
        return;
    }

    LOG_INFO("NikonTi: disconnecting...");
    emit propertyUpdated("", "Disconnecting");
    utils::stopwatch sw;

    MM_UnloadAllDevices(mmc);
    MM_Close(mmc);
    mmc = nullptr;

    g_nikon_ti = nullptr;
    connected = false;
    LOG_INFO("NikonTi: disconnected in {:.3f}ms", stopwatch_ms(sw));
    emit propertyUpdated("", "Disconnected");
}

bool NikonTi::isModuleLoaded(const std::string module)
{
    return std::find(loadedModules.begin(),
                     loadedModules.end(),
                     module) != loadedModules.end();
}

std::vector<std::string> NikonTi::listDeviceProperty()
{
    if (!connected) {
        throw std::runtime_error("NikonTi: listDeviceProperty: device not connected");
    }

    return propertyList;
}

std::string NikonTi::getDevicePropertyDescription(const std::string name)
{
    auto kv = propInfoNikonTi.find(name);
    if (kv == propInfoNikonTi.end()) {
        throw std::invalid_argument("invalid property name");
    }
    auto& info = kv->second;
    return info.description;
}

std::string NikonTi::getDeviceProperty(const std::string name, bool force_update)
{
    if (name == "") {
        if (connected) {
            return "Connected";
        }
        return "Disconnected";
    }
    if (!connected) {
        throw std::runtime_error("NikonTi: getDeviceProperty: device not connected");
    }

    if (force_update == false) {
        auto it = propertyCache.find(name);
        if (it != propertyCache.end()) {
            auto prop = it->second;
            std::lock_guard<std::mutex> lk(prop->mu);
            if ((prop->current) && (prop->pendingSet == nullptr)) {
                return prop->current->value;
            }
        }
    }

    auto event = new PropertyEvent(PROP_EVENT_GET, name);
    utils::stopwatch sw;

    if (name == "ZDrivePosition") {
        if (!isModuleLoaded("TIZDrive")) {
            throw std::invalid_argument("invalid property name (module not loaded)");
        }
        std::string mmLabel = "TIZDrive";

        //
        // Call MM_GetPosition
        //
        double position;
        MM_Status status;

        slog::Fields log_fields;
        log_fields["api_call"] = "MM_GetPosition(TIZDrive)";
        utils::stopwatch sw_api_call;
        {
            std::lock_guard<std::mutex> lk(mu_mmc);
            sw_api_call.reset();
            status = MM_GetPosition(mmc, mmLabel.c_str(), &position);
        }
        log_fields["duration_ms"] = stopwatch_ms(sw_api_call);

        std::string value = fmt::format("{:.3f}", position);
        log_fields["response"] = value;
        
        if (status != 0) {
            log_fields["error_code"] = fmt::format("{}", status);
            LOGFIELDS_ERROR(log_fields, "MM_GetPosition failed");
            throw std::runtime_error("MM_GetPosition failed");
        } else {
            LOGFIELDS_TRACE(log_fields, "");
        }

        //
        // Log the event
        //
        event->completed(value);
        LOG_DEBUG("NikonTi: GetProperty: {}='{}'. {:.3f}ms", name, value, stopwatch_ms(sw));
        processPropertyEvent(event);

        return value;
    }

    auto kv = propInfoNikonTi.find(name);
    if (kv == propInfoNikonTi.end()) {
        throw std::invalid_argument("invalid property name");
    }
    auto& info = kv->second;
    if (!isModuleLoaded(info.mmLabel)) {
        throw std::invalid_argument("invalid property name");
    }

    //
    // Call MM_GetProperty
    //
    MM_Status status;
    char *mm_value_str;

    slog::Fields log_fields;
    log_fields["api_call"] = fmt::format("MM_GetProperty({}, {})", info.mmLabel, info.mmProperty);
    utils::stopwatch sw_api_call;
    {
        std::lock_guard<std::mutex> lk(mu_mmc);
        sw_api_call.reset();
        status = MM_GetProperty(mmc, info.mmLabel.c_str(), info.mmProperty.c_str(), &mm_value_str);
    }
    log_fields["duration_ms"] = stopwatch_ms(sw_api_call);

    std::string mmValue = std::string(mm_value_str);
    MM_StringFree(mm_value_str);

    log_fields["response"] = mmValue;

    if (status != 0) {
        log_fields["error_code"] = fmt::format("{}", status);
        LOGFIELDS_ERROR(log_fields, "MM_GetProperty failed");
        throw std::runtime_error("MM_GetProperty failed");
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }
    
    //
    // Convert value
    //
    std::string convertedValue;

    if (info.mmValueConverter) {
        convertedValue = info.mmValueConverter->valueFromMMValue(mmValue);
    } else {
        convertedValue = mmValue;
    }

    //
    // Logging and emit signals
    //
    event->completed(convertedValue);
    // LOG_DEBUG("NikonTi: GetProperty: {}='{}'. {:.3f}ms", name, convertedValue, stopwatch_ms(sw));
    processPropertyEvent(event);

    return convertedValue;
}

void NikonTi::setDeviceProperty(const std::string name, const std::string value)
{
    if (!connected) {
        throw std::runtime_error("NikonTi: setDeviceProperty: device not connected");
    }

    auto event = new PropertyEvent(PROP_EVENT_SET, name, value);
    utils::stopwatch sw;

    if (name == "ZDrivePosition") {
        if (!isModuleLoaded("TIZDrive")) {
            throw std::invalid_argument("invalid property name");
        }
        std::string mmLabel = "TIZDrive";

        //
        // Call MM_SetPosition
        //
        double position = std::stod(value);
        MM_Status status;

        slog::Fields log_fields;
        log_fields["api_call"] = "MM_SetPosition(TIZDrive)";
        log_fields["request"] = value;

        utils::stopwatch sw_api_call;
        {
            std::lock_guard<std::mutex> lk(mu_mmc);
            sw_api_call.reset();
            status = MM_SetPosition(mmc, mmLabel.c_str(), position);
        }
        log_fields["duration_ms"] = stopwatch_ms(sw_api_call);

        if (status != 0) {
            log_fields["error_code"] = fmt::format("{}", status);
            LOGFIELDS_ERROR(log_fields, "MM_SetPosition failed");
            throw std::runtime_error("MM_SetPosition failed");
        } else {
            LOGFIELDS_TRACE(log_fields, "");
        }

        //
        // Log the event
        //
        event->completed(value);
        LOG_DEBUG("NikonTi: MM_SetPosition: {}='{}'. {:.3f}ms", name, value, stopwatch_ms(sw));
        processPropertyEvent(event);

        return;
    }

    auto kv = propInfoNikonTi.find(name);
    if (kv == propInfoNikonTi.end()) {
        throw std::invalid_argument("invalid property name");
    }
    auto& info = kv->second;
    if (!isModuleLoaded(info.mmLabel)) {
        throw std::invalid_argument("invalid property name");
    }

    //
    // Convert value
    //
    std::string convertedValue;
    if (info.mmValueConverter) {
        convertedValue = info.mmValueConverter->valueToMMValue(value);
    } else {
        convertedValue = value;
    }

    //
    // Call MM_SetPropertyString
    //
    MM_Status status;

    slog::Fields log_fields;
    log_fields["api_call"] = fmt::format("MM_SetPropertyString({}, {})", info.mmLabel, info.mmProperty);
    log_fields["request"] = convertedValue;

    utils::stopwatch sw_api_call;
    {
        std::lock_guard<std::mutex> lk(mu_mmc);
        sw_api_call.reset();
        status = MM_SetPropertyString(mmc, info.mmLabel.c_str(), info.mmProperty.c_str(), convertedValue.c_str());
    }
    log_fields["duration_ms"] = stopwatch_ms(sw_api_call);

    if (status != 0) {
        log_fields["error_code"] = fmt::format("{}", status);
        LOGFIELDS_ERROR(log_fields, "MM_SetPropertyString failed");
        throw std::runtime_error("MM_SetProperty failed");
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }

    //
    // Logging
    //
    event->completed();
    // LOG_DEBUG("NikonTi: SetProperty: {}='{}'. {:.3f}ms", name, value, stopwatch_ms(sw));
    processPropertyEvent(event);

    // Setting DiaShutter is synchronous.
    // Get property immediately to validate and emit propertyUpdated signal.
    int n_attempt = 100;
    auto polling_start = std::chrono::steady_clock::now();
    auto polling_timeout = std::chrono::milliseconds(1000);
    for (int i_attempt = 0; i_attempt < n_attempt; i_attempt++) {
        if (name == "DiaShutter") {
            std::string value_get = getDeviceProperty(name);
            if (value_get == value) {
                break;
            }
            if ((i_attempt == n_attempt - 1) || (std::chrono::steady_clock::now() - polling_start > polling_timeout)) {
                LOG_ERROR("NikonTi: Set DiaShutter failed after {} attempts", i_attempt + 1);
                throw std::runtime_error("Set DiaShutter failed");
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    return;
}

void NikonTi::onPropertyChangedCallback(MM_Session mmc, const char* label, const char* property, const char* value) {
    if (mmc != this->mmc) {
        return;
    }

    slog::Fields log_fields;
    log_fields["api_call"] = fmt::format("onPropertyChanged({}, {})", label, property);
    log_fields["response"] = value;
    LOGFIELDS_TRACE(log_fields, "");

    for (const auto& kv : propInfoNikonTi) {
        std::string name = kv.first;
        auto& info = kv.second;

        if ((info.mmLabel == label) && (info.mmProperty == property)) {
            //
            // Convert value
            //
            std::string convertedValue;
            if (info.mmValueConverter) {
                convertedValue = info.mmValueConverter->valueFromMMValue(value);
            } else {
                convertedValue = value;
            }

            //
            // Logging and emit signals
            //
            auto event = new PropertyEvent(PROP_EVENT_UPDATED, name, convertedValue);
            LOG_DEBUG("NikonTi: PropertyUpdated: {}='{}'", name, convertedValue);
            processPropertyEvent(event);

            return;
        }
    }
}

void NikonTi::onStagePositionChangedCallback(MM_Session mmc, char* label, double pos) {
    if (mmc != this->mmc) {
        return;
    }

    slog::Fields log_fields;
    log_fields["api_call"] = fmt::format("onStagePositionChanged({})", label);
    log_fields["response"] = pos;
    LOGFIELDS_TRACE(log_fields, "");

    std::string label_str = std::string(label);

    std::string name;
    std::string value;

    if (label_str == "TIZDrive") {
        name = "ZDrivePosition";
        value = fmt::format("{:.3f}", pos);
    } else if (label_str == "TIPFSOffset") {
        name = "PFSOffset";
        value = fmt::format("{:.3f}", pos);
    } else {
        return;
    }

    // LOG_DEBUG("NikonTi: PropertyUpdated: {}='{}'", name, value);
    auto event = new PropertyEvent(PROP_EVENT_UPDATED, name, value);
    processPropertyEvent(event);
}

void NikonTi::processPropertyEvent(PropertyEvent* event)
{
    auto it = propertyCache.find(event->name);
    if (it == propertyCache.end()) {
        throw std::runtime_error("invalid property name: " + event->name);
    }
    PropertyStatus *prop = it->second;

    bool notifyNewValue = false;
    bool notifySetComplete = false;

    // Acquire lock of the property
    {
        std::lock_guard<std::mutex> lk(prop->mu);
        prop->eventLog.push_back(event);

//        while (prop->eventLog.size() > 10000) {
//            PropertyEvent *front = prop->eventLog.front();
//            if ((front != prop->pendingSet) && (front != prop->current)) {
//                delete front;
//            }
//            prop->eventLog.pop_front();
//        }

        if (event->type == PROP_EVENT_SET) {
            if ((prop->current) && (prop->current->value != event->value)) {
                prop->pendingSet = event;
            } else {
                // TODO: check value;
            }
        }

        if ((event->type == PROP_EVENT_GET) || (event->type == PROP_EVENT_UPDATED)) {
            PropertyEvent *previous = prop->current;
            prop->current = event;

            if ((previous == nullptr) || (previous->value != event->value)) {
                notifyNewValue = true;
            }

            if (prop->pendingSet != nullptr) {
                bool setComplete = (prop->pendingSet->value == event->value);
                if (event->name == "ZDrivePosition") {
                    double valueSet = stod(prop->pendingSet->value);
                    double valueCurrent = stod(event->value);
                    double tolerance = 0.1; // when tol is set to 9
                    double diff = std::abs(valueSet - valueCurrent);
                    setComplete = diff < (tolerance + 0.001);
                    LOG_DEBUG("ZDrivePosition update: set={}, update={}, diff={}, setComplete={}", valueSet, valueCurrent, diff, setComplete);
                }
                notifySetComplete = setComplete;

                if (setComplete) {
                    auto duration = event->tEnd - prop->pendingSet->tStart;
                    if (previous != nullptr) {
                        LOG_DEBUG("NikonTi: {} set from {} to {} in {:.3f}ms",
                                     event->name, previous->value, event->value,
                                     std::chrono::duration<double>(duration).count() * 1000);
                    } else {
                        LOG_DEBUG("NikonTi: {} set from ? to {} in {:.3f}ms",
                                     event->name, event->value,
                                     std::chrono::duration<double>(duration).count() * 1000);
                    }
                    prop->pendingSet = nullptr;
                }
            }
        }
    }

    // Notify after releasing lock
    if (notifyNewValue) {
        emit propertyUpdated(event->name, event->value);
    }
    if (notifySetComplete) {
        prop->setCompleted.notify_all();
    }
}

bool NikonTi::waitDeviceProperty(const std::vector<std::string> nameList, const std::chrono::milliseconds timeout)
{
    std::vector<PropertyStatus *> propList;
    propList.reserve(nameList.size());

    for (const auto& name : nameList) {
        auto it = propertyCache.find(name);
        if (it == propertyCache.end()) {
            throw std::runtime_error("invalid property name: " + name);
        }

        propList.push_back(it->second);
    }

    auto t = std::chrono::steady_clock::now();
    bool completed = true;
    for (auto& prop : propList) {
        std::unique_lock<std::mutex> lk(prop->mu);
        completed |= prop->setCompleted.wait_until(lk, t + timeout, [prop]{
            return (prop->pendingSet == nullptr);
        });
    }
    return completed;
}

std::unordered_map<std::string, std::string> NikonTi::getCachedDeviceProperty()
{
    std::unordered_map<std::string, std::string> cachedProperty;
    for (const auto& kv : propertyCache) {
        std::string name = kv.first;
        PropertyStatus *prop = kv.second;

        std::lock_guard<std::mutex> lk(prop->mu);
        if ((prop->current) && (prop->pendingSet == nullptr)) {
            cachedProperty[name] = prop->current->value;
        }
    }
    return cachedProperty;
}
