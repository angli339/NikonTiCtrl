#include "trigger_controller.h"

#include <stdexcept>

#include "logger.h"

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
    LOG_INFO("TriggerController: connecting...");
    emit propertyUpdated("", "Connecting");
    utils::StopWatch sw;

    ViStatus status;

    slog::Fields log_fields;
    log_fields["device"] = portName;
    log_fields["api_call"] = "viOpen(VI_EXCLUSIVE_LOCK, 50)";

    utils::StopWatch sw_api_call;
    status = viOpen(rm, portName.c_str(), VI_EXCLUSIVE_LOCK, 50, &dev);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    if (status != VI_SUCCESS) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(status));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("viOpen: Error {:#10x}", uint32_t(status)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }
    LOG_INFO("TriggerController: port {} opened", portName);

    status = viSetAttribute(dev, VI_ATTR_ASRL_BAUD, 115200);
    log_fields["api_call"] = "viSetAttribute(VI_ATTR_ASRL_BAUD, 115200)";

    if (status != VI_SUCCESS) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(status));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("viSetAttribute VI_ATTR_ASRL_BAUD: Error {:#10x}", uint32_t(status)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
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
        LOG_ERROR("failed to connect: {}", e.what());
        throw std::runtime_error(fmt::format("failed to connect: {}", e.what()));
    }
    connected = true;

    LOG_INFO("TriggerController: connected ({}). {:.3f}ms", idn, sw.Milliseconds());
    emit propertyUpdated("", "Connected");
}

void TriggerController::disconnect()
{
    if (dev == 0) {
        return;
    }

    LOG_INFO("TriggerController: disconnecting...");
    emit propertyUpdated("", "Disconnecting");
    utils::StopWatch sw;

    viClose(dev);
    dev = 0;
    connected = false;

    LOG_INFO("TriggerController: disconnected in {:.3f}ms", sw.Milliseconds());
    emit propertyUpdated("", "Disconnected");
}


uint32_t TriggerController::clearReadBuffer()
{
    char buf[4096];
    ViStatus status;
    ViUInt32 count;

    viGetAttribute(dev, VI_ATTR_ASRL_AVAIL_NUM, &count);
    if (count > 0) {
        slog::Fields log_fields;
        log_fields["device"] = portName;
        log_fields["api_call"] = fmt::format("viRead({})", count);

        utils::StopWatch sw_api_call;
        status = viRead(dev, (uint8_t *)buf, count,  &count);
        log_fields["duration_ms"] = sw_api_call.Milliseconds();
        
        log_fields["response"] = std::string(buf, count);

        if (status < VI_SUCCESS) {
            log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(status));
            LOGFIELDS_ERROR(log_fields, "");
            throw std::runtime_error(fmt::format("failed to remove unexpected data from buffer. Err={:#10X}", uint32_t(status)));
        } else {
            LOGFIELDS_TRACE(log_fields, "");
        }
    }

    return count;
}

void TriggerController::write(const std::string command, std::string caller)
{
    ViStatus status;
    ViUInt32 count;

    std::string cmdWrite = command + "\n";

    slog::Fields log_fields;
    log_fields["device"] = portName;
    log_fields["api_call"] = "viWrite()";
    log_fields["request"] = cmdWrite;

    utils::StopWatch sw_api_call;
    status = viWrite(dev, (const uint8_t *)cmdWrite.c_str(), cmdWrite.size(), &count);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    if (status < VI_SUCCESS) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(status));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("failed to send command. Err={:#10X}", uint32_t(status)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }
}

std::string TriggerController::readline(std::string caller)
{
    ViStatus status;
    ViUInt32 count;

    char buf[4096];

    slog::Fields log_fields;
    log_fields["device"] = portName;
    log_fields["api_call"] = "viRead(4096)";

    utils::StopWatch sw_api_call;
    status = viRead(dev, (uint8_t *)buf, sizeof(buf),  &count);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    log_fields["response"] = std::string(buf, count);

    if (status < VI_SUCCESS) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(status));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("failed to read response. Err={:#10X}", uint32_t(status)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
    }
    if ((count<2) ||(buf[count-1] != '\n') || (buf[count-2] != '\r')) {
        LOGFIELDS_ERROR(log_fields, "unexpected response: no end of line");
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
        LOG_WARN("query: discarded {} bytes of unexpected data before sending command", count);
    }

    //
    // Send command
    //
    std::string cmdWrite = command + "\n";

    slog::Fields log_fields;
    log_fields["device"] = portName;
    log_fields["api_call"] = "viWrite()";
    log_fields["request"] = cmdWrite;

    utils::StopWatch sw_api_call;
    status = viWrite(dev, (const uint8_t *)cmdWrite.c_str(), cmdWrite.size(), &count);
    log_fields["duration_ms"] = sw_api_call.Milliseconds();

    if (status < VI_SUCCESS) {
        log_fields["error_code"] = fmt::format("{:#10x}", uint32_t(status));
        LOGFIELDS_ERROR(log_fields, "");
        throw std::runtime_error(fmt::format("failed to send command. Err={:#10X}", uint32_t(status)));
    } else {
        LOGFIELDS_TRACE(log_fields, "");
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