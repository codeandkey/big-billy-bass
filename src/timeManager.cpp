#include "timeManager.h"

#include <time.h>


uint64_t timeManager::getUsSinceEpoch()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}