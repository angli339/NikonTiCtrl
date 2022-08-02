#include <csignal>
#include <future>

#include <QApplication>
#include <QFile>
#include <QSurfaceFormat>
#include <fmt/format.h>

#include "api/api_server.h"
#include "config.h"
#include "device/devicehub.h"
#include "experimentcontrol.h"
#include "logging.h"
#include "qt/mainwindow.h"
#include "utils/time_utils.h"
#include "version.h"

#include "device/hamamatsu/hamamatsu_dcam.h"
#include "device/nikon/nikon_ti.h"
#include "device/prior/prior_proscan.h"

void printEvents(EventStream *stream)
{
    Event e;
    while (stream->Receive(&e)) {
        switch (e.type) {
        case EventType::DeviceConnectionStateChanged:
            LOG_DEBUG("[Event:{}] {}=\"{}\"", EventTypeToString(e.type),
                      e.device, e.value);
            break;
        case EventType::DeviceOperationComplete:
            LOG_DEBUG("[Event:{}] {}=\"{}\"", EventTypeToString(e.type), e.path,
                      e.value);
            break;
        case EventType::DevicePropertyValueUpdate:
            if ((e.path.PropertyName() != "XYPosition") &&
                (e.path.PropertyName() != "RawXYPosition") &&
                (e.path.PropertyName() != "ZDrivePosition"))
            {
                LOG_DEBUG("[Event:{}] {}=\"{}\"", EventTypeToString(e.type),
                          e.path, e.value);
            }
        }
    }
}

std::string api_listen_addr = "0.0.0.0:50051";

void sigHandler(int signal)
{
    std::signal(signal, SIG_DFL);
    LOG_INFO("Received exit signal...");

    qApp->exit(0);
}

void configApp()
{
    QFile file(":/qdarkstyle/dark/darkstyle.qss");
    file.open(QFile::ReadOnly);
    QString styleSheet = QString::fromLatin1(file.readAll());
    styleSheet += "\nQPushButton { border-radius: 0px; }\nQWidget:focus "
                  "{border: 1px solid #1A72BB; }";
    qApp->setStyleSheet(styleSheet);
}

int main(int argc, char *argv[])
{
    slog::InitConsole();
    LOG_INFO("Welcome to NikonTiControl {}", gitTagVersion);

    try {
        std::filesystem::path systemConfigPath = getSystemConfigPath();
        loadSystemConfig(systemConfigPath);
        LOG_INFO("  System config loaded from {}", systemConfigPath.string());

        std::filesystem::path userConfigPath = getUserConfigPath();
        loadUserConfig(userConfigPath);
        LOG_INFO("  User config loaded from {}", userConfigPath.string());

    } catch (std::exception &e) {
        LOG_FATAL("Failed to load config: {}", e.what());
        return 1;
    }

    LOG_INFO("Current user: {}<{}>", config.user.name, config.user.email);

    //
    // Add devices
    //
    DeviceHub dev;
    try {
        dev.AddDevice("NikonTi", new NikonTi::Microscope);
        dev.AddDevice("PriorProScan",
                      new PriorProscan::Proscan("ASRL1::INSTR"));
        dev.AddCamera("Hamamatsu", new Hamamatsu::DCam);
        // dev.AddDevice("FLIR", new FLIR::Camera);

    } catch (std::exception &e) {
        LOG_ERROR("Failed to add device: {}", e.what());
    }

    ExperimentControl exp(&dev);

    //
    // Start API Server
    //
    APIServer api_server(api_listen_addr, &exp);
    auto api_server_future =
        std::async(std::launch::async, &APIServer::Wait, &api_server);
    LOG_INFO("Listening {}...", api_listen_addr);

    //
    // Init and show UI window
    //
    QApplication app(argc, argv);
    configApp();
    MainWindow w;
    w.setDeviceHub(&dev);
    w.setExperimentControl(&exp);
    w.showMaximized();
    utils::StopWatch sw;

    //
    // Print events for debugging
    //
    EventStream event_stream;
    auto print_event_future =
        std::async(std::launch::async, printEvents, &event_stream);
    dev.SubscribeEvents(&event_stream);

    //
    // Connect devices
    //
    std::future<void> connect_devices_future =
        std::async(std::launch::async, [&] {
            absl::Status status = dev.ConnectAll();
            if (!status.ok()) {
                LOG_ERROR("Connect: {}", status.ToString());
            } else {
                LOG_INFO("All connected");
            }

            // Init properties
            status = dev.SetProperty("/Hamamatsu/BIT PER CHANNEL", "16");
            if (!status.ok()) {
                LOG_ERROR("Init device properties: {}", status.ToString());
            } else {
                LOG_INFO("Device initialized");
            }
        });

    //
    // Handle exit signal
    //
    std::signal(SIGINT, sigHandler);
    std::signal(SIGTERM, sigHandler);

    //
    // Start UI event loop
    //
    int returnCode = app.exec();

    LOG_INFO("Disconnecting devices...");
    dev.DisconnectAll();
    LOG_INFO("Disconnected");

    LOG_INFO("Shutting down API Server...");
    api_server.Shutdown();
    api_server_future.wait();

    event_stream.Close();
    print_event_future.wait();
    return returnCode;
}
