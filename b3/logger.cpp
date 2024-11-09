

#include "logger.h"

#ifndef LOGGING_DISABLED

#include <pthread.h>
#include <stdio.h>

#include <chrono>       
#include <ctime>
#include <cstdarg>


// color codes from here: https://gist.github.com/RabaDabaDoba/145049536f815903c79944599c6f952a

//Regular text
#define BLK "\e[0;30m"
#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define BLU "\e[0;34m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"

//Regular bold text
#define BBLK "\e[1;30m"
#define BRED "\e[1;31m"
#define BGRN "\e[1;32m"
#define BYEL "\e[1;33m"
#define BBLU "\e[1;34m"
#define BMAG "\e[1;35m"
#define BCYN "\e[1;36m"
#define BWHT "\e[1;37m"

//Regular underline text
#define UBLK "\e[4;30m"
#define URED "\e[4;31m"
#define UGRN "\e[4;32m"
#define UYEL "\e[4;33m"
#define UBLU "\e[4;34m"
#define UMAG "\e[4;35m"
#define UCYN "\e[4;36m"
#define UWHT "\e[4;37m"

//Regular background
#define BLKB "\e[40m"
#define REDB "\e[41m"
#define GRNB "\e[42m"
#define YELB "\e[43m"
#define BLUB "\e[44m"
#define MAGB "\e[45m"
#define CYNB "\e[46m"
#define WHTB "\e[47m"

//High intensty background 
#define BLKHB "\e[0;100m"
#define REDHB "\e[0;101m"
#define GRNHB "\e[0;102m"
#define YELHB "\e[0;103m"
#define BLUHB "\e[0;104m"
#define MAGHB "\e[0;105m"
#define CYNHB "\e[0;106m"
#define WHTHB "\e[0;107m"

//High intensty text
#define HBLK "\e[0;90m"
#define HRED "\e[0;91m"
#define HGRN "\e[0;92m"
#define HYEL "\e[0;93m"
#define HBLU "\e[0;94m"
#define HMAG "\e[0;95m"
#define HCYN "\e[0;96m"
#define HWHT "\e[0;97m"

//Bold high intensity text
#define BHBLK "\e[1;90m"
#define BHRED "\e[1;91m"
#define BHGRN "\e[1;92m"
#define BHYEL "\e[1;93m"
#define BHBLU "\e[1;94m"
#define BHMAG "\e[1;95m"
#define BHCYN "\e[1;96m"
#define BHWHT "\e[1;97m"

//Reset
#define reset "\e[0m"
#define CRESET "\e[0m"
#define COLOR_RESET "\e[0m"



bool _logger::g_log_verbose = false;

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
    // Get the current time as a time_point
    auto now = std::chrono::system_clock::now();

    // Convert to time_t for strftime formatting
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm *now_tm = std::localtime(&now_time_t);

    // Get milliseconds from the time_point
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    pthread_mutex_lock(&g_logMutex);
    char buffer[80];
    std::strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", now_tm);
    printf("[%s.%03d] ", buffer, static_cast<int>(ms.count()));
    va_list args;
    va_start(args, message);
    switch (level) {
    case DEBUG:
        printf("" BBLU "[DEBUG]" reset " ");
        break;
    case INFO:
        printf("" BGRN "[INFO]" reset " ");
        break;
    case WARNING:
        printf("" BYEL "[WARNING]" reset " ");
        break;
    case ERROR:
        printf("" BRED "[ERROR]" reset " ");
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
