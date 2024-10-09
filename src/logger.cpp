
#include "logger.h"

#include <pthread.h>
#include <stdio.h>

#include <chrono>
#include <cstdarg>


using namespace b3;

void b3::log(LogLevel level, const char *message, ...)
{
    if (!log_verbose && level == DEBUG) {
        return;
    }
    std::time_t now = std::time(nullptr);
    std::tm *now_tm = std::localtime(&now);

    char buffer[80];
    std::strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", now_tm);
    printf("[%s] ", buffer);
    va_list args;
    va_start(args, message);
    switch (level) {
    case DEBUG:
        printf("[DEBUG] ");
        break;
    case INFO:
        printf("[INFO] ");
        break;
    case WARNING:
        printf("[WARNING] ");
        break;
    case ERROR:
        printf("[ERROR] ");
        break;
    }


    vprintf(message, args);

    va_end(args);
}