#include "taskcontrol.h"

#include <stdexcept>

#include "config.h"
#include "image.h"
#include "logger.h"
#include "utils/string_utils.h"
#include "utils/time_utils.h"

TaskControl::TaskControl(QObject *parent) : QObject(parent)
{
    for (const auto& kv : configChannel) {
        ChannelSetting ch;
        ch.name = kv.first;
        ch.enabled = false;
        ch.exposure_ms = 50;
        ch.intensity = 100;
        task.channels.push_back(ch);
    }
}

TaskControl::~TaskControl()
{

}

void TaskControl::setDeviceControl(DeviceControl *dev) {
    this->dev = dev;
    connect(dev, &DeviceControl::propertyUpdated, this, &TaskControl::processDevicePropertyUpdate);
}

void TaskControl::switchToChannel(std::string channel)
{
    auto channelConfig_Filter = getChannelConfig_Filter(channel);

    std::unordered_map<std::string, std::string> propertyToChange;

    auto cache = dev->getCachedDeviceProperty();
    for (const auto& [name, value] : channelConfig_Filter) {
        auto it = cache.find(name);
        if (it == cache.end()) {
            LOG_WARN("TaskControl::switchToChannel({}): {} not found in cache", channel, name);
            propertyToChange[name] = value;
        }
        if (it->second != value) {
            propertyToChange[name] = value;
        }
    }

    if (propertyToChange.size() == 0) {
        LOG_INFO("TaskControl: {} is already the current channel", channel);
        emit channelChanged(channel);
        return;
    }

    LOG_INFO("TaskControl: switching to {}...", channel);
    utils::stopwatch sw;

    std::vector<std::string> waitProperties;

    for (const auto& [name, value] : propertyToChange) {
        dev->setDeviceProperty(name, value);
        waitProperties.push_back(name);    
    }

    if (dev->waitDeviceProperty(waitProperties, std::chrono::milliseconds(5000))) {
        this->channel = channel;
        LOG_INFO("TaskControl: switched to {}. {:.3f}ms", channel, stopwatch_ms(sw));
        emit channelChanged(channel);
    } else {
        LOG_ERROR("TaskControl: timeout");
    }
}

void TaskControl::openShutter()
{
    auto it = configChannel.find(channel);
    if (it == configChannel.end()) {
        throw std::invalid_argument("invalid channel");
    }
    auto ch = it->second;

    std::vector<std::string> waitProperties;

    auto it_diaShutter = ch.find("NikonTi/DiaShutter");
    if (it_diaShutter != ch.end()) {
        std::string name = it_diaShutter->first;
        std::string value = it_diaShutter->second;
        if (dev->getDeviceProperty(name, false) != value) {
            dev->setDeviceProperty(name, value);
            waitProperties.push_back(name);
        }
    }

    auto it_lumenShutter = ch.find("PriorProScan/LumenShutter");
    if (it_lumenShutter != ch.end()) {
        std::string name = it_lumenShutter->first;
        std::string value = it_lumenShutter->second;
        if (dev->getDeviceProperty(name, false) != value) {
            dev->setDeviceProperty(name, value);
            waitProperties.push_back(name);
        }
    }

    if (waitProperties.size() == 0) {
        return;
    }

    using namespace std::chrono_literals;
    if (dev->waitDeviceProperty(waitProperties, 500ms)) {
        LOG_INFO("TaskControl: openShutter: done", channel);
    } else {
        LOG_ERROR("TaskControl: openShutter: timeout");
    }
}

void TaskControl::closeShutter()
{
    std::vector<std::string> waitProperties;

    if (dev->getDeviceProperty("NikonTi/DiaShutter", false) != "Off") {
        try {
            dev->setDeviceProperty("NikonTi/DiaShutter", "Off");
        } catch (std::runtime_error &e1) {
            LOG_ERROR("TaskControl: Nikon DiaShutter failed to close: {}. Try again...", e1.what());
            try {
                dev->setDeviceProperty("NikonTi/DiaShutter", "Off");
            } catch (std::runtime_error &e2) {
                LOG_ERROR("TaskControl: Nikon DiaShutter still failed to close.");
                throw std::runtime_error(fmt::format("shutter failed to close: {}", e2.what()));
            }
        }
        
        waitProperties.push_back("NikonTi/DiaShutter");
    }

    if (dev->getDeviceProperty("PriorProScan/LumenShutter", false) != "Off") {
        dev->setDeviceProperty("PriorProScan/LumenShutter", "Off");
        waitProperties.push_back("PriorProScan/LumenShutter");
    }

    if (waitProperties.size() == 0) {
        return;
    }

    using namespace std::chrono_literals;
    if (dev->waitDeviceProperty(waitProperties, 500ms)) {
        LOG_INFO("TaskControl: closeShutter: done", channel);
    } else {
        LOG_ERROR("TaskControl: closeShutter: timeout");
    }
}

