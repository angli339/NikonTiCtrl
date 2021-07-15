#include "prior_proscan.h"
#include "prior_proscan_property.h"

#include <stdexcept>
#include <unordered_map>

#include "logging.h"
#include "utils/wmi.h"

#include "visa.h"

PriorProscan::PriorProscan(std::string name, QObject *parent) : QObject(parent)
{
    ViStatus status;
    status = viOpenDefaultRM(&rm);
    if (status != VI_SUCCESS) {
        throw std::runtime_error("viOpenDefaultRM: Error" + std::to_string(status));
    }

    portName = name;
}

PriorProscan::~PriorProscan()
{
    if (connected) {
        disconnect();
    }
    if (rm != 0) {
        viClose(rm);
        rm = 0;
    }
}

bool PriorProscan::detectDevice()
{
    WMI w;
    std::vector<std::string> usbList;

    try {
        usbList = w.listUSBDeviceID("10DB", "1234");
    } catch (std::runtime_error &e) {
        SPDLOG_DEBUG("NikonTi::detectDevice failed: %s", e.what());
        throw e;
    }

    if (usbList.size() > 0) {
        SPDLOG_DEBUG("PriorProscan::detected {}", usbList[0]);
    }
    return usbList.size() != 0;
}

void PriorProscan::connect()
{
    if (!detectDevice()) {
        SPDLOG_WARN("PriorProscan: USB connection not detected...");
        emit propertyUpdated("", "Disconnected");
        return;
    }

    SPDLOG_INFO("PriorProscan: connecting...");
    emit propertyUpdated("", "Connecting");
    spdlog::stopwatch sw;

    ViStatus status;

    status = viOpen(rm, portName.c_str(), VI_EXCLUSIVE_LOCK, 50, &dev);
    log_io("PriorProscan", "connect", portName,
        "viOpen(VI_EXCLUSIVE_LOCK, 50)", "", "",
        fmt::format("ViStatus {:#10x}", uint32_t(status))
    );
    if (status != VI_SUCCESS) {
        throw std::runtime_error(fmt::format("viOpen: Error {:#10x}", uint32_t(status)));
    }
    SPDLOG_INFO("PriorProscan: port {} opened", portName);

    status = viSetAttribute(dev, VI_ATTR_ASRL_BAUD, 38400);
    log_io("PriorProscan", "connect", portName,
            "viSetAttribute(VI_ATTR_ASRL_BAUD, 38400)", "", "",
            fmt::format("ViStatus {:#10x}", uint32_t(status))
    );
    if (status != VI_SUCCESS) {
        throw std::runtime_error(fmt::format("viSetAttribute VI_ATTR_ASRL_BAUD: Error {:#10x}", uint32_t(status)));
    }

    status = viSetAttribute(dev, VI_ATTR_TERMCHAR, '\r');
    if (status != VI_SUCCESS) {
        throw std::runtime_error(fmt::format("viSetAttribute VI_ATTR_TERMCHAR: Error {:#10x}", uint32_t(status)));
    }
    status = viSetAttribute(dev, VI_ATTR_TMO_VALUE, 50);
    if (status != VI_SUCCESS) {
        throw std::runtime_error(fmt::format("viSetAttribute VI_ATTR_TMO_VALUE: Error {:#10x}", uint32_t(status)));
    }

    try {
        if (!checkCommunication(1)) {
            switchBaudrate();
        }
    } catch (std::runtime_error& e) {
        SPDLOG_ERROR("failed to connect: {}", e.what());
        throw std::runtime_error(fmt::format("failed to connect: {}", e.what()));
    }
    connected = true;
    SPDLOG_INFO("PriorProscan: communication established at 38400. {:.3f}ms", stopwatch_ms(sw));

    //
    // Init property cache
    //
    auto propertyList = listDeviceProperty();
    for (const auto& propName : propertyList) {
        propertyCache.insert({propName, new PropertyStatus});
    }

    //
    // Init properties
    //
    std::vector<std::pair<std::string, std::string>> propertyToInit= {
        {"CommandProtocol", "0"},
        {"XYResolution", "0.1"},
    };

    for (const auto& [name, value] : propertyToInit) {
        std::string getResp = getDeviceProperty(name);
        SPDLOG_INFO("ProScan: [Init] Get {}='{}'", name, getResp);
        if (getResp == value) {
            continue;
        }
        std::string getValue;
        try {
            getValue = setGetDeviceProperty(name, value);
        } catch (std::exception& e) {
            SPDLOG_ERROR("PriorProScan: [Init] setGetDeviceProperty: {}", e.what());
            emit propertyUpdated("", "Error");
            return;
        }
        if (getValue != value) {
            SPDLOG_ERROR("PriorProScan: [Init] Set {}={}, got {}", name, value, getValue);
            emit propertyUpdated("", "Error");
            return;
        }
        SPDLOG_INFO("ProScan: [Init] Set {}='{}'", name, value);
    }

    //
    // Enumerate properties to fill the cache 
    //
    for (const auto& [name, info] : propInfoProScan) {
        std::string getResp;
        if (info.getCommand == "") {
            if ((name == "LumenOutputIntensity") || (name == "XYPosition")) {
                getResp = getDeviceProperty(name);
                SPDLOG_INFO("ProScan: [Init] {}='{}'", name, getResp);
            }
            continue;
        }
        if ((name == "CommandProtocol") || (name == "XYResolution")) {
            continue;
        }
        getResp = getDeviceProperty(name);
        SPDLOG_INFO("ProScan: [Init] {}='{}'", name, getResp);
    }

    SPDLOG_INFO("PriorProscan: init finished. connected. {:.3f}ms", stopwatch_ms(sw));
    emit propertyUpdated("", "Connected");

    //
    // Start polling
    //
    polling = true;
    threadPolling = std::thread([this]{
        while (polling) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            try {
                // Update volatile properties
                getDeviceProperty("XYPosition", true, "pollingThread");
                getDeviceProperty("MotionStatus", true, "pollingThread");
            } catch (std::exception& e) {
                polling = false;
                SPDLOG_ERROR("Error in polling: {}", e.what());
                return;
            }
        }
    });
    SPDLOG_INFO("PriorProscan: Polling started.");
}

