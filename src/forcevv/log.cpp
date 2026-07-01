#include "forcevv/log.hpp"

#include <Windows.h>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace forcevv::log {
namespace {

std::mutex g_mutex;
std::ofstream g_file;
bool g_consoleAttached = false;

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

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm local{};
    localtime_s(&local, &time);

    std::ostringstream stream;
    stream << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string defaultLogPath() {
    char tempPath[MAX_PATH]{};
    const DWORD length = GetTempPathA(static_cast<DWORD>(sizeof(tempPath)), tempPath);
    if (length == 0 || length >= sizeof(tempPath)) {
        return "vibrant-visuals-patcher.log";
    }

    std::string path(tempPath);
    path += "vibrant-visuals-patcher.log";
    return path;
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

void writeToConsoleHandle(Level level, const std::string& line) {
    const DWORD handleId = level == Level::Error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
    HANDLE handle = GetStdHandle(handleId);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD ignored{};
    WriteFile(handle, line.data(), static_cast<DWORD>(line.size()), &ignored, nullptr);
}

void writeUnlocked(Level level, std::string_view message) {
    const std::string visibleLine = "[" + std::string(levelName(level)) + "] " + std::string(message) + "\n";
    const std::string fileLine = "[" + timestamp() + "][" + levelName(level) + "] " + std::string(message) + "\n";

    OutputDebugStringA(visibleLine.c_str());
    writeToConsoleHandle(level, visibleLine);

    if (g_file.is_open()) {
        g_file << fileLine;
        g_file.flush();
    }
}

}

void initialize() {
    std::lock_guard lock(g_mutex);

    if (g_file.is_open()) {
        return;
    }

    openConsole();

    const std::string path = defaultLogPath();
    g_file.open(path, std::ios::out | std::ios::app);
}

void shutdown() {
    std::lock_guard lock(g_mutex);

    if (g_file.is_open()) {
        g_file.flush();
        g_file.close();
    }
}

void write(Level level, std::string_view message) {
    std::lock_guard lock(g_mutex);
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
    std::string current;

    for (const char ch : message) {
        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            if (!current.empty()) {
                write(level, current);
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty()) {
        write(level, current);
    }
}

}
