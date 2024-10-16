#pragma once


#ifndef LOGGING_DISABLED
# define LOG_V(LEVEL, MESSAGE, ...) _logger::log(LEVEL, __FILE__, __LINE__, __func__, MESSAGE, ##__VA_ARGS__)
# define LOG(LEVEL, MESSAGE, ...) _logger::log(LEVEL,"",0,"", MESSAGE, ##__VA_ARGS__)
# define DEBUG(MESSAGE, ...) LOG(_logger::LogLevel::DEBUG, MESSAGE, ##__VA_ARGS__)
# define INFO(MESSAGE, ...) LOG(_logger::LogLevel::INFO, MESSAGE, ##__VA_ARGS__)
# define WARNING(MESSAGE, ...) LOG(_logger::LogLevel::WARNING, MESSAGE, ##__VA_ARGS__)
# define ERROR(MESSAGE, ...) LOG_V(_logger::LogLevel::ERROR, MESSAGE, ##__VA_ARGS__)

#define SET_VERBOSE_LOGGING(VERBOSE) _logger::g_log_verbose = VERBOSE;
namespace _logger {

    extern bool g_log_verbose;

    enum LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    void log(LogLevel level, const char *file, int line, const char *func, const char *message, ...);

}; // namespace b3

#else
# define DEBUG(MESSAGE, ...)
# define INFO(MESSAGE, ...)
# define WARNING(MESSAGE, ...)
# define ERROR(MESSAGE, ...)

#define SET_VERBOSE_LOGGING(VERBOSE)
#endif

