#pragma once

#include <string_view>

namespace forcevv::log {

enum class Level {
    Info,
    Warn,
    Error,
};

void initialize();
void shutdown();

void write(Level level, std::string_view message);
void writeMultiline(Level level, std::string_view message);

void info(std::string_view message);
void warn(std::string_view message);
void error(std::string_view message);

}
