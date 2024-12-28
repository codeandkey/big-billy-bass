#include "timeManager.h"

#include <time.h>


uint64_t timeManager::uS_since_epoch()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}