void PriorProscan::disconnect()
{
    if (dev == 0) {
        return;
    }

    SPDLOG_INFO("PriorProscan: disconnecting...");
    emit propertyUpdated("", "Disconnecting");
    spdlog::stopwatch sw;

    if (polling) {
        polling = false;
        threadPolling.join();
        SPDLOG_INFO("PriorProscan: Polling stopped.");
    }

    viClose(dev);
    dev = 0;
    connected = false;

    SPDLOG_INFO("PriorProscan: disconnected in {:.3f}ms", stopwatch_ms(sw));
    emit propertyUpdated("", "Disconnected");
}

bool PriorProscan::checkCommunication(int n_retry)
{
    try {
        query("");
    } catch (std::runtime_error& e) {
        if (n_retry == 0) {
            return false;
        } else {
            return checkCommunication(n_retry - 1);
        }
    }
    return true;
}

void PriorProscan::switchBaudrate()
{
    //
    // Check communication at 9600
    //
    ViStatus status;
    status = viSetAttribute(dev, VI_ATTR_ASRL_BAUD, 9600);
    if (status != VI_SUCCESS) {
        throw std::runtime_error(fmt::format("viSetAttribute VI_ATTR_ASRL_BAUD: Error {:#10x}", uint32_t(status)));
    }
    if (!checkCommunication(2)) {
        throw std::runtime_error("communication cannot be established at 9600 either");
    }
    SPDLOG_INFO("PriorProscan: communication established at 9600. switching to 38400");

    // Set ProScan to 38400
    write("BAUD,38");
    status = viFlush(dev, VI_WRITE_BUF);
    if (status != VI_SUCCESS) {
        throw std::runtime_error(fmt::format("viFlush VI_WRITE_BUF: Error {:#10x}", uint32_t(status)));
    }

    // Add some delay for ProScan to finish the work
    // Otherwise it does not respone to the commands
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Set serial port to 38400
    status = viSetAttribute(dev, VI_ATTR_ASRL_BAUD, 38400);
    if (status != VI_SUCCESS) {
        throw std::runtime_error(fmt::format("viSetAttribute VI_ATTR_ASRL_BAUD: Error {:#10x}", uint32_t(status)));
    }

    // Check communication at 38400
    if (checkCommunication(1)) {
        return;
    } else {
        throw std::runtime_error("failed to switch baudrate from 9600 to 38400");
    }
}

