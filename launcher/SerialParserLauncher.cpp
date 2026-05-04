#include <windows.h>

#include <string>
#include <vector>

static std::wstring parentDirectory(const std::wstring &path)
{
    const size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return L".";
    }
    return path.substr(0, pos);
}

static bool fileExists(const std::wstring &path)
{
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        MessageBoxW(nullptr, L"Cannot locate SerialParser launcher path.", L"SerialParser", MB_ICONERROR | MB_OK);
        return 1;
    }

    const std::wstring rootDir = parentDirectory(modulePath);
    std::wstring appExe = rootDir + L"\\app\\SerialParserApp.exe";
    std::wstring appDir = rootDir + L"\\app";

    if (!fileExists(appExe)) {
        appExe = rootDir + L"\\SerialParserApp.exe";
        appDir = rootDir;
    }

    if (!fileExists(appExe)) {
        MessageBoxW(nullptr,
                    L"SerialParserApp.exe was not found.\nPlease keep the release folder structure intact.",
                    L"SerialParser",
                    MB_ICONERROR | MB_OK);
        return 1;
    }

    std::wstring commandLine = L"\"" + appExe + L"\"";
    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};

    if (!CreateProcessW(appExe.c_str(),
                        commandBuffer.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        0,
                        nullptr,
                        appDir.c_str(),
                        &startupInfo,
                        &processInfo)) {
        MessageBoxW(nullptr, L"Failed to launch SerialParserApp.exe.", L"SerialParser", MB_ICONERROR | MB_OK);
        return 1;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return 0;
}
