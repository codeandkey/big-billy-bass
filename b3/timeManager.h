

#ifndef __timeManager_h
#define __timeManager_h

#include <stdint.h>
#include <unistd.h>


class timeManager {
public:
    timeManager() :
        m_startTime(timeManager::uS_since_epoch()),
        m_lastLap(0)
    {}

    static uint64_t uS_since_epoch();

    inline uint64_t start() { m_startTime = timeManager::uS_since_epoch(); return m_startTime; }
    inline uint64_t elapsed() { return timeManager::uS_since_epoch() - m_startTime; }
    inline uint64_t lap() { m_lastLap = timeManager::uS_since_epoch() - m_startTime; m_startTime += m_lastLap; return m_lastLap; }
    inline uint64_t last_lap() const { return m_lastLap; }
private:
    uint64_t m_startTime;
    uint64_t m_lastLap;

};
#endif // __timeManager_h

