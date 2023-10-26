#include "logger.hpp"
#include <fmt/format.h>
#include <iostream>
#include <sstream>

Logger::Logger(std::string tag, LogLevel logLevel) : tag(std::move(tag)), logLevel(logLevel) {}

void Logger::setLogLevel(LogLevel level) {
    logLevel = level;
}

template<typename... Args>
void Logger::log(LogLevel level, const std::string &format, const Args &... args) {
    if (level >= logLevel) {
        std::string logString = fmt::format(format, args...);
        outputLog(level, logString);
    }
}

template<typename... Args>
void Logger::debug(const std::string &format, const Args &... args) {
    log(LogLevel::Debug, format, args...);
}

template<typename... Args>
void Logger::warn(const std::string &format, const Args &... args) {
    log(LogLevel::Warning, format, args...);
}

template<typename... Args>
void Logger::error(const std::string &format, const Args &... args) {
    log(LogLevel::Error, format, args...);
}

void Logger::outputLog(LogLevel level, const std::string &logString) {
    std::string levelString;
    switch (level) {
        case LogLevel::Debug:
            levelString = "DEBUG";
            break;
        case LogLevel::Info:
            levelString = "INFO";
            break;
        case LogLevel::Warning:
            levelString = "WARNING";
            break;
        case LogLevel::Error:
            levelString = "ERROR";
            break;
    }

    std::cout << utcTime() << " [" << levelString << "] " << "(" << tag << ") " << logString << std::endl;
}

std::string Logger::utcTime() {
    time_t t = time({});
    char timeString[std::size("yyyy-mm-ddThh:mm:ss.SSSZ")];
    strftime(std::data(timeString), std::size(timeString),
             "%FT%TZ", gmtime(&t));
    std::stringstream ss;
    ss << timeString;
    return ss.str();
}
