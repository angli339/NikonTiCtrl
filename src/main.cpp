#include "mainwindow.h"

#include <csignal> // std::signal
#include <cstdlib> // std::getenv
#include <fmt/format.h>
#include <filesystem>
#include <QApplication>
#include <QOpenGLContext>
#include <QStandardPaths>
#include <QtConcurrentRun>

#include "api_server.h"
#include "config.h"
#include "datamanager.h"
#include "devicecontrol.h"
#include "logger.h"
#include "taskcontrol.h"
#include "version.h"

namespace fs = std::filesystem;

void sigHandler(int s)
{
    std::signal(s, SIG_DFL);
    LOG_INFO("Received exit signal...");
    qApp->quit();
}

void showStartupFatalError(std::string message)
{
    QMessageBox msgBox;
    msgBox.setText("Program fails to start");
    msgBox.setInformativeText(message.c_str());
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.addButton("Exit", QMessageBox::AcceptRole);
    msgBox.exec();
}

int main(int argc, char *argv[])
{
    slog::InitConsole();
    
    qRegisterMetaType<std::string>("std::string");
    qRegisterMetaType<uint16_t>("uint16_t");

    QSurfaceFormat fmt;
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    //
    // Find directories for config and log
    //
    
    // C:/ProgramData
    auto program_data_dir = fs::path(std::getenv("ALLUSERSPROFILE"));
    if (program_data_dir.empty()) {
        std::string error_msg = "failed to get ALLUSERSPROFILE path from environment variables";
        LOG_FATAL(error_msg);
        showStartupFatalError(error_msg);
        return 1;
    }
    // C:/ProgramData/NikonTiControl
    fs::path app_dir = program_data_dir / "NikonTiControl";
    if (!fs::exists(app_dir)) {
        std::string error_msg = fmt::format("Directory {} does not exists. It needs to be created manually and assigned with the correct permission.", app_dir.string());
        LOG_FATAL(error_msg);
        showStartupFatalError(error_msg);
        return 1;
    }
    // C:/Users/<username>/AppData/Roaming
    fs::path user_app_data_dir = fs::path(std::getenv("APPDATA"));
    if (user_app_data_dir.empty()) {
        std::string error_msg = "failed to get APPDATA path from environment variables";
        LOG_FATAL(error_msg);
        showStartupFatalError(error_msg);
        return 1;
    }
    // C:/Users/<username>/AppData/Roaming/NikonTiControl
    fs::path user_app_dir = user_app_data_dir / "NikonTiControl";
    if (!fs::exists(user_app_dir)) {
        fs::create_directory(user_app_dir);
    }

    //
    // init logger
    //
    fs::path log_dir = app_dir / "log";
    if (!fs::exists(log_dir)) {
        fs::create_directory(log_dir);
    }
    std::string log_filename = fmt::format("nikon_ti_ctrl-{:%Y%m%d-%H%M%S}.log", utils::Now().Local());
    slog::DefaultLogger().SetFilename(log_dir / log_filename);
    slog::DefaultLogger().SetFlushLevel(slog::level::debug);

    LOG_INFO("Welcome to NikonTiControl {}", gitTagVersion);

    //
    // load config and print to log
    //
    fs::path config_dir = app_dir;
    fs::path config_file = config_dir / "config.json";
    fs::path user_config_dir = user_app_dir;
    fs::path user_config_file = user_config_dir / "user.json";
   
    loadConfig(config_file);
    LOG_INFO("Configuration loaded from {}", config_file.string());
    LOG_INFO("Configurated Labels:");
    for (const auto& [property, labelMap]: configLabel) {
        LOG_INFO("    {}", property);
        for (const auto& [value, label]: labelMap) {
            LOG_INFO("        {}={} ({})", value, label.name, label.description);
        }
    }

    LOG_INFO("Configurated Channels:");
    for (const auto& [name, channelPropertyValue]: configChannel) {
        LOG_INFO("    {}", name);
    }
    
    if (fs::exists(user_config_file)) {
        loadUserConfig(user_config_file);
        LOG_INFO("User configuration loaded from {}", user_config_file.string());
        LOG_INFO("Current User: {} <{}>", configUser.name, configUser.email);
    } else {
        LOG_INFO("User configuration not found.");
        std::string username = std::string(std::getenv("USERNAME"));
        configUser.name = username;
        LOG_INFO("Current User: {}", username);
    }
    
    //
    // Init DataManager
    //
    fs::path userprofile_dir = fs::path(std::getenv("USERPROFILE"));
    if (userprofile_dir.empty()) {
        std::string error_msg = "failed to get USERPROFILE path from environment variables";
        LOG_FATAL(error_msg);
        showStartupFatalError(error_msg);
        return 1;
    }
    fs::path data_root = userprofile_dir / "Data";
    DataManager *dataManager = new DataManager(data_root);

    DeviceControl *dev = new DeviceControl;
    dev->setDataManager(dataManager);

    TaskControl *taskControl = new TaskControl;
    taskControl->setDeviceControl(dev);
    taskControl->setDataManager(dataManager);

    APIServer *apiServer;
    try {
        apiServer = new APIServer;
    } catch (std::runtime_error &e) {
        std::string error_msg = fmt::format("API server failed to start: {}. Another NikonTiControl process may be running.", e.what());
        LOG_FATAL(error_msg);
        showStartupFatalError(error_msg);
        return 1;
    }
    
    apiServer->setDeviceControl(dev);
    apiServer->setDataManager(dataManager);
    apiServer->setTaskControl(taskControl);

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
    LOG_INFO("Exiting...");
    
    delete apiServer;
    delete dataManager;
    delete dev;
    delete w;
    
    LOG_INFO("Exit {}", return_code);
    return return_code;
}