std::string TaskControl::captureImage(std::string name)
{
    if (getFrameFuture.valid()) {
        LOG_INFO("TaskControl: waiting for getFrame");
        getFrameFuture.wait();
        LOG_INFO("TaskControl: getFrame completed");
    }
    if (taskState != "Ready") {
        throw std::runtime_error(fmt::format("TaskControl: captureImage. taskStatus is {}, not ready", taskState));
    }

    taskState = "Capture";
    emit taskStateChanged(taskState);
    LOG_INFO("TaskControl: captureImage start --------------");

    dev->setExposure(exposure_ms);
    dev->setBinning(binning);

    utils::stopwatch sw;
    dev->allocBuffer(1);
    LOG_INFO("TaskControl: allocBuffer {:.3f}ms", stopwatch_ms(sw));
    
    if (channel != "") {
        sw.reset();
        openShutter();
        LOG_INFO("TaskControl: captureImage openShutter {:.3f}ms", stopwatch_ms(sw));
    }

    sw.reset();
    dev->startAcquisition();
    LOG_INFO("TaskControl: captureImage startAcquisition {:.3f}ms", stopwatch_ms(sw));
    
    sw.reset();
    if (ext_trigger_enable) {
        dev->outputTriggerPulse();
    }

    dev->waitExposureEnd(exposure_ms + 500);
    std::string timestamp = utils::Now().FormatRFC3339();
    auto deviceProperty = dev->getCachedDeviceProperty();
    LOG_INFO("TaskControl: captureImage waitExposureEnd {:.3f}ms", stopwatch_ms(sw));

    Image *im = new Image;

    im->id = fmt::format("image-{}", tempID);
    tempID++;

    if (name == "") {
        im->name = fmt::format("{}-{}", im->id, channel);
    } else {
        im->name = name;
    }

    im->channel = channel;
    im->exposure_ms = exposure_ms;
    im->timestamp = timestamp;
    im->deviceProperty = deviceProperty;

    getFrameFuture = std::async(std::launch::async, [this, im] {
        getSaveFrame(im);

        try {
            LOG_DEBUG("TaskControl: releaseBuffer");
            dev->releaseBuffer();
        } catch (std::exception &e) {
            LOG_ERROR("TaskControl: releaseBuffer failed: {}", e.what());
        }
        
        LOG_INFO("TaskControl: captureImage done --------------");

        this->taskState = "Ready";
        emit taskStateChanged(this->taskState);

        return true;
    });

    return im->id;
}

void TaskControl::getSaveFrame(Image *im)
{
    utils::stopwatch sw;

    if (channel != "") {
        sw.reset();
        try {
            closeShutter();
        } catch (std::runtime_error &e) {
            this->taskState = "Error";
            emit taskStateChanged(this->taskState);
            throw std::runtime_error(e);
        }
        LOG_INFO("TaskControl: getSaveFrame closeShutter {:.3f}ms after exposure end", stopwatch_ms(sw));
    }
    uint32_t bufSize;
    uint16_t imageWidth;
    uint16_t imageHeight;
    dev->waitFrameReady(500);
    LOG_INFO("TaskControl: getSaveFrame frame ready {:.3f}ms after exposure end", stopwatch_ms(sw));


    uint8_t *buf = dev->getFrame(&bufSize, &imageWidth, &imageHeight);

    LOG_INFO("TaskControl: data transfer completed. image size {}x{}.  {:.3f}ms after exposure end", imageWidth, imageHeight, stopwatch_ms(sw));
    
    im->width = imageWidth;
    im->height = imageHeight;
    im->pixelFormat = PixelMono16;
    im->bufSize = imageWidth * imageHeight * 2;
    im->buf =  (uint8_t *)malloc(im->bufSize);
    memcpy_s(im->buf, im->bufSize, buf, im->bufSize);

    auto xy_position = im->deviceProperty["PriorProScan/XYPosition"];
    auto n = xy_position.find(',');
    double x = std::stod(xy_position.substr(0, n));
    double y = std::stod(xy_position.substr(n + 1));
    im->positionX_um= x;
    im->positionY_um= y;

    double z = std::stod(im->deviceProperty["NikonTi/ZDrivePosition"]);
    im->positionZ_um = z;

    
    if (im->deviceProperty["PriorProScan/LumenShutter"] == "On") {
        im->excitationIntensity = std::stod(im->deviceProperty["PriorProScan/LumenOutputIntensity"]);
    }
    im->pixelSize_um = configPixelSize["60xO"];

    dataManager->addImage(im);
}

