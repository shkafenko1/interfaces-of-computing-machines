#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <windows.h>
#include <powrprof.h>
#include <Setupapi.h>
#include <initguid.h>
#include <Devguid.h>
#include <winioctl.h>

#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "setupapi.lib")

#define IOCTL_BATTERY_QUERY_TAG CTL_CODE(FILE_DEVICE_BATTERY, 0x10, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_BATTERY_QUERY_INFORMATION CTL_CODE(FILE_DEVICE_BATTERY, 0x11, METHOD_BUFFERED, FILE_READ_ACCESS)

typedef enum _BATTERY_QUERY_INFORMATION_LEVEL {
    BatteryInformation
} BATTERY_QUERY_INFORMATION_LEVEL;

typedef struct _BATTERY_QUERY_INFORMATION {
    ULONG BatteryTag;
    BATTERY_QUERY_INFORMATION_LEVEL InformationLevel;
    ULONG AtRate;
} BATTERY_QUERY_INFORMATION, *PBATTERY_QUERY_INFORMATION;

typedef struct _BATTERY_INFORMATION {
    ULONG Capabilities;
    UCHAR Technology;
    UCHAR Reserved[3];
    UCHAR Chemistry[4];
    ULONG DesignedCapacity;
    ULONG FullChargedCapacity;
    ULONG DefaultAlert1;
    ULONG DefaultAlert2;
    ULONG CriticalBias;
    ULONG CycleCount;
} BATTERY_INFORMATION, *PBATTERY_INFORMATION;

std::string getACLineStatusString(UCHAR status) {
    switch (status) {
        case 0: return "Offline";
        case 1: return "Online";
        default: return "Unknown";
    }
}

std::string getBatteryChemistry() {
    HDEVINFO hdev = SetupDiGetClassDevs(&GUID_DEVCLASS_BATTERY, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hdev == INVALID_HANDLE_VALUE) return "N/A";

    SP_DEVICE_INTERFACE_DATA did = {0};
    did.cbSize = sizeof(did);

    if (SetupDiEnumDeviceInterfaces(hdev, 0, &GUID_DEVCLASS_BATTERY, 0, &did)) {
        DWORD cbRequired = 0;
        SetupDiGetDeviceInterfaceDetail(hdev, &did, NULL, 0, &cbRequired, NULL);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            SetupDiDestroyDeviceInfoList(hdev); return "N/A";
        }

        PSP_DEVICE_INTERFACE_DETAIL_DATA pdidd = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, cbRequired);
        if (!pdidd) {
            SetupDiDestroyDeviceInfoList(hdev); return "N/A";
        }
        pdidd->cbSize = sizeof(*pdidd);

        if (SetupDiGetDeviceInterfaceDetail(hdev, &did, pdidd, cbRequired, NULL, NULL)) {
            HANDLE hBattery = CreateFile(pdidd->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hBattery != INVALID_HANDLE_VALUE) {
                BATTERY_QUERY_INFORMATION bqi = {0};
                DWORD dwOut;

                DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_TAG, NULL, 0, &bqi.BatteryTag, sizeof(bqi.BatteryTag), &dwOut, NULL);
                
                if (bqi.BatteryTag) {
                    BATTERY_INFORMATION bi = {0};
                    bqi.InformationLevel = BatteryInformation;
                    if (DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi), &bi, sizeof(bi), &dwOut, NULL)) {
                        char name[5] = {0};
                        memcpy(name, bi.Chemistry, 4);
                        CloseHandle(hBattery);
                        LocalFree(pdidd);
                        SetupDiDestroyDeviceInfoList(hdev);
                        return std::string(name);
                    }
                }
                CloseHandle(hBattery);
            }
        }
        LocalFree(pdidd);
    }
    SetupDiDestroyDeviceInfoList(hdev);
    return "N/A";
}

void listenForCommands() {
    std::string line;
    while (std::cin >> line) {
        if (line == "sleep") {
            SetSuspendState(FALSE, TRUE, TRUE);
        } else if (line == "hibernate") {
            SetSuspendState(TRUE, TRUE, TRUE);
        }
    }
}

int main() {
    std::thread command_thread(listenForCommands);
    command_thread.detach();

    while (true) {
        SYSTEM_POWER_STATUS sps;
        if (GetSystemPowerStatus(&sps)) {
            int batteryLifePercent = static_cast<int>(sps.BatteryLifePercent);
            if (batteryLifePercent > 100) {
                batteryLifePercent = 100;
            }

            std::cout << "{";
            std::cout << "\"powerSource\":\"" << getACLineStatusString(sps.ACLineStatus) << "\",";
            std::cout << "\"batteryType\":\"" << getBatteryChemistry() << "\",";
            std::cout << "\"batteryLevel\":" << batteryLifePercent << ",";
            std::cout << "\"batteryLifeTime\":" << sps.BatteryLifeTime << ",";
            std::cout << "\"batteryFullLifeTime\":" << sps.BatteryFullLifeTime;
            std::cout << "}" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return 0;
}