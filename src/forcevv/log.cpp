#include "forcevv/log.hpp"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <string_view>

namespace forcevv::log {
namespace {

SRWLOCK g_srwLock = SRWLOCK_INIT;
HANDLE g_fileHandle = INVALID_HANDLE_VALUE;
bool g_consoleAttached = false;

struct ScopedLock {
    ScopedLock() { AcquireSRWLockExclusive(&g_srwLock); }
    ~ScopedLock() { ReleaseSRWLockExclusive(&g_srwLock); }
};

const char* levelName(Level level) {
    switch (level) {
    case Level::Info:
        return "INFO";
    case Level::Warn:
        return "WARN";
    case Level::Error:
        return "ERROR";
    }
    return "LOG";
}

void defaultLogPath(char* outPath, DWORD outSize) {
    const DWORD length = GetTempPathA(outSize, outPath);
    if (length == 0 || length >= outSize - 30) {
        strncpy_s(outPath, outSize, "vibrant-visuals-patcher.log", _TRUNCATE);
        return;
    }
    strncat_s(outPath, outSize, "vibrant-visuals-patcher.log", _TRUNCATE);
}

void openConsole() {
#if defined(VVP_ENABLE_CONSOLE)
    if (g_consoleAttached) {
        return;
    }

    if (AllocConsole() == FALSE) {
        return;
    }

    FILE* ignored{};
    freopen_s(&ignored, "CONOUT$", "w", stdout);
    freopen_s(&ignored, "CONOUT$", "w", stderr);
    SetConsoleTitleW(L"vibrant-visuals-patcher");
    g_consoleAttached = true;
#endif
}

void writeToConsoleHandle(Level level, const char* buffer, DWORD length) {
    const DWORD handleId = level == Level::Error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
    HANDLE handle = GetStdHandle(handleId);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD ignored{};
    WriteFile(handle, buffer, length, &ignored, nullptr);
}

void writeUnlocked(Level level, std::string_view message) {
    char visibleLine[1024];
    int visLen = snprintf(visibleLine, sizeof(visibleLine), "[%s] %.*s\n",
                          levelName(level), static_cast<int>(message.size()), message.data());
    if (visLen > 0) {
        OutputDebugStringA(visibleLine);
        writeToConsoleHandle(level, visibleLine, static_cast<DWORD>(visLen));
    }

    if (g_fileHandle != INVALID_HANDLE_VALUE) {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        char fileLine[1024];
        int fileLen = snprintf(fileLine, sizeof(fileLine), "[%04d-%02d-%02d %02d:%02d:%02d][%s] %.*s\n",
                               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                               levelName(level), static_cast<int>(message.size()), message.data());
        if (fileLen > 0) {
            DWORD written{};
            WriteFile(g_fileHandle, fileLine, static_cast<DWORD>(fileLen), &written, nullptr);
        }
    }
}

}

void initialize() {
    ScopedLock lock;

    if (g_fileHandle != INVALID_HANDLE_VALUE) {
        return;
    }

    openConsole();

    char path[MAX_PATH]{};
    defaultLogPath(path, static_cast<DWORD>(sizeof(path)));

    g_fileHandle = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
}

void shutdown() {
    ScopedLock lock;

    if (g_fileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(g_fileHandle);
        g_fileHandle = INVALID_HANDLE_VALUE;
    }
}

void write(Level level, std::string_view message) {
    ScopedLock lock;
    writeUnlocked(level, message);
}

void info(std::string_view message) {
    write(Level::Info, message);
}

void warn(std::string_view message) {
    write(Level::Warn, message);
}

void error(std::string_view message) {
    write(Level::Error, message);
}

void writeMultiline(Level level, std::string_view message) {
    std::string_view remaining = message;

    while (!remaining.empty()) {
        const std::size_t pos = remaining.find('\n');
        std::string_view line = (pos == std::string_view::npos) ? remaining : remaining.substr(0, pos);

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        if (!line.empty()) {
            write(level, line);
        }

        if (pos == std::string_view::npos) {
            break;
        }

        remaining.remove_prefix(pos + 1);
    }
}

}
