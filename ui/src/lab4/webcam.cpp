#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

std::atomic<bool> isRunning(true);
std::atomic<bool> isHidden(false);

class WebcamCapture {
public:
    WebcamCapture() {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (SUCCEEDED(hr)) {
            MFStartup(MF_VERSION);
        }
    }

    ~WebcamCapture() {
        MFShutdown();
        CoUninitialize();
    }

    void outputJSON(const std::string& json) {
        std::cout << json << std::endl;
        std::cout.flush();
    }

    void outputError(const std::string& context, HRESULT hr) {
        std::ostringstream ss;
        ss << "{\"type\":\"status\",\"message\":\"Error in " << context 
           << " (Code: " << std::hex << hr << ")\",\"error\":true}";
        outputJSON(ss.str());
    }

    static std::string escapeJSON(const std::string& s) {
        std::ostringstream o;
        for (char c : s) {
            switch (c) {
            case '\"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:   o << c; break;
            }
        }
        return o.str();
    }

    std::string getCameraInfo() {
        IMFAttributes* pConfig = nullptr;
        IMFActivate** ppDevices = nullptr;
        UINT32 count = 0;
        std::string name = "No camera found";
        std::string status = "Not Available";
        std::string resolution = "Checking...";
        std::string fps = "Checking...";

        if (SUCCEEDED(MFCreateAttributes(&pConfig, 1))) {
            pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
            if (SUCCEEDED(MFEnumDeviceSources(pConfig, &ppDevices, &count)) && count > 0) {
                WCHAR* szFriendlyName = nullptr;
                UINT32 cchName;
                if (SUCCEEDED(ppDevices[0]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &szFriendlyName, &cchName))) {
                    char buffer[512] = {0};
                    WideCharToMultiByte(CP_UTF8, 0, szFriendlyName, -1, buffer, sizeof(buffer) - 1, NULL, NULL);
                    name = buffer;
                    CoTaskMemFree(szFriendlyName);
                    status = "Available";
                    
                    IMFMediaSource* pSrc = nullptr;
                    IMFSourceReader* pRdr = nullptr;
                    if (SUCCEEDED(ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSrc)))) {
                        if (SUCCEEDED(MFCreateSourceReaderFromMediaSource(pSrc, NULL, &pRdr))) {
                            IMFMediaType* pMT = nullptr;
                            if (SUCCEEDED(pRdr->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pMT))) {
                                UINT32 w=0, h=0;
                                MFGetAttributeSize(pMT, MF_MT_FRAME_SIZE, &w, &h);
                                resolution = std::to_string(w) + "x" + std::to_string(h);

                                UINT32 num=0, den=1;
                                MFGetAttributeRatio(pMT, MF_MT_FRAME_RATE, &num, &den);
                                if (den != 0) fps = std::to_string(num / den);

                                pMT->Release();
                            }
                            pRdr->Release();
                        }
                        pSrc->Release();
                    }
                }
            }
        }

        SAFE_RELEASE_ARRAY(ppDevices, count);
        SAFE_RELEASE(pConfig);

        std::ostringstream json;
        json << "{\"type\":\"camera_info\",\"name\":\"" << escapeJSON(name)
             << "\",\"status\":\"" << status
             << "\",\"resolution\":\"" << resolution
             << "\",\"fps\":\"" << fps << "\"}";
        return json.str();
    }

    bool capturePhotoBMP(const std::string& filename) {
        IMFAttributes* pConfig = nullptr;
        IMFAttributes* pReaderConfig = nullptr;
        IMFActivate** ppDevices = nullptr;
        UINT32 count = 0;
        IMFMediaSource* pSource = nullptr;
        IMFSourceReader* pReader = nullptr;
        IMFMediaType* pType = nullptr;
        IMFMediaType* pNativeType = nullptr;
        IMFSample* pSample = nullptr;
        IMFMediaBuffer* pBuffer = nullptr;
        BYTE* pData = nullptr;
        
        DWORD flags = 0;
        LONGLONG llTimeStamp = 0;
        DWORD maxLen = 0, curLen = 0;
        UINT32 width = 640, height = 480;
        DWORD imageSize = 0;
        
        BITMAPFILEHEADER bf = {0};
        BITMAPINFOHEADER bi = {0};
        
        bool ok = false;
        HRESULT hr = S_OK;

        hr = MFCreateAttributes(&pConfig, 1);
        if (FAILED(hr)) { outputError("MFCreateAttributes", hr); goto done; }
        
        hr = pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        if (FAILED(hr)) { outputError("SetGUID", hr); goto done; }

        hr = MFEnumDeviceSources(pConfig, &ppDevices, &count);
        if (FAILED(hr) || count == 0) { outputError("MFEnumDeviceSources", hr); goto done; }

        hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
        if (FAILED(hr)) { outputError("ActivateObject", hr); goto done; }

        hr = MFCreateAttributes(&pReaderConfig, 1);
        if (FAILED(hr)) { outputError("CreateAttributes(Reader)", hr); goto done; }
        
        hr = pReaderConfig->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        if (FAILED(hr)) { outputError("Set EnableVideoProcessing", hr); goto done; }

        hr = MFCreateSourceReaderFromMediaSource(pSource, pReaderConfig, &pReader);
        if (FAILED(hr)) { outputError("CreateSourceReader", hr); goto done; }

        hr = pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &pNativeType);
        if (SUCCEEDED(hr)) {
            MFGetAttributeSize(pNativeType, MF_MT_FRAME_SIZE, &width, &height);
            SAFE_RELEASE(pNativeType);
        }

        hr = MFCreateMediaType(&pType);
        if (FAILED(hr)) goto done;
        hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (FAILED(hr)) goto done;
        hr = pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        if (FAILED(hr)) goto done;
        
        hr = MFSetAttributeSize(pType, MF_MT_FRAME_SIZE, width, height);
        if (FAILED(hr)) goto done;

        hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
        if (FAILED(hr)) { outputError("SetCurrentMediaType", hr); goto done; }

        for (int i = 0; i < 20; ++i) {
            if (pSample) { pSample->Release(); pSample = nullptr; }
            
            hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &flags, &llTimeStamp, &pSample);
            
            if (FAILED(hr)) { outputError("ReadSample", hr); goto done; }
            
            if (flags & MF_SOURCE_READERF_STREAMTICK) continue;
            if (!pSample) continue;

            if (i > 5) break; 
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (!pSample) {
            outputJSON("{\"type\":\"status\",\"message\":\"Timeout: Camera sent no data.\",\"error\":true}");
            goto done; 
        }

        hr = pSample->ConvertToContiguousBuffer(&pBuffer);
        if (FAILED(hr)) { outputError("ConvertToContiguousBuffer", hr); goto done; }

        hr = pBuffer->Lock(&pData, &maxLen, &curLen);
        if (FAILED(hr)) { outputError("Buffer Lock", hr); goto done; }

        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = (LONG)width;
        bi.biHeight = (LONG)height * -1;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        imageSize = curLen;
        bf.bfType = 0x4D42;
        bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bf.bfSize = bf.bfOffBits + imageSize;

        {
            std::ofstream file(filename, std::ios::binary);
            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(&bf), sizeof(bf));
                file.write(reinterpret_cast<const char*>(&bi), sizeof(bi));
                file.write(reinterpret_cast<const char*>(pData), imageSize);
                file.close();
                ok = true;
            } else {
                outputJSON("{\"type\":\"status\",\"message\":\"Cannot open file for writing.\",\"error\":true}");
            }
        }

        pBuffer->Unlock();

    done:
        SAFE_RELEASE(pBuffer);
        SAFE_RELEASE(pSample);
        SAFE_RELEASE(pType);
        SAFE_RELEASE(pReader);
        SAFE_RELEASE(pReaderConfig);
        SAFE_RELEASE(pSource);
        SAFE_RELEASE_ARRAY(ppDevices, count);
        SAFE_RELEASE(pConfig);

        if (ok) {
            std::ostringstream json;
            json << "{\"type\":\"file_saved\",\"filename\":\"" << escapeJSON(filename) << "\"}";
            outputJSON(json.str());
        }
        return ok;
    }

    bool captureVideoMP4(const std::string& filename, int durationSeconds = 5) {
        IMFAttributes* pConfig = nullptr;
        IMFActivate** ppDevices = nullptr;
        UINT32 count = 0;
        IMFMediaSource* pSource = nullptr;
        IMFSourceReader* pReader = nullptr;
        IMFSinkWriter* pWriter = nullptr;
        IMFMediaType* pNativeType = nullptr;
        IMFMediaType* pReaderType = nullptr;
        IMFMediaType* pOutType = nullptr;
        IMFMediaType* pInType = nullptr;
        IMFSample* pSample = nullptr;
        
        DWORD streamIndex = 0;
        UINT32 width = 640, height = 480;
        UINT32 num = 30, den = 1;
        DWORD flags = 0;
        LONGLONG ts = 0;
        LONGLONG startTs = -1;
        std::chrono::steady_clock::time_point deadline;
        
        bool ok = false;

        if (FAILED(MFCreateAttributes(&pConfig, 1))) goto done;
        if (FAILED(pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) goto done;
        if (FAILED(MFEnumDeviceSources(pConfig, &ppDevices, &count)) || count == 0) goto done;
        if (FAILED(ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource)))) goto done;
        if (FAILED(MFCreateSourceReaderFromMediaSource(pSource, NULL, &pReader))) goto done;

        if (FAILED(pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &pNativeType))) goto done;

        MFGetAttributeSize(pNativeType, MF_MT_FRAME_SIZE, &width, &height);
        MFGetAttributeRatio(pNativeType, MF_MT_FRAME_RATE, &num, &den);
        SAFE_RELEASE(pNativeType);

        if (FAILED(MFCreateMediaType(&pReaderType))) goto done;
        pReaderType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pReaderType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pReaderType);
        SAFE_RELEASE(pReaderType);

        if (FAILED(MFCreateSinkWriterFromURL(std::wstring(filename.begin(), filename.end()).c_str(), NULL, NULL, &pWriter))) goto done;

        if (FAILED(MFCreateMediaType(&pOutType))) goto done;
        pOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        pOutType->SetUINT32(MF_MT_AVG_BITRATE, 4000000);
        pOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(pOutType, MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(pOutType, MF_MT_FRAME_RATE, num, den);
        MFSetAttributeRatio(pOutType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        if (FAILED(pWriter->AddStream(pOutType, &streamIndex))) { SAFE_RELEASE(pOutType); goto done; }
        SAFE_RELEASE(pOutType);

        if (FAILED(MFCreateMediaType(&pInType))) goto done;
        pInType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pInType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        MFSetAttributeSize(pInType, MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(pInType, MF_MT_FRAME_RATE, num, den);
        MFSetAttributeRatio(pInType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        if (FAILED(pWriter->SetInputMediaType(streamIndex, pInType, NULL))) { SAFE_RELEASE(pInType); goto done; }
        SAFE_RELEASE(pInType);

        if (FAILED(pWriter->BeginWriting())) goto done;

        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);

        while (std::chrono::steady_clock::now() < deadline) {
            pSample = nullptr;
            flags = 0;
            ts = 0;
            
            if (FAILED(pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &flags, &ts, &pSample))) break;
            if (flags & MF_SOURCE_READERF_STREAMTICK) continue;
            if (!pSample) continue;

            if (startTs == -1) startTs = ts;

            pSample->SetSampleTime(ts - startTs);

            if (FAILED(pWriter->WriteSample(streamIndex, pSample))) { SAFE_RELEASE(pSample); break; }
            SAFE_RELEASE(pSample);
        }

        if (FAILED(pWriter->Finalize())) goto done;
        ok = true;

    done:
        SAFE_RELEASE(pWriter);
        SAFE_RELEASE(pReader);
        SAFE_RELEASE(pSource);
        SAFE_RELEASE_ARRAY(ppDevices, count);
        SAFE_RELEASE(pConfig);

        if (ok) {
            std::ostringstream json;
            json << "{\"type\":\"file_saved\",\"filename\":\"" << escapeJSON(filename) << "\"}";
            outputJSON(json.str());
        } else {
            outputJSON("{\"type\":\"status\",\"message\":\"Failed to capture video\",\"error\":true}");
        }
        return ok;
    }

private:
    template <typename T>
    static void SAFE_RELEASE(T*& p) {
        if (p) { p->Release(); p = nullptr; }
    }
    template <typename T>
    static void SAFE_RELEASE_ARRAY(T**& pArray, UINT32& count) {
        if (pArray) {
            for (UINT32 i = 0; i < count; ++i) {
                if (pArray[i]) pArray[i]->Release();
            }
            CoTaskMemFree(pArray);
            pArray = nullptr;
            count = 0;
        }
    }
};

