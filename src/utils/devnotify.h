#ifndef DEVNOTIFY_H
#define DEVNOTIFY_H

#include <QThread>

struct HWND__;
typedef HWND__ *HWND;
typedef void *HDEVNOTIFY;

class DevNotify : public QThread
{
    Q_OBJECT
public:
    explicit DevNotify(QObject *parent = nullptr);
    ~DevNotify();

    void run() override;

signals:
    void DeviceArrival(std::string name);
    void DeviceRemovaComplete(std::string name);

private:
    HWND hWindow = NULL;
    HDEVNOTIFY hDevNotify = NULL;
};

#endif // DEVNOTIFY_H
