#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCharts/QtCharts>

#include "datamanager.h"
#include "taskcontrol.h"
#include "image.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setDataManager(DataManager *dataManager);
    void setTaskControl(TaskControl *taskControl);

signals:
    void requestSetTaskState(std::string state);
    void requestSetChannel(std::string channel);
    void requestDevicePropertyGet(std::string name);
    void requestDevicePropertySet(std::string name, std::string value);

public slots:
    void displayImage(Image *im);
    void updateDeviceProperty(std::string name, std::string value);
    void updateTaskState(std::string state);
    void updateChannel(std::string channel);

private:
    Ui::MainWindow *ui;
    DataManager *dataManager;
    TaskControl *taskControl;

    void updateButtonGroup(std::unordered_map<std::string, QPushButton *> buttons, std::string value);
    void setButtonGroupEnabled(std::unordered_map<std::string, QPushButton *> buttons, bool enabled);

    // Histogram View
    QChart *histChart;
    QLineSeries *histSeries;
    QValueAxis *histAxisX;
    QValueAxis *histAxisY;

    // Device Control View
    std::unordered_map<std::string, QPushButton *> filterBlock1Buttons;
    std::unordered_map<std::string, QPushButton *> lightPathButtons;
    std::unordered_map<std::string, QPushButton *> nosePieceButtons;
    std::unordered_map<std::string, QPushButton *> diaShutterButtons;
    std::unordered_map<std::string, QPushButton *> pfsStateButtons;
    std::unordered_map<std::string, QPushButton *> excitationFilterButtons;
    std::unordered_map<std::string, QPushButton *> emissionFilterButtons;

    // Task Control View
    std::unordered_map<std::string, QPushButton *> channelButtons;

    void initHistogramView();
    void initDeviceControlView();
    void initTaskControlView();
};
#endif // MAINWINDOW_H
