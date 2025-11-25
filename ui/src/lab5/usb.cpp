#include <windows.h>
#include <dbt.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <map>
#include <algorithm>
#include <chrono>

const GUID GUID_DEVINTERFACE_DISK = { 0x53f56307, 0xb6bf, 0x11d0, { 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b } };
const GUID GUID_DEVINTERFACE_MOUSE = { 0x378de44c, 0x56ef, 0x11d1, { 0xbc, 0x8c, 0x00, 0xa0, 0xc9, 0x14, 0x05, 0xdd } };

std::map<std::string, HANDLE> lockedDevices;

auto lastSafeRemovalRequestTime = std::chrono::steady_clock::time_point::min();
auto lastLogTime = std::chrono::steady_clock::time_point::min();
WPARAM lastLogWParam = 0;

struct DeviceInfo {
    std::string id;
    std::string name;
    std::string type;
    std::string driveLetter;
};

std::string escapeJson(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '\\') res += "\\\\";
        else if (c == '"') res += "\\\"";
        else res += c;
    }
    return res;
}

bool IdsEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), 
        [](char c1, char c2) { return tolower(c1) == tolower(c2); });
}

void SendLog(std::string msg, std::string level = "normal") {
    std::cout << "{ \"type\": \"log\", \"message\": \"" << escapeJson(msg) << "\", \"level\": \"" << level << "\" }" << std::endl;
}

std::string GetProperty(DEVINST devInst, ULONG property) {
    char buffer[1024];
    ULONG len = sizeof(buffer);
    if (CM_Get_DevNode_Registry_PropertyA(devInst, property, NULL, (PBYTE)buffer, &len, 0) == CR_SUCCESS) {
        return std::string(buffer);
    }
    return "";
}

std::string GetDetailedName(DEVINST devInst) {
    std::string name = GetProperty(devInst, CM_DRP_FRIENDLYNAME);
    if (!name.empty()) return name;
    name = GetProperty(devInst, CM_DRP_DEVICEDESC);
    
    if (name == "USB Input Device" || name.find("HID") != std::string::npos || name == "Disk drive" || name == "USB Mass Storage Device") {
        DEVINST parentInst;
        if (CM_Get_Parent(&parentInst, devInst, 0) == CR_SUCCESS) {
            std::string parentName = GetProperty(parentInst, CM_DRP_FRIENDLYNAME);
            if (!parentName.empty() && parentName != "USB Composite Device") return parentName;
        }
    }
    return name.empty() ? "Unknown Device" : name;
}

std::string GetDriveLetter(const std::string& deviceId) {
    DWORD drives = GetLogicalDrives();
    char driveName[] = "A:";
    for (int i = 0; i < 26; i++) {
        if (drives & (1 << i)) {
            driveName[0] = 'A' + i;
            if (GetDriveTypeA(driveName) == DRIVE_REMOVABLE) return std::string(driveName);
        }
    }
    return "";
}

void ListDevices() {
    std::vector<DeviceInfo> devices;
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA spDevInfoData;

    hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DISK, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo != INVALID_HANDLE_VALUE) {
        spDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        for (int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &spDevInfoData); i++) {
            char buf[1024];
            if (CM_Get_Device_IDA(spDevInfoData.DevInst, buf, 1024, 0) == CR_SUCCESS) {
                std::string devId = buf;
                if (devId.find("USB") != std::string::npos) {
                    DeviceInfo dev;
                    dev.id = devId;
                    dev.name = GetDetailedName(spDevInfoData.DevInst);
                    dev.type = "DISK";
                    dev.driveLetter = GetDriveLetter(devId);
                    devices.push_back(dev);
                }
            }
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }

    hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_MOUSE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo != INVALID_HANDLE_VALUE) {
        spDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        for (int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &spDevInfoData); i++) {
            char buf[1024];
            if (CM_Get_Device_IDA(spDevInfoData.DevInst, buf, 1024, 0) == CR_SUCCESS) {
                std::string devId = buf;
                if (devId.find("USB") != std::string::npos || devId.find("HID") != std::string::npos) {
                    DeviceInfo dev;
                    dev.id = devId;
                    dev.name = GetDetailedName(spDevInfoData.DevInst);
                    dev.type = "MOUSE";
                    devices.push_back(dev);
                }
            }
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }

    std::cout << "{ \"type\": \"device_list\", \"devices\": [";
    for (size_t i = 0; i < devices.size(); ++i) {
        bool isLocked = lockedDevices.find(devices[i].id) != lockedDevices.end();
        std::cout << "{ \"id\": \"" << escapeJson(devices[i].id) 
                  << "\", \"name\": \"" << escapeJson(devices[i].name) 
                  << "\", \"type\": \"" << escapeJson(devices[i].type)
                  << "\", \"path\": \"" << escapeJson(devices[i].driveLetter)
                  << "\", \"isLocked\": " << (isLocked ? "true" : "false") 
                  << " }";
        if (i < devices.size() - 1) std::cout << ",";
    }
    std::cout << "] }" << std::endl;
}

