#include "trigger_controller.h"

#include <stdexcept>

#include "logging.h"

#include "visa.h"

TriggerController::TriggerController(std::string name, QObject *parent) : QObject(parent)
{
    ViStatus status;
    status = viOpenDefaultRM(&rm);
    if (status != VI_SUCCESS) {
        throw std::runtime_error("viOpenDefaultRM: Error" + std::to_string(status));
    }

    portName = name;
}

TriggerController::~TriggerController()
{
    if (connected) {
        disconnect();
    }
    if (rm != 0) {
        viClose(rm);
        rm = 0;
    }
}

void TriggerController::connect()
{
    SPDLOG_INFO("TriggerController: connecting...");
    emit propertyUpdated("", "Connecting");
    spdlog::stopwatch sw;

    ViStatus status;

    status = viOpen(rm, portName.c_str(), VI_EXCLUSIVE_LOCK, 50, &dev);
    log_io("TriggerController", "connect", portName,
        "viOpen(VI_EXCLUSIVE_LOCK, 50)", "", "",
        fmt::format("ViStatus {:#10x}", uint32_t(status))
    );
    if (status != VI_SUCCESS) {
        throw std::runtime_error(fmt::format("viOpen: Error {:#10x}", uint32_t(status)));
    }
    SPDLOG_INFO("TriggerController: port {} opened", portName);

    status = viSetAttribute(dev, VI_ATTR_ASRL_BAUD, 115200);
    log_io("TriggerController", "connect", portName,
            "viSetAttribute(VI_ATTR_ASRL_BAUD, 115200)", "", "",
            fmt::format("ViStatus {:#10x}", uint32_t(status))
    );
    if (status != VI_SUCCESS) {
        throw std::runtime_error(fmt::format("viSetAttribute VI_ATTR_ASRL_BAUD: Error {:#10x}", uint32_t(status)));
    }

    status = viSetAttribute(dev, VI_ATTR_TERMCHAR, '\n');
    if (status != VI_SUCCESS) {
        throw std::runtime_error(fmt::format("viSetAttribute VI_ATTR_TERMCHAR: Error {:#10x}", uint32_t(status)));
    }
    status = viSetAttribute(dev, VI_ATTR_TMO_VALUE, 50);
    if (status != VI_SUCCESS) {
        throw std::runtime_error(fmt::format("viSetAttribute VI_ATTR_TMO_VALUE: Error {:#10x}", uint32_t(status)));
    }

    std::string idn;
    try {
        idn = query("*IDN?");
    } catch (std::runtime_error& e) {
        SPDLOG_ERROR("failed to connect: {}", e.what());
        throw std::runtime_error(fmt::format("failed to connect: {}", e.what()));
    }
    connected = true;

    SPDLOG_INFO("TriggerController: connected ({}). {:.3f}ms", idn, stopwatch_ms(sw));
    emit propertyUpdated("", "Connected");
}

void TriggerController::disconnect()
{
    if (dev == 0) {
        return;
    }

    SPDLOG_INFO("TriggerController: disconnecting...");
    emit propertyUpdated("", "Disconnecting");
    spdlog::stopwatch sw;

    viClose(dev);
    dev = 0;
    connected = false;

    SPDLOG_INFO("TriggerController: disconnected in {:.3f}ms", stopwatch_ms(sw));
    emit propertyUpdated("", "Disconnected");
}


uint32_t TriggerController::clearReadBuffer()
{
    char buf[4096];
    ViStatus status;
    ViUInt32 count;

    viGetAttribute(dev, VI_ATTR_ASRL_AVAIL_NUM, &count);
    if (count > 0) {
        status = viRead(dev, (uint8_t *)buf, count,  &count);
        log_io("TriggerController", "clearReadBuffer", portName,
            fmt::format("viRead({})", count), "", std::string(buf, count),
            fmt::format("ViStatus {:#10x}", uint32_t(status))
        );
        if (status < VI_SUCCESS) {
            throw std::runtime_error(fmt::format("failed to remove unexpected data from buffer. Err={:#10X}", uint32_t(status)));
        }
    }

    return count;
}

void TriggerController::write(const std::string command, std::string caller)
{
    ViStatus status;
    ViUInt32 count;

    std::string cmdWrite = command + "\n";
    status = viWrite(dev, (const uint8_t *)cmdWrite.c_str(), cmdWrite.size(), &count);
    log_io("TriggerController", "write", portName,
        "viWrite()", command, "",
        fmt::format("ViStatus {:#10x}", uint32_t(status))
    );
    if (status < VI_SUCCESS) {
        throw std::runtime_error(fmt::format("failed to send command. Err={:#10X}", uint32_t(status)));
    }
}

std::string TriggerController::readline(std::string caller)
{
    ViStatus status;
    ViUInt32 count;

    char buf[4096];
    status = viRead(dev, (uint8_t *)buf, sizeof(buf),  &count);
    log_io("TriggerController", "readline", portName,
        "viRead(4096)", "", std::string(buf, count),
        fmt::format("ViStatus {:#10x}", uint32_t(status))
    );
    if (status < VI_SUCCESS) {
        throw std::runtime_error(fmt::format("failed to read response. Err={:#10X}", uint32_t(status)));
    }
    if ((count<2) ||(buf[count-1] != '\n') || (buf[count-2] != '\r')) {
        throw std::runtime_error(fmt::format("unexpected response: {} bytes, not terminated by \\r", count));
    }
    std::string resp = std::string(buf, count-2);

    return resp;
}

std::string TriggerController::query(const std::string command, std::string caller)
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
    std::string cmdWrite = command + "\n";
    status = viWrite(dev, (const uint8_t *)cmdWrite.c_str(), cmdWrite.size(), &count);
    log_io("TriggerController", "query", portName,
        "viWrite()", cmdWrite, "",
        fmt::format("ViStatus {:#10x}", uint32_t(status))
    );

    if (status < VI_SUCCESS) {
        throw std::runtime_error(fmt::format("failed to send command. Err={:#10X}", uint32_t(status)));
    }

    //
    // Read line
    //
    std::string resp = readline(caller);

    return resp;
}

void TriggerController::outputTriggerPulse()
{
    write("CAMERA:EXT_TRG:OUTPUT");
}

void TriggerController::setTriggerPulseWidth(double duration_ms)
{
    write(fmt::format("CAMERA:EXT_TRG:PULSE_WIDTH {}", duration_ms));
}

double TriggerController::getTriggerPulseWidth()
{
    return std::stod(query("CAMERA:EXT_TRG:PULSE_WIDTH"));
}