#include "devnotify.h"

#include <stdexcept>

#include "logger.h"

#include <windows.h>
#include <dbt.h>

DevNotify *g_dev_notify = nullptr;

// https://docs.microsoft.com/en-us/windows/win32/devio/wm-devicechange
LRESULT CALLBACK DevNotifyWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT retVal = 1; // Default return value: True

    if (uMsg == WM_DEVICECHANGE) {
        if ((wParam != DBT_DEVICEARRIVAL) || (wParam != DBT_DEVICEREMOVECOMPLETE)) {
            // Ingore other event types
            return retVal;
        }

        DEV_BROADCAST_HDR *eventDataHdr = (DEV_BROADCAST_HDR *)lParam;
        if (eventDataHdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE) {
            // Ingore unexpected event data
            return retVal;
        }

        //
        // Extract name from event data
        //
        DEV_BROADCAST_DEVICEINTERFACE_W *eventData = (DEV_BROADCAST_DEVICEINTERFACE_W *)lParam;
        size_t sizeName = eventData->dbcc_size - sizeof(DEV_BROADCAST_DEVICEINTERFACE_W) + 1;
        std::wstring wname = std::wstring(eventData->dbcc_name, sizeName);
        std::string name = std::string(wname.begin(), wname.end());

        //
        // Emit signals
        //
        if (g_dev_notify) {
            if (wParam == DBT_DEVICEARRIVAL) {
                LOG_DEBUG("DevNotify:DevNotifyWindowProc DeviceArrival: {}", name);
                emit g_dev_notify->DeviceArrival(name);
            }
            if (wParam == DBT_DEVICEREMOVECOMPLETE) {
                LOG_DEBUG("DevNotify:DevNotifyWindowProc DeviceRemoveComplete: {}", name);
                emit g_dev_notify->DeviceRemovaComplete(name);
            }
        }
    } else if (uMsg == WM_DESTROY) {
        PostQuitMessage(0);
    } else {
        retVal = DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    return retVal;
}

DevNotify::DevNotify(QObject *parent) : QThread(parent)
{
    if (g_dev_notify != 0) {
        throw std::runtime_error("multiple DevNotify instances are not yet supported");
    }
    this->start();
}

DevNotify::~DevNotify()
{
    if (UnregisterDeviceNotification(hDevNotify)) {
        CloseHandle(hDevNotify);
        hDevNotify = NULL;
    }
    if (SendMessageW(hWindow, WM_CLOSE, 0, 0)) {
        CloseHandle(hWindow);
        hWindow = NULL;
    }
    wait();
    g_dev_notify = NULL;
}

void DevNotify::run() {
    //
    // Create a hidden Window
    //
    HMODULE hModule = GetModuleHandleW(NULL);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &DevNotifyWindowProc;
    wc.hInstance = hModule;
    wc.lpszClassName = L"DevNotifyEventWindow";

    RegisterClassExW(&wc);

    hWindow = CreateWindowExW(
        0,
        L"DevNotifyEventWindow",
        NULL,
        0,
        0, 0, 0, 0,
        0,
        0,
        hModule,
        0);

    if (hWindow == NULL) {
        throw std::runtime_error("failed to create event window for DevNotify");
    }

    //
    // Register device notification
    //
    DEV_BROADCAST_DEVICEINTERFACE_W notificationFilter;
    memset(&notificationFilter, 0, sizeof(notificationFilter));

    notificationFilter.dbcc_size = sizeof(notificationFilter);
    notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    hDevNotify = RegisterDeviceNotificationW(
        hWindow,
        &notificationFilter,
        DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    if (hDevNotify == NULL) {
        throw std::runtime_error("RegisterDeviceNotificationW failed");
    }

    LOG_DEBUG("DevNotify: loop started");

    g_dev_notify = this;

    //
    // Start event loop of Window in the same thread
    //
    MSG msg;
    int8_t bRet;

    for (;;) {
        bRet = GetMessageW(&msg, hWindow, 0, 0);
        if (bRet == -1) {
            throw std::runtime_error("GetMessageW failed");
        }
        if (bRet == 0) {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    LOG_DEBUG("DevNotify: loop stopped");
}