void LockDevice(const std::string& id) {
    std::string drive = GetDriveLetter(id); 
    if (drive.empty()) {
        SendLog("Cannot lock: No drive letter found.", "error");
        return;
    }
    std::string path = "\\\\.\\" + drive;
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        lockedDevices[id] = hFile;
        SendLog("Device LOCKED. Windows Safe Eject will now fail.", "warning");
        ListDevices();
    } else {
        SendLog("Lock Failed. Error: " + std::to_string(GetLastError()), "error");
    }
}

void UnlockDevice(const std::string& id) {
    if (lockedDevices.find(id) != lockedDevices.end()) {
        CloseHandle(lockedDevices[id]);
        lockedDevices.erase(id);
        SendLog("Device UNLOCKED.");
        ListDevices();
    }
}

bool AttemptEject(DEVINST devInst) {
    PNP_VETO_TYPE vetoType = PNP_VetoTypeUnknown; 
    char vetoName[MAX_PATH];

    CONFIGRET res = CM_Request_Device_EjectA(devInst, &vetoType, vetoName, MAX_PATH, 0);
    if (res == CR_SUCCESS) return true;

    DEVINST parentInst;
    if (CM_Get_Parent(&parentInst, devInst, 0) == CR_SUCCESS) {
        return AttemptEject(parentInst);
    }
    return false;
}

void EjectDevice(const std::string& id) {
    if (lockedDevices.find(id) != lockedDevices.end()) {
        UnlockDevice(id);
    }

    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DISK, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) return;

    SP_DEVINFO_DATA spDevInfoData;
    spDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    bool found = false;

    for (int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &spDevInfoData); i++) {
        char buf[1024];
        if (CM_Get_Device_IDA(spDevInfoData.DevInst, buf, 1024, 0) == CR_SUCCESS) {
            if (IdsEqual(id, std::string(buf))) {
                found = true;
                SendLog("Device found. Attempting eject...");
                if (AttemptEject(spDevInfoData.DevInst)) {
                    lastSafeRemovalRequestTime = std::chrono::steady_clock::now();
                } else {
                    SendLog("Ejection Failed. Device is busy.", "error");
                }
                break;
            }
        }
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);

    if (!found) {
        SendLog("Device ID not found in list.", "error");
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DEVICECHANGE) {
        auto now = std::chrono::steady_clock::now();
        auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLogTime).count();
        if (wParam == lastLogWParam && timeDiff < 500) {
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        bool shouldUpdateList = false;

        switch (wParam) {
            case DBT_DEVICEARRIVAL:
                SendLog("Event: Device Inserted.", "success");
                shouldUpdateList = true;
                break;
                
            case DBT_DEVICEQUERYREMOVE:
                lastSafeRemovalRequestTime = now;
                if (!lockedDevices.empty()) {
                    SendLog("Windows requesting removal (Device Locked)...", "warning");
                } else {
                    SendLog("Windows requesting removal...", "normal");
                }
                break;

            case DBT_DEVICEQUERYREMOVEFAILED:
                lastSafeRemovalRequestTime = std::chrono::steady_clock::time_point::min();
                SendLog("Safe Removal DENIED by System.", "error");
                break;

            case DBT_DEVICEREMOVECOMPLETE:
                {
                    auto removeDiff = std::chrono::duration_cast<std::chrono::seconds>(now - lastSafeRemovalRequestTime).count();
                    
                    if (removeDiff < 3) {
                        SendLog("Event: Device Removed SAFELY.", "success");
                    } else {
                        SendLog("Event: Device Removed UNSAFELY.", "warning");
                    }
                    shouldUpdateList = true;
                }
                break;
        }

        if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE || wParam == DBT_DEVICEQUERYREMOVE) {
            lastLogTime = now;
            lastLogWParam = wParam;
        }

        if (shouldUpdateList) {
            ListDevices();
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void WindowThread() {
    WNDCLASSEX wx = {};
    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = WndProc;
    wx.lpszClassName = "USBMonitorClass";
    RegisterClassEx(&wx);
    HWND hwnd = CreateWindowEx(0, "USBMonitorClass", "USB Monitor", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);

    DEV_BROADCAST_DEVICEINTERFACE notificationFilter = {};
    notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    
    notificationFilter.dbcc_classguid = GUID_DEVINTERFACE_MOUSE;
    RegisterDeviceNotification(hwnd, &notificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

    notificationFilter.dbcc_classguid = GUID_DEVINTERFACE_DISK;
    RegisterDeviceNotification(hwnd, &notificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
}

int main() {
    std::thread wThread(WindowThread);
    wThread.detach();
    ListDevices();

    std::string input;
    while (std::getline(std::cin, input)) {
        size_t first = input.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = input.find_last_not_of(" \t\r\n");
        input = input.substr(first, (last - first + 1));

        if (input == "refresh") ListDevices();
        else if (input.find("lock|") == 0) LockDevice(input.substr(5));
        else if (input.find("unlock|") == 0) UnlockDevice(input.substr(7));
        else if (input.find("eject|") == 0) EjectDevice(input.substr(6));
    }
    return 0;
}