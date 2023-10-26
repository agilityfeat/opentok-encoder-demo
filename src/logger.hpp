#ifndef OPENTOK_ENCODER_LOGGER_HPP
#define OPENTOK_ENCODER_LOGGER_HPP

#include <string>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    explicit Logger(std::string tag, LogLevel logLevel = LogLevel::Debug);

    void setLogLevel(LogLevel level);

    template<typename... Args>
    void log(LogLevel level, const std::string &format, const Args &... args);

    template<typename... Args>
    void debug(const std::string &format, const Args &... args);

    template<typename... Args>
    void warn(const std::string &format, const Args &... args);

    template<typename... Args>
    void error(const std::string &format, const Args &... args);

private:
    std::string tag;
    LogLevel logLevel;

    void outputLog(LogLevel level, const std::string &logString);

    static std::string utcTime();
};

#endif //OPENTOK_ENCODER_LOGGER_HPP