uint32_t PriorProscan::clearReadBuffer()
{
    char buf[4096];
    ViStatus status;
    ViUInt32 count;

    viGetAttribute(dev, VI_ATTR_ASRL_AVAIL_NUM, &count);
    if (count > 0) {
        status = viRead(dev, (uint8_t *)buf, count,  &count);
        log_io("PriorProscan", "clearReadBuffer", portName,
            fmt::format("viRead({})", count), "", std::string(buf, count),
            fmt::format("ViStatus {:#10x}", uint32_t(status))
        );
        if (status < VI_SUCCESS) {
            throw std::runtime_error(fmt::format("failed to remove unexpected data from buffer. Err={:#10X}", uint32_t(status)));
        }
    }

    return count;
}

void PriorProscan::write(const std::string command, std::string caller)
{
    ViStatus status;
    ViUInt32 count;

    std::string cmdWrite = command + "\r";
    status = viWrite(dev, (const uint8_t *)cmdWrite.c_str(), cmdWrite.size(), &count);
    log_io("PriorProscan", "write", portName,
        "viWrite()", command, "",
        fmt::format("ViStatus {:#10x}", uint32_t(status))
    );
    if (status < VI_SUCCESS) {
        throw std::runtime_error(fmt::format("failed to send command. Err={:#10X}", uint32_t(status)));
    }
}

std::string PriorProscan::readline(std::string caller)
{
    ViStatus status;
    ViUInt32 count;

    char buf[4096];
    status = viRead(dev, (uint8_t *)buf, sizeof(buf),  &count);
    if (caller != "pollingThread") {
        log_io("PriorProscan", "readline", portName,
            "viRead(4096)", "", std::string(buf, count),
            fmt::format("ViStatus {:#10x}", uint32_t(status))
        );
    }
    if (status < VI_SUCCESS) {
        throw std::runtime_error(fmt::format("failed to read response. Err={:#10X}", uint32_t(status)));
    }
    if (buf[count-1] != '\r') {
        throw std::runtime_error(fmt::format("unexpected response: {} bytes, not terminated by \\r", count));
    }
    std::string resp = std::string(buf, count-1);

    return resp;
}

std::string PriorProscan::query(const std::string command, std::string caller)
{
    ViStatus status;
    ViUInt32 count;

    count = clearReadBuffer();
    if (count > 0) {
        SPDLOG_WARN("query: discarded {} bytes of unexpected data before sending command", count);
    }

    //
    // Send command
    //
    std::string cmdWrite = command + "\r";
    status = viWrite(dev, (const uint8_t *)cmdWrite.c_str(), cmdWrite.size(), &count);
    if (caller != "pollingThread") {
        log_io("PriorProscan", "query", portName,
            "viWrite()", cmdWrite, "",
            fmt::format("ViStatus {:#10x}", uint32_t(status))
        );
    }
    if (status < VI_SUCCESS) {
        throw std::runtime_error(fmt::format("failed to send command. Err={:#10X}", uint32_t(status)));
    }

    //
    // Read line
    //
    std::string resp = readline(caller);

    //
    // Check for ProScan Error Code
    //
    if ((resp.size() >= 2) && (resp.substr(0, 2) == "E,")) {
        auto it = errorCodeProScan.find(resp);
        if (it != errorCodeProScan.end()) {
            std::string errMsg = it->second;
            throw std::runtime_error(fmt::format("proscan error {}: {}", resp, errMsg));
        } else {
            throw std::runtime_error(fmt::format("proscan error {}", resp));
        }
    }

    return resp;
}