void TaskControl::setTaskState(std::string state)
{
    if ((this->taskState == "Ready") && (state == "Live")) {
        if (ext_trigger_enable) {
            dev->setLevelTrigger(false);
        }
        if (channel == "BF") {
            dev->setExposure(25);
            openShutter();
        } else {
            dev->setExposure(exposure_ms);
        }
        dev->setBinning(binning);
        dev->allocBuffer(2);
        dev->startContinuousAcquisition();
        dataManager->setLiveViewStatus(true);

        double timeout = dev->getExposure() + 500;

        taskThread = std::thread([this, timeout]{
            auto t_previous = std::chrono::steady_clock::now();
            int n_frame_count = 0;
            for (;;) {
                try {
                    if ((n_frame_count > 0) && (n_frame_count % 5 == 0)) {
                        auto t_now = std::chrono::steady_clock::now();
                        auto duration = t_now - t_previous;
                        t_previous = t_now;
                        int duration_us = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
                        double frame_rate =  1 / ((double)duration_us * 1e-6 / 5);
                        LOG_INFO("live view frame rate: {}", frame_rate);
                    }
                    
                    dev->waitFrameReady(timeout);
                    Image *im = new Image;
                    uint8_t *buf = dev->getFrame(NULL, &im->width, &im->height);
                    n_frame_count++;
                    im->pixelFormat = PixelMono16;
                    im->bufSize = im->width * im->height * 2;
                    im->buf = (uint8_t *)malloc(im->bufSize);
                    memcpy_s(im->buf, im->bufSize, buf, im->bufSize);
                    dataManager->setLiveImage(im);
                } catch (std::exception& e) {
                    if (!dev->isAcquisitionRunning()) {
                        if (channel == "BF") {
                            closeShutter();
                        }
                        return;
                    }
                    LOG_ERROR("Error during live view: {}", e.what());
                    this->taskState = "Error";
                    if (channel == "BF") {
                        closeShutter();
                    }
                    return;
                }

                if (!dev->isAcquisitionRunning()) {
                    if (channel == "BF") {
                        closeShutter();
                    }
                    return;
                }
            }
        });
        this->taskState = "Live";
        emit taskStateChanged(this->taskState);
    } else if ((this->taskState == "Ready") && (state == "Capture")) {
        taskThread = std::thread([this]{
            try {
                captureImage();
            } catch (std::exception& e) {
                LOG_ERROR("capture failed: {}", e.what());
                this->taskState = "Error";
                return;
            }
        });
        taskThread.detach();
    } else if ((this->taskState == "Ready") && (state == "Run")) {
        this->taskState = "Run";
        emit taskStateChanged(this->taskState);
    } else if ((this->taskState == "Pause") && (state == "Run")) {
        this->taskState = "Run";
        emit taskStateChanged(this->taskState);
    } else if ((this->taskState == "Run") && (state == "Stop")) {
        this->taskState = "Ready";
        emit taskStateChanged(this->taskState);
    } else if ((this->taskState == "Live") && (state == "Stop")) {
        dev->stopAcquisition();
        dataManager->setLiveViewStatus(false);
        taskThread.join();
        dev->releaseBuffer();
        if (ext_trigger_enable) {
            dev->setLevelTrigger(ext_trigger_enable);
        }
        this->taskState = "Ready";
        emit taskStateChanged(this->taskState);
    } else if ((this->taskState == "Capture") && (state == "Stop")) {
        dev->stopAcquisition();
        taskThread.join();
        this->taskState = "Ready";
        emit taskStateChanged(this->taskState);
    } else if ((this->taskState == "Run") && (state == "Pause")) {
        this->taskState = "Pause";
        emit taskStateChanged(this->taskState);
    } else {
        LOG_ERROR("undefined state transition: '{}' -> '{}'", this->taskState, state);
        return;
    }
}

void TaskControl::processDevicePropertyUpdate(std::string name, std::string value)
{
    // When any device is connected or disconnected
    if ((value == "Connected") || (value == "Disconnected")) {
        bool channelReady = true;
        bool acquisitionReady = true;
        if (dev->getDeviceProperty("NikonTi/") != "Connected") {
            channelReady = false;
            acquisitionReady = false;
        }
        if (dev->getDeviceProperty("PriorProScan/") != "Connected") {
            channelReady = false;
            acquisitionReady = false;
        }
        if (dev->getDeviceProperty("Hamamatsu/") != "Connected") {
            acquisitionReady = false;
        }

        if (channelReady) {
            std::string channel = getChannel();
            emit channelChanged(channel);
        }
        if (acquisitionReady) {
            this->taskState = "Ready";
            emit taskStateChanged(this->taskState);
        } else {
            this->taskState = "Not Connected";
        }
    }
}

