

#include "logger.h"

#ifndef LOGGING_DISABLED

#include <pthread.h>
#include <stdio.h>

#include <chrono>
#include <cstdarg>


pthread_mutex_t g_logMutex;
bool g_mutexInitialized = false;

void _logger::log(LogLevel level, const char *file, int line, const char *func, const char *message, ...)
{
    if (!g_log_verbose && level == DEBUG) {
        return;
    }
    if (!g_mutexInitialized) {
        pthread_mutex_init(&g_logMutex, nullptr);
        g_mutexInitialized = true;
    }

    std::time_t now = std::time(nullptr);
    std::tm *now_tm = std::localtime(&now);

    pthread_mutex_lock(&g_logMutex);
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
        printf("%s:%d:%s ", file, line, func);
        break;
    case ERROR:
        printf("[ERROR] ");
        printf("%s:%d:%s ", file, line, func);
        break;
    }
    vprintf(message, args);
    printf("\n");

    fflush(stdout);
    pthread_mutex_unlock(&g_logMutex);

    va_end(args);
}
#endif  // LOGGING_DISABLED