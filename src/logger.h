#pragma once



#define LOG(LEVEL, MESSAGE, ...) b3::log(LEVEL, MESSAGE, ##__VA_ARGS__)
#define DEBUG(MESSAGE, ...) LOG(b3::DEBUG, MESSAGE, ##__VA_ARGS__)
#define INFO(MESSAGE, ...) LOG(b3::INFO, MESSAGE, ##__VA_ARGS__)
#define WARNING(MESSAGE, ...) LOG(b3::WARNING, MESSAGE, ##__VA_ARGS__)
#define ERROR(MESSAGE, ...) LOG(b3::ERROR, MESSAGE, ##__VA_ARGS__)


namespace b3 {

    bool log_verbose = false;

    enum LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    void log(LogLevel level, const char *message, ...);


}; // namespace b3