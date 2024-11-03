

#ifndef __timeManager_h
#define __timeManager_h

#include <stdint.h>
#include <unistd.h>


class timeManager {
public:
    timeManager() :
        m_startTime(timeManager::getUsSinceEpoch()),
        m_lastLap(0)
    {}

    static uint64_t getUsSinceEpoch();

    inline uint64_t start() { m_startTime = timeManager::getUsSinceEpoch(); return m_startTime; }
    inline uint64_t elapsed() { return timeManager::getUsSinceEpoch() - m_startTime; }
    inline uint64_t lap() { m_lastLap = timeManager::getUsSinceEpoch() - m_startTime; m_startTime += m_lastLap; return m_lastLap; }
    inline uint64_t lastLap() const { return m_lastLap; }
private:
    uint64_t m_startTime;
    uint64_t m_lastLap;

};
#endif // __timeManager_h

