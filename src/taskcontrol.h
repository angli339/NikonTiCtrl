#ifndef TASKCONTROL_H
#define TASKCONTROL_H

#include <future>
#include <QObject>
#include <string>
#include <thread>
#include <vector>

#include "datamanager.h"
#include "devicecontrol.h"

struct ChannelSetting
{
    bool enabled;
    std::string name;
    double intensity;
    double exposure_ms;
};

struct Task
{
    std::string name_prefix;
    std::string name_suffix;
    std::vector<ChannelSetting> channels;
};

class TaskControl : public QObject
{
    Q_OBJECT
public:
    TaskControl(QObject *parent = nullptr);
    ~TaskControl();
    void setDeviceControl(DeviceControl *dev);
    void setDataManager(DataManager *dataManager) { this->dataManager = dataManager; }
    
    void openShutter();
    void closeShutter();
    std::string captureImage(std::string name = "");

    std::string getChannel();
    std::string getTaskState() { return taskState; }

    std::string getTaskProperty(std::string name);
    void setTaskProperty(std::string name, std::string value);
    std::vector<std::string> listTaskProperty(std::string name);

public slots:
    void setTaskState(std::string state);
    void switchToChannel(std::string channel);
    void processDevicePropertyUpdate(std::string name, std::string value);

signals:
    void taskStateChanged(std::string state);
    void channelChanged(std::string channel);
    void propertyUpdated(std::string name, std::string value);

private:
    // Current channel and exposure time
    std::string channel;
    std::string binning = "1x1";
    double exposure_ms = 25;

    std::string taskState = "Not Connected";
    std::thread taskThread;

    DeviceControl *dev;
    DataManager *dataManager;

    Task task;
    int tempID = 1;

    std::future<bool> getFrameFuture;

    void getSaveFrame(Image *im);
};

#endif // TASKCONTROL_H