std::string TaskControl::getChannel()
{
    auto cache = dev->getCachedDeviceProperty();
    channel = getChannelFromDeviceProperties(cache);
    emit channelChanged(channel);
    return channel;
}

std::vector<std::string> TaskControl::listTaskProperty(std::string name)
{
    if (name == "") {
        return {"ChannelSettings", "Channel", "ExposureTime_ms", "Binning", "ExtTrigger", "NamePrefix", "NameSuffix"};
    }
    if (name == "ChannelSettings") {
        std::vector<std::string> ch_list;

        for (const auto& ch : task.channels) {
            ch_list.push_back("ChannelSettings/" + ch.name);
            ch_list.push_back("ChannelSettings/" + ch.name + "/IlluminationIntensity");
            ch_list.push_back("ChannelSettings/" + ch.name + "/ExposureTime_ms");
        }
        return ch_list;
    }

    throw std::invalid_argument("invalid name"); 
}

std::string TaskControl::getTaskProperty(std::string name)
{
    if (name == "Channel") {
        return getChannel();
    }
    if (name == "ExposureTime_ms") {
        return fmt::format("{}", exposure_ms);
    }
    if (name == "Binning") {
        return binning;
    }
    if (name == "ExtTrigger") {
        if (ext_trigger_enable) {
            return "Enable";
        } else {
            return "Disable";
        }
    }
    if (util::hasPrefix(name, "ChannelSettings")) {
        std::string ch_property_name = util::trimPrefix(name, "ChannelSettings/");
        
        for (const auto& ch : task.channels) {
            if (ch_property_name == ch.name) {
                return ch.enabled ? "Enabled" : "Disabled";
            } else if (ch_property_name == ch.name + "/IlluminationIntensity") {
                return fmt::format("{}", ch.intensity);
            } else if (ch_property_name == ch.name + "/ExposureTime_ms") {
                return fmt::format("{}", ch.exposure_ms);
            } else {
                continue;
            }
        }
        throw std::invalid_argument(fmt::format("{} not found", name));
    }
    throw std::invalid_argument(fmt::format("{} not found", name));
}

void TaskControl::setTaskProperty(std::string name, std::string value)
{
    if (name == "Channel") {
        switchToChannel(value);
        return;
    }
    if (name == "ExposureTime_ms") {
        exposure_ms = std::stod(value);
        return;
    }
    if (name == "Binning") {
        binning = value;
        return;
    }
    if (name == "ExtTrigger") {
        if (value == "Enable") {
            setExtTriggerMode(true);
        } else if (value == "Disable") {
            setExtTriggerMode(false);
        } else {
            throw std::invalid_argument(fmt::format("invalid value '{}', should be 'Enable' or 'Disable'", value));
        }
        return;
    }
    if (name == "NamePrefix") {
        task.name_prefix = value;
        return;
    }
    if (name == "NameSuffix") {
        task.name_suffix = value;
        return;
    }
    if (util::hasPrefix(name, "ChannelSettings")) {
        std::string ch_property_name = util::trimPrefix(name, "ChannelSettings/");
        for (auto& ch : task.channels) {
            if (ch_property_name == ch.name) {
                if (value == "Enabled") {
                    ch.enabled = true;
                    return;
                } else if (value == "Disabled") {
                    ch.enabled = false;
                    return;
                } else {
                    throw std::invalid_argument(fmt::format("cannot set {}={}, expecting 'Enabled' or 'Disabled'", name, value));
                }
            } else if (ch_property_name == ch.name + "/IlluminationIntensity") {
                ch.intensity = stod(value);
                return;
            } else if (ch_property_name == ch.name + "/ExposureTime_ms") {
                ch.exposure_ms = stod(value);
                return;
            } else {
                throw std::invalid_argument(fmt::format("{} not found", name));
            }
        }
    }
    throw std::invalid_argument(fmt::format("{} not found", name));
}

void TaskControl::setExtTriggerMode(bool ext_trigger_enable)
{
    this->ext_trigger_enable = ext_trigger_enable;

    if (ext_trigger_enable) {
        dev->setLevelTrigger(true);
    } else {
        dev->setLevelTrigger(false);
    }
}