std::vector<std::string> PriorProscan::listDeviceProperty()
{
    std::vector<std::string> propertyList;
    propertyList.reserve(propInfoProScan.size());

    for (const auto& kv : propInfoProScan) {
        auto& name = kv.first;
        propertyList.push_back(name);

        // Insert XYPosition after RawXYPosition
        if (name == "RawXYPosition") {
            propertyList.push_back("XYPosition");
        }
    }

    return propertyList;
}

std::string PriorProscan::getDeviceProperty(const std::string name, bool force_update, std::string caller)
{
    if (name == "") {
        if (connected) {
            return "Connected";
        }
        return "Disconnected";
    }

    if (!connected) {
        throw std::runtime_error("PriorProscan: getDeviceProperty: device not connected");
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

    if (name == "XYPosition") {
        std::string raw_xy_position = getDeviceProperty("RawXYPosition", true, caller);
        std::string xy_position = xyPositionFromRaw(raw_xy_position);

        event->completed(xy_position);
        processPropertyEvent(event);

        return xy_position;
    }

    if (name == "LumenOutputIntensity") {
        std::string value;
        {
            std::lock_guard<std::mutex> lk(mu);
            value = fmt::format("{:d}", lumenOutputIntensity);
        }
        event->completed(value);
        processPropertyEvent(event);
        return value;
    }

    auto kv = propInfoProScan.find(name);
    if (kv == propInfoProScan.end()) {
        throw std::invalid_argument("invalid name");
    }
    auto& info = kv->second;

    if (info.getCommand == "") {
        throw std::invalid_argument("property not readable");
    }

    std::string resp;
    std::lock_guard<std::mutex> lk(mu);
    try {
        resp = query(info.getCommand, caller);
    } catch (std::runtime_error& e) {
        throw std::runtime_error(fmt::format("getDeviceProperty failed: ", e.what()));
    }

    if (name == "LumenShutter") {
        if (resp == "0") {
            resp = "Off";
        } else {
            // TODO: figure out a better way to handle lumen intensity
            lumenOutputIntensity = stoi(resp);
            resp = "On";
        }
    }

    event->completed(resp);
    processPropertyEvent(event);

    return resp;
}

void PriorProscan::setDeviceProperty(const std::string name, const std::string value, std::string caller)
{
    if (!connected) {
        throw std::runtime_error("PriorProscan: setDeviceProperty: device not connected");
    }

    auto event = new PropertyEvent(PROP_EVENT_SET, name, value);

    if (name == "XYPosition") {
        std::string raw_xy_position = xyPositionToRaw(value);
        setDeviceProperty("RawXYPosition", raw_xy_position, caller);

        event->completed();
        processPropertyEvent(event);
        SPDLOG_DEBUG("PriorProscan: set {}='{}'", name, value);

        return;
    }

    if (name == "LumenOutputIntensity") {
        uint8_t value_int = std::stoi(value);
        if ((value_int < 1) || (value_int > 100)) {
            throw std::invalid_argument(fmt::format("{} cannot be set to {}, expecting [1, 100]", name, value));
        }

        {
            std::lock_guard<std::mutex> lk(mu);
            lumenOutputIntensity = value_int;
        }
        event->completed(value);
        processPropertyEvent(event);
        
        SPDLOG_DEBUG("PriorProscan: set {}='{}'", name, value);

        return;
    }

    auto kv = propInfoProScan.find(name);
    if (kv == propInfoProScan.end()) {
        throw std::invalid_argument("invalid name");
    }
    auto& info = kv->second;

    if (info.setCommand == "") {
        throw std::invalid_argument("property not wriable");
    }

    // if ((name == "XYResolution") || (name == "COMP") || (name == "Baudrate")) {
    //     throw std::invalid_argument(fmt::format("property {} cannot be set from external program", name));
    // }

    std::string commandValue = value;
    if (name == "LumenShutter") {
        if (value == "On") {
            commandValue = fmt::format("{:d}", lumenOutputIntensity);
        } else if (value == "Off") {
            commandValue = "0";
        } else {
            throw std::invalid_argument(fmt::format("invalid value '{}', expecting ['On', 'Off']", value));
        }
    }

    std::lock_guard<std::mutex> lk(mu);

    if (info.setResponse == "") {
        write(fmt::format(info.setCommand, commandValue), caller);
    } else {
        std::string resp = query(fmt::format(info.setCommand, commandValue), caller);
        if (info.setResponse != resp) {
            throw std::runtime_error(fmt::format("unexpected response: '{}'", resp));
        }
    }

    event->completed();
    processPropertyEvent(event);
    SPDLOG_DEBUG("PriorProscan: set {}='{}'", name, value);

    return;
}

std::string PriorProscan::setGetDeviceProperty(const std::string name, const std::string value, std::string caller)
{
    if (!connected) {
        throw std::runtime_error("PriorProscan: setGetDeviceProperty: device not connected");
    }

    auto eventSet = new PropertyEvent(PROP_EVENT_SET, name, value);

    if ((name == "XYPosition") || (name == "FilterWheel1") || (name == "FilterWheel3") || (name == "LumenShutter")) {
        throw std::invalid_argument(fmt::format("cannot setGetDeviceProperty on motion property {}", name));
    }

    if (name == "LumenOutputIntensity") {
        uint8_t value_int = std::stoi(value);
        if ((value_int < 1) || (value_int > 100)) {
            throw std::invalid_argument(fmt::format("{} cannot be set to {}, expecting [1, 100]", name, value));
        }

        {
            std::lock_guard<std::mutex> lk(mu);
            lumenOutputIntensity = value_int;
        }
        eventSet->completed(value);
        processPropertyEvent(eventSet);

        return value;
    }

    //
    // Find PropInfo
    //
    auto kv = propInfoProScan.find(name);
    if (kv == propInfoProScan.end()) {
        throw std::invalid_argument("invalid name");
    }
    auto& info = kv->second;

    if (info.setCommand == "") {
        throw std::invalid_argument("property not wriable");
    }
    if (info.getCommand == "") {
        throw std::invalid_argument("property not readable");
    }

    // if ((name == "XYResolution") || (name == "CommandProtocol") || (name == "RawXYPosition") || (name == "Baudrate")) {
    //     throw std::invalid_argument(fmt::format("property {} cannot be set from external program", name));
    // }


    //
    // Lock for Set and Get
    //
    std::lock_guard<std::mutex> lk(mu);

    //
    // Set
    //
    std::string setValue = value;

    if (info.setResponse == "") {
        write(fmt::format(info.setCommand, setValue), caller);
    } else {
        std::string setResp = query(fmt::format(info.setCommand, setValue), caller);
        if (info.setResponse != setResp) {
            throw std::runtime_error(fmt::format("unexpected response during set: '{}'", setResp));
        }
    }

    eventSet->completed();
    processPropertyEvent(eventSet);

    //
    // Get
    //
    auto eventGet = new PropertyEvent(PROP_EVENT_GET, name);
    std::string getResp;
    try {
        getResp = query(info.getCommand, caller);
    } catch (std::runtime_error& e) {
        throw std::runtime_error(fmt::format("getDeviceProperty failed: ", e.what()));
    }
    eventGet->completed(getResp);
    processPropertyEvent(eventGet);

    return getResp;
}

std::string PriorProscan::xyPositionFromRaw(const std::string raw_xy_position)
{
    int x_raw, y_raw;
    try {
        auto n = raw_xy_position.find(',');
        x_raw = stoi(raw_xy_position.substr(0, n));
        y_raw = stoi(raw_xy_position.substr(n + 1));
    } catch (...) {
        throw std::runtime_error(fmt::format("invalid value: '{}'", raw_xy_position));
    }

    double x =  xyResolution * (double)(x_raw);
    double y = xyResolution * (double)(y_raw);
    std::string xy_position = fmt::format("{:.1f},{:.1f}", x, y);
    return xy_position;
}

std::string PriorProscan::xyPositionToRaw(const std::string xy_position)
{
    double x, y;
    try {
        auto n = xy_position.find(',');
        x = stod(xy_position.substr(0, n));
        y = stod(xy_position.substr(n + 1));
    } catch (...) {
        throw std::runtime_error(fmt::format("invalid value: '{}'", xy_position));
    }

    int x_raw = (int)(x / xyResolution);
    int y_raw = (int)(y / xyResolution);
    std::string raw_xy_position = fmt::format("{},{}", x_raw, y_raw);
    return raw_xy_position;
}

void PriorProscan::processPropertyEvent(PropertyEvent* event)
{
    auto it = propertyCache.find(event->name);
    if (it == propertyCache.end()) {
        throw std::runtime_error("invalid property name: " + event->name);
    }
    PropertyStatus *prop = it->second;

    bool notifyNewValue = false;
    bool notifySetComplete_S = false;
    bool notifySetComplete_LumenIntensity = false;
    std::string notifySetComplete_F1 = "";
    std::string notifySetComplete_F2 = "";
    std::string notifySetComplete_F3 = "";

    {
        std::lock_guard<std::mutex> lk(prop->mu);
        prop->eventLog.push_back(event);

        if (event->type == PROP_EVENT_SET) {
            prop->pendingSet = event;

            if (event->name == "LumenOutputIntensity") {
                notifySetComplete_LumenIntensity = true;
                prop->pendingSet = nullptr;
                prop->current = event;
            }
        }

        if (event->type == PROP_EVENT_GET) {
            PropertyEvent *previous = prop->current;
            prop->current = event;

            if ((previous == nullptr) || (previous->value != event->value)) {
                notifyNewValue = true;
            }
        }
    }

    if (event->name == "MotionStatus") {
        uint8_t motionStatus = std::stoi(event->value);

        // 5  4  3  2  1  0
        // F2 F1 F3 ?  Y  X
        const uint8_t MOTIONSTATUS_XY_MASK = 0b11;
        const uint8_t MOTIONSTATUS_F3 = 1 << 3;
        const uint8_t MOTIONSTATUS_F1 = 1 << 4;
        const uint8_t MOTIONSTATUS_F2 = 1 << 5;

        {
            prop = propertyCache["XYPosition"];
            std::lock_guard<std::mutex> lk(prop->mu);
            if ((prop->pendingSet != nullptr) && ((motionStatus & MOTIONSTATUS_XY_MASK) == 0)) {
                notifySetComplete_S = true;
                prop->pendingSet = nullptr;

                auto duration = std::chrono::steady_clock::now() - prop->current->tStart;
                SPDLOG_DEBUG("Proscan: set {} to {} motion completed in {:.3f}ms",
                             prop->current->name, prop->current->value,
                             std::chrono::duration<double>(duration).count() * 1000);
            }
        }

        {
            prop = propertyCache["FilterWheel3"];
            std::lock_guard<std::mutex> lk(prop->mu);
            if ((prop->pendingSet != nullptr) && ((motionStatus & MOTIONSTATUS_F3) == 0)) {
                notifySetComplete_F3 = prop->pendingSet->value;
                prop->current = prop->pendingSet;
                prop->pendingSet = nullptr;

                auto duration = std::chrono::steady_clock::now() - prop->current->tStart;
                SPDLOG_DEBUG("Proscan: set {} to {} motion completed in {:.3f}ms",
                             prop->current->name, prop->current->value,
                             std::chrono::duration<double>(duration).count() * 1000);
            }
        }

        {
            prop = propertyCache["FilterWheel1"];
            std::lock_guard<std::mutex> lk(prop->mu);
            if ((prop->pendingSet != nullptr) && ((motionStatus & MOTIONSTATUS_F1) == 0)) {
                notifySetComplete_F1 = prop->pendingSet->value;
                prop->current = prop->pendingSet;
                prop->pendingSet = nullptr;

                auto duration = std::chrono::steady_clock::now() - prop->current->tStart;
                SPDLOG_DEBUG("Proscan: set {} to {} motion completed in {:.3f}ms",
                             prop->current->name, prop->current->value,
                             std::chrono::duration<double>(duration).count() * 1000);
            }
        }

        {
            prop = propertyCache["LumenShutter"];
            std::lock_guard<std::mutex> lk(prop->mu);
            if ((prop->pendingSet != nullptr) && ((motionStatus & MOTIONSTATUS_F2) == 0)) {
                notifySetComplete_F2 = prop->pendingSet->value;
                prop->current = prop->pendingSet;
                prop->pendingSet = nullptr;

                auto duration = std::chrono::steady_clock::now() - prop->current->tStart;
                SPDLOG_DEBUG("Proscan: set {} to {} motion completed in {:.3f}ms",
                             prop->current->name, prop->current->value,
                             std::chrono::duration<double>(duration).count() * 1000);
            }
        }
    }

    // Notify after releasing lock
    if (notifyNewValue) {
        emit propertyUpdated(event->name, event->value);
    }

    if (notifySetComplete_LumenIntensity) {
        propertyCache["LumenOutputIntensity"]->setCompleted.notify_all();
        emit propertyUpdated(event->name, event->value);
    }

    if (notifySetComplete_S) {
        propertyCache["XYPosition"]->setCompleted.notify_all();
    }
    if (notifySetComplete_F1 != "") {
        propertyCache["FilterWheel1"]->setCompleted.notify_all();
        emit propertyUpdated("FilterWheel1", notifySetComplete_F1);
    }
    if (notifySetComplete_F2 != "") {
        propertyCache["LumenShutter"]->setCompleted.notify_all();
        emit propertyUpdated("LumenShutter", notifySetComplete_F2);
    }
    if (notifySetComplete_F3 != "") {
        propertyCache["FilterWheel3"]->setCompleted.notify_all();
        emit propertyUpdated("FilterWheel3", notifySetComplete_F3);
    }
}

bool PriorProscan::waitDeviceProperty(const std::vector<std::string> nameList, const std::chrono::milliseconds timeout)
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

std::unordered_map<std::string, std::string> PriorProscan::getCachedDeviceProperty()
{
    std::unordered_map<std::string, std::string> cachedProperty;
    for (const auto& kv : propertyCache) {
        std::string name = kv.first;
        PropertyStatus *prop = kv.second;


        //
        // Return unless a property is in itermediate state
        // (e.g. when filter wheel or shutter is moving)
        //
        std::lock_guard<std::mutex> lk(prop->mu);
        if (prop->current) {
            if (name == "XYPosition") {
                // XYPosition is updated by polling, okay to return even when still moving
                cachedProperty[name] = prop->current->value;
                continue;
            }
            if (prop->pendingSet == nullptr) {
                // Property is set and validated
                // Or motion is completed
                cachedProperty[name] = prop->current->value;
                continue;
            } else {
                // Property is set but not yet validated
                // and not a volatile (motion related) property
                auto info = propInfoProScan[name];
                if (info.isVolatile == false) {
                    cachedProperty[name] = prop->pendingSet->value;
                    continue;
                }
            }
        }
    }
    return cachedProperty;
}
