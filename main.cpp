#include "mainwindow.h"

#include <csignal>

#include <QApplication>
#include <QOpenGLContext>
#include <QtConcurrentRun>
#include <QStandardPaths>

#include "api_server.h"
#include "datamanager.h"
#include "devicecontrol.h"
#include "taskcontrol.h"
#include "logging.h"

void sigHandler(int s)
{
    std::signal(s, SIG_DFL);
    SPDLOG_INFO("Received exit signal...");
    qApp->quit();
}

int main(int argc, char *argv[])
{
    init_logger();
    SPDLOG_INFO("Welcome to NikonTiControl");

    qRegisterMetaType<std::string>("std::string");
    qRegisterMetaType<uint16_t>("uint16_t");

    QSurfaceFormat fmt;
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    DataManager *dataManager = new DataManager;

    DeviceControl *dev = new DeviceControl;
    dev->setDataManager(dataManager);

    TaskControl *taskControl = new TaskControl;
    taskControl->setDeviceControl(dev);
    taskControl->setDataManager(dataManager);

    APIServer apiServer;
    apiServer.setDeviceControl(dev);
    apiServer.setDataManager(dataManager);
    apiServer.setTaskControl(taskControl);

    MainWindow *w = new MainWindow;
    QObject::connect(w, &MainWindow::requestDevicePropertyGet, [dev](std::string name){
        dev->getDeviceProperty(name);
    });
    QObject::connect(w, &MainWindow::requestDevicePropertySet, dev, &DeviceControl::setDeviceProperty);
    QObject::connect(dev, &DeviceControl::propertyUpdated, w, &MainWindow::updateDeviceProperty);

    w->setTaskControl(taskControl);
    w->setDataManager(dataManager);
    w->show();
    QtConcurrent::run(dev, &DeviceControl::connectAll);

    int return_code = app.exec();
    SPDLOG_INFO("Exiting...");

    delete w;
    delete dataManager;
    delete dev;

    SPDLOG_INFO("Exit {}", return_code);
    deinit_logger();
    return return_code;
}