void toggleStealthMode() {
    HWND hwndElectron = FindWindowA(NULL, "LAB4: WEBCAM");
    HWND hwndConsole = GetConsoleWindow();

    bool shouldHide = !isHidden;
    int cmd = shouldHide ? SW_HIDE : SW_SHOW;

    if (hwndElectron) ShowWindow(hwndElectron, cmd);
    if (hwndConsole) ShowWindow(hwndConsole, cmd);

    isHidden = shouldHide;
}

void generateFilename(char* buffer, size_t size, const char* prefix, const char* ext) {
    CreateDirectoryA("captures", NULL);
    SYSTEMTIME st; GetLocalTime(&st);
    sprintf_s(buffer, size, "captures\\%s_%04d%02d%02d_%02d%02d%02d.%s",
        prefix, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, ext);
}

void keyListenerThread(WebcamCapture* cam) {
    while (isRunning) {
        if (GetAsyncKeyState(VK_F8) & 0x0001) toggleStealthMode();

        if (GetAsyncKeyState(VK_F9) & 0x0001) {
            if (!isHidden) toggleStealthMode();
            char filename[256];
            generateFilename(filename, 256, "hidden_photo", "bmp");
            cam->capturePhotoBMP(filename);
        }

        if (GetAsyncKeyState(VK_F10) & 0x0001) {
            if (!isHidden) toggleStealthMode();
            char filename[256];
            generateFilename(filename, 256, "hidden_video", "mp4");
            cam->captureVideoMP4(filename, 5);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    WebcamCapture webcam;
    std::cout << webcam.getCameraInfo() << std::endl;
    std::thread listener(keyListenerThread, &webcam);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "refresh_info") {
            std::cout << webcam.getCameraInfo() << std::endl;
        } else if (line == "capture_photo") {
            char filename[256];
            generateFilename(filename, 256, "photo", "bmp");
            webcam.capturePhotoBMP(filename);
        } else if (line == "hidden_photo") {
            toggleStealthMode();
            char filename[256];
            generateFilename(filename, 256, "hidden_photo", "bmp");
            webcam.capturePhotoBMP(filename);
            toggleStealthMode();
        } else if (line == "capture_video") {
            char filename[256];
            generateFilename(filename, 256, "video", "mp4");
            webcam.captureVideoMP4(filename, 5);
        } else if (line == "hidden_video") {
            toggleStealthMode();
            char filename[256];
            generateFilename(filename, 256, "hidden_video", "mp4");
            webcam.captureVideoMP4(filename, 5);
            toggleStealthMode();
        } else if (line == "exit" || line == "quit") {
            isRunning = false;
            break;
        }
    }

    if (listener.joinable()) listener.join();
    return 0;
}