#include "gpio.h"

#include "logger.h"
#include "timeManager.h"

#ifdef ENABLE_GPIO
#include <pigpio.h>
#endif

#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>

extern "C" {
#include <signal.h>
}

using namespace b3;
using namespace b3::gpio;
using namespace std;

// Service instance
static GPIO* g_gpioService;

GPIO::GPIO(b3Config* config) : m_config(config), m_running(false) {
    assert(!g_gpioService);
    g_gpioService = this;
}

GPIO::~GPIO() {
    _flushPins();

#ifdef ENABLE_GPIO
    gpioTerminate();
#endif

    assert(g_gpioService);
    g_gpioService = nullptr;
}

void GPIO::start(void (*sigintHandler)(int)) {
    assert(!m_thread);

    m_running = true;
    m_thread = new thread([&]() {
        int ret = _threadMain(sigintHandler);

        const char* fmt = "GPIO thread terminated with status %d";
        if (ret) {
            ERROR(fmt, ret);
        } else {
            INFO(fmt, ret);
        }
    });
}

void GPIO::stop() {
    m_running = false;
    DEBUG("GPIO exit signal sent, joining..");

    assert(m_thread);
    m_thread->join();
    DEBUG("Joined GPIO thread");

    delete m_thread;
}

void GPIO::submitFrame(defaults::Sample* lpf, defaults::Sample* hpf, int n_samples) {
    assert(g_gpioService);

    lock_guard<mutex> lock(g_gpioService->m_frameQueueMutex);
    g_gpioService->m_frameQueue.emplace(lpf, hpf, n_samples);
}

int GPIO::_threadMain(void (*sigintHandler)(int)) {
    bool timingReset = false;

    m_gpioInitialized = false;

#ifdef ENABLE_GPIO
    m_gpioInitialized = true;

    // Set up pins
    if (gpioInitialise() < 0) {
        ERROR("Failed to initialize GPIO: %s", strerror(errno));
        m_gpioInitialized = false;
    }

    if (m_gpioInitialized) {
        uint8_t fail = _enumPins([&](int pin) -> uint8_t {
            if (gpioSetMode(pin, PI_OUTPUT) < 0) {
                ERROR("Failed to set pin %d mode: %s", pin, strerror(errno));
                return 1;
            }

            return 0;
        });

        if (fail) {
            m_gpioInitialized = false;
        }
    }

    _flushPins();
#else
    WARNING("GPIO disabled, using mock mode");
#endif

    // Install signal handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigintHandler;

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        ERROR("Failed installing SIGINT handler: %s", strerror(errno));
        return errno;
    }

    _flushPins();

    INFO("GPIO ready for frames");
    m_currentFrameStartUs = timeManager::getUsSinceEpoch();

    while (m_running.load()) {
        // Pull frame from queue, or reset timing if empty
        Frame currentFrame;
        {
            lock_guard<mutex> lock(m_frameQueueMutex);

            if (m_frameQueue.empty()) {
                m_currentFrameStartUs = timeManager::getUsSinceEpoch();

                if (!timingReset) {
                    WARNING("GPIO ran out of frames, timing reset");
                    timingReset = true;
                }

                continue;
            }

            currentFrame = std::move(m_frameQueue.front());
            m_frameQueue.pop();

            if (timingReset) {
                INFO("GPIO resuming stream");
                timingReset = false;
            }
        }

        assert(currentFrame.lpf.size() == currentFrame.hpf.size());

        _processFrame(currentFrame);
        m_previousFrame = std::move(currentFrame);

        uint64_t now = timeManager::getUsSinceEpoch();
        if (now - m_lastDebugUs > defaults::DEBUG_INTERVAL_S * 1000000) {
            INFO("%d GPIO writes/s, thresholds [%d %d]",
                 m_pinWriteCount / defaults::DEBUG_INTERVAL_S,
                 m_config->BODY_THRESHOLD, m_config->MOUTH_THRESHOLD);

            m_lastDebugUs = now;
            m_pinWriteCount = 0;
        }
    }

    _flushPins();
    return 0;
}

void GPIO::_processFrame(const Frame& frame) {
    bool skippedFrame = true;
    int rmsLpf = 0, rmsHpf = 0;

    while (1) {
        uint64_t now = timeManager::getUsSinceEpoch();

        rmsLpf = _computeRMS(now, frame, true);
        rmsHpf = _computeRMS(now, frame, false);

        if (rmsLpf < 0 || rmsHpf < 0) {
            break;
        }

        _writeGPIO(rmsLpf, rmsHpf);
        skippedFrame = false;
    }

    if (skippedFrame) {
        WARNING("GPIO skipped frame");
    }

    m_currentFrameStartUs += frame.lpf.size() * 1000000 / defaults::SAMPLE_RATE;
}

int GPIO::_computeRMS(uint64_t now, const Frame& frame, bool lpf) {
    int cursor =
        (now - m_currentFrameStartUs) * defaults::SAMPLE_RATE / 1000000;
    int window = m_config->RMS_WINDOW_MS * defaults::SAMPLE_RATE / 1000;
    int count = cursor;
    int sum = 0;

    const std::vector<defaults::Sample>& samples = lpf ? frame.lpf : frame.hpf;
    const std::vector<defaults::Sample>& lastSamples =
        lpf ? m_previousFrame.lpf : m_previousFrame.hpf;

    if (count <= 0 || count >= int(frame.lpf.size())) {
        return -1;
    }

    for (int i = cursor; i > 0; --i) {
        sum += samples[i] * samples[i];
    }

    for (int i = 0; i > cursor - window + int(lastSamples.size()); --i) {
        int idx = i + lastSamples.size();

        if (idx < 0 || idx >= int(lastSamples.size())) {
            continue;
        }

        sum += lastSamples[i] * lastSamples[i];
        count += 1;
    }

    return sqrt(sum / count);
}

uint8_t GPIO::_enumPins(uint8_t (*callback)(int)) {
    return callback(defaults::PIN_BODY_DIRECTION_A)
         | callback(defaults::PIN_BODY_DIRECTION_B)
         | callback(defaults::PIN_BODY_SPEED)
         | callback(defaults::PIN_MOUTH_DIRECTION_A)
         | callback(defaults::PIN_MOUTH_DIRECTION_B)
         | callback(defaults::PIN_MOUTH_SPEED);
}

void GPIO::_flushPins() {
#ifdef ENABLE_GPIO
    if (m_gpioInitialized) {
        _enumPins([](int pin) { gpioWrite(pin, 0); });
        DEBUG("GPIO pins flushed");
    }
#endif
}

#ifdef ENABLE_GPIO
void GPIO::_writeGPIO(int rmsLpf, int rmsHpf) {
    if (!m_gpioInitialized) {
        return;
    }

    static int flip;
    static int consecutiveLow;
    int move_body = rmsLpf > m_config->BODY_THRESHOLD;
    int move_mouth = rmsHpf > m_config->MOUTH_THRESHOLD;

    if (move_body) {
        if (flip) {
            gpioWrite(defaults::PIN_BODY_DIRECTION_A, 0);
            gpioWrite(defaults::PIN_BODY_DIRECTION_B, 1);
        } else {
            gpioWrite(defaults::PIN_BODY_DIRECTION_B, 0);
            gpioWrite(defaults::PIN_BODY_DIRECTION_A, 1);
        }

        gpioPWM(defaults::PIN_BODY_SPEED, defaults::BODY_DUTY);
        consecutiveLow = 0;
    } else {
        gpioPWM(defaults::PIN_BODY_SPEED, 0);
        ++consecutiveLow;

        if (consecutiveLow > (defaults::SAMPLE_RATE / 20)) {
            flip = (now / 1000000) % 4 < 2;
        }
    }

    if (move_mouth) {
        gpioWrite(defaults::PIN_MOUTH_DIRECTION_A, 0);
        gpioWrite(defaults::PIN_MOUTH_DIRECTION_B, 1);
        gpioPWM(defaults::PIN_MOUTH_SPEED, defaults::MOUTH_DUTY);
    } else {
        gpioPWM(defaults::PIN_MOUTH_SPEED, 0);
    }

    m_pinWriteCount += 1;
}
#else
void GPIO::_writeGPIO(int, int) {}
#endif
