#include <iostream>
#include <fstream>
#include <iomanip>
#include <windows.h>
#include <stdio.h>

static inline void outpd(unsigned short port, unsigned int value) {
    __asm__ __volatile__("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned int inpd(unsigned short port) {
    unsigned int value;
    __asm__ __volatile__("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

int main() {
    HANDLE hGiveIo = CreateFile("\\\\.\\giveio", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hGiveIo == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Could not open handle to GiveIO driver." << std::endl;
        std::cerr << "Ensure the 'giveio' service is started (run 'sc start giveio' as Admin)." <<
        system("pause");
        return 1;
    }

    std::ofstream outFile("Z:\\pci_devices.txt");
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not create output file 'Z:\\pci_devices.txt'." << std::endl;
        CloseHandle(hGiveIo);
        system("pause");
        return 1;
    }

    outFile << "{\n";
    outFile << "  \"devices\": [\n";
    
    int deviceCount = 0;
    for (DWORD bus = 0; bus < 256; ++bus) {
        for (DWORD device = 0; device < 32; ++device) {
            bool isMultiFunctionDevice = false;

            for (DWORD function = 0; function < 8; ++function) {
                
                DWORD address = 0x80000000 | (bus << 16) | (device << 11) | (function << 8);
                
                outpd(PCI_CONFIG_ADDRESS, address);
                DWORD value = inpd(PCI_CONFIG_DATA);
                
                if (value != 0xFFFFFFFF && value != 0x00000000) {
                    if (function == 0) {
                        outpd(PCI_CONFIG_ADDRESS, address + 0x0C);
                        DWORD headerValue = inpd(PCI_CONFIG_DATA);
                        if ((headerValue & 0x00800000) != 0) {
                            isMultiFunctionDevice = true;
                        }
                    }

                    WORD vendorID = (WORD)(value & 0xFFFF);
                    WORD deviceID = (WORD)(value >> 16);
                
                    if (deviceCount > 0) {
                        outFile << ",\n";
                    }
                    
                    outFile << "    {";
                    outFile << "\"DeviceID\": \"0x" << std::hex << std::setfill('0') << std::setw(4) << deviceID << "\", ";
                    outFile << "\"VendorID\": \"0x" << std::hex << std::setfill('0') << std::setw(4) << vendorID << "\"";
                    outFile << "}";

                    deviceCount++;
                    
                    if (!isMultiFunctionDevice) {
                        break;
                    }

                } else if (function == 0) {
                    break;
                }
            }
        }
    }

    outFile << "\n  ]\n}\n";

    std::cout << "Success! Wrote data for " << deviceCount << " devices to Z:\\pci_devices.txt" << std::endl;

    CloseHandle(hGiveIo);
    Sleep(3000);
    return 0;
}