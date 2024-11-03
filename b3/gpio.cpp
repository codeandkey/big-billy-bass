#include "gpio.h"
#include "timeManager.h"

#include "logger.h"

#ifdef ENABLE_PIGPIO
#include <pigpio.h>
#else
int gpioInitialise() { return 0; }
void gpioTerminate() {}
int gpioSetMode(unsigned gpio, unsigned mode) { return 0; }
int gpioPWM(unsigned user_gpio, unsigned dutycycle) { return 0; }
int gpioWrite(unsigned user_gpio, unsigned level) { return 0; }
#define PI_OUTPUT 0
#endif

#define MAXCHUNK 16384

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cmath>

#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <atomic>

extern "C" {
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
}

extern void sigint_handler(int sig);

using namespace b3;
using namespace std;

// Command types
enum GpioCommandType { GPIO_CMDTYPE_SUBMIT, GPIO_CMDTYPE_EXIT };

struct GpioCommand {
    GpioCommandType type;

    struct {
        vector<int16_t> lpf, hpf;
    } submit;

    static GpioCommand withType(GpioCommandType t) {
        GpioCommand out;
        out.type = t;
        return out;
    }

    static GpioCommand makeSubmit(int16_t* lpf, int16_t* hpf, int n_samples) {
        GpioCommand out;
        out.type = GPIO_CMDTYPE_SUBMIT;
        out.submit.lpf = std::move(vector<int16_t>(lpf, lpf + n_samples));
        out.submit.hpf = std::move(vector<int16_t>(lpf, lpf + n_samples));
        return out;
    }
};

static int _computeNormalizedRMS(int channel, int window_start, int window_end);
static int _waitRead(int fd, char* buf, size_t count);
static void _handleChunk();
static int _gpioMain(gpio::GpioConfig config);

// Program state
static gpio::GpioConfig g_config;
static uint64_t g_chunkStartTime;

static int16_t g_lastChunk[2][MAXCHUNK];
static int g_lastChunkSize;
static int16_t g_currentChunk[2][MAXCHUNK];
static int g_currentChunkSize;

// Sync state
static thread* gpio_thread;
static mutex gpio_command_mutex;
static queue<GpioCommand> gpio_command_queue;
static atomic<bool> g_gpioShouldExit = 0;

int gpio::gpio_spawn(gpio::GpioConfig config) {
    g_config = config;

    gpio_thread = new thread([&]() {
        int ret = _gpioMain(config);

        const char* fmt = "GPIO thread terminated with status %d";
        if (ret) {
            ERROR(fmt, ret);
        } else {
            INFO(fmt, ret);
        }
    });

    return 0;
}

int _gpioMain(gpio::GpioConfig config) {
    int res;
    uint8_t mode;

    INFO("GPIO initializing..");

    // Set up pins
    if (gpioInitialise() < 0) {
        ERROR("Failed to initialize GPIO: %s", strerror(errno));
        return -1;
    }

    signal(SIGINT, sigint_handler);

    int pins[6];
    config.getPins(pins);

    for (int i = 0; i < 6; ++i) {
        int pin = pins[i];

        if (gpioSetMode(pin, PI_OUTPUT) < 0) {
            ERROR("Failed to set pin %d to output: %s", pin, strerror(errno));
            return -1;
        }

        gpioWrite(pin, 0);
    }

    INFO("GPIO ready!");

    while (!g_gpioShouldExit) {
        GpioCommand next_command;
        {
            lock_guard<mutex> lock(gpio_command_mutex);

            if (gpio_command_queue.empty()) {
                g_chunkStartTime = timeManager::getUsSinceEpoch();
                //DEBUG("GPIO stream timing reset");
                continue;
            }

            next_command = std::move(gpio_command_queue.front());
            gpio_command_queue.pop();
        }

        switch (next_command.type) {
            case GPIO_CMDTYPE_SUBMIT:
                g_lastChunkSize = g_currentChunkSize;

                memcpy(g_lastChunk[0], g_currentChunk[0], g_lastChunkSize * sizeof(*g_currentChunk[0]));
                memcpy(g_lastChunk[1], g_currentChunk[1], g_lastChunkSize * sizeof(*g_currentChunk[1]));

                assert(next_command.submit.lpf.size() == next_command.submit.hpf.size());

                g_currentChunkSize = next_command.submit.lpf.size();
                //INFO("Reading chunk of size %d", g_currentChunkSize);

                memcpy(g_currentChunk[0], &next_command.submit.lpf[0], g_currentChunkSize * sizeof(*g_currentChunk[0]));
                memcpy(g_currentChunk[1], &next_command.submit.hpf[0], g_currentChunkSize * sizeof(*g_currentChunk[0]));

                g_chunkStartTime = timeManager::getUsSinceEpoch();
                _handleChunk();
                break;
            default:
                ERROR("Unknown GPIO command: %d", mode);
                break;
        }
    }

    for (int i = 0; i < sizeof pins / sizeof pins[0]; ++i) {
        gpioWrite(pins[i], 0);
    }

    gpioTerminate();
    INFO("Cleaned up GPIO pin state");

    return 0;
}

int _waitRead(int fd, char* buf, size_t count) {
    int res;
    size_t bytesRead = 0;

    while (bytesRead < count) {
        res = read(fd, buf + bytesRead, count - bytesRead);

        if (res < 0) {
            if (errno == EAGAIN) {
                continue;
            } else {
                return res;
            }
        }

        bytesRead += res;
    }

    return bytesRead;
}

void _handleChunk() {
    uint64_t end_time = g_chunkStartTime + g_currentChunkSize * (1000000 / g_config.m_sampleRate);
    uint64_t now = timeManager::getUsSinceEpoch();

    const int window_samples = g_config.m_windowSize * g_config.m_sampleRate / 1000;

    while ((now = timeManager::getUsSinceEpoch()) < end_time) {
        int cursor = (now - g_chunkStartTime) * g_config.m_sampleRate / 1000000;
        int window_start = cursor - window_samples;
        int window_end = cursor;

        int rms[2];

        for (int i = 0; i < 2; ++i) {
            rms[i] = _computeNormalizedRMS(i, window_start, window_end);
        }

        //DEBUG("GPIO playback window=[%d, %d] sample=%d rms[0]=%d", window_start, window_end, g_currentChunk[0][cursor], rms[0]);

        static int flip;
        static int consecutiveLow;
        int move_body = rms[0] > g_config.m_bodyMinRMS;
        int move_mouth = 0; // rms[0] > g_config.m_mouthMinRMS;


        if (move_body) {
            if (flip) {
                gpioWrite(g_config.m_pinBodyDirectionA, 0);
                gpioWrite(g_config.m_pinBodyDirectionB, 1);
            } else {
                gpioWrite(g_config.m_pinBodyDirectionB, 0);
                gpioWrite(g_config.m_pinBodyDirectionA, 1);
            }

            gpioPWM(g_config.m_pinBodySpeed, gpio::BODY_DUTY);
            consecutiveLow = 0;
        } else {
            gpioPWM(g_config.m_pinBodySpeed, 0);
            ++consecutiveLow;
            
            if (consecutiveLow > (g_config.m_sampleRate / 20)) {
                flip = (now / 1000000) % 4 < 2;
            }
        }

        if (move_mouth) {
            gpioWrite(g_config.m_pinMouthDirectionA, 0);
            gpioWrite(g_config.m_pinMouthDirectionA, 1);
            gpioPWM(g_config.m_pinMouthSpeed, gpio::MOUTH_DUTY);
        } else {
            gpioPWM(g_config.m_pinMouthSpeed, 0);
        }
    }

    g_chunkStartTime = end_time;
}

// Sample processing impl
int _computeNormalizedRMS(int channel, int window_start, int window_end) {
    int64_t sum = 0;
    int count = 0;

    if (window_start < 0 && g_lastChunkSize) {
        for (int i = max(0, g_lastChunkSize + window_start); i < g_lastChunkSize; ++i) {
            sum += g_lastChunk[channel][i] * g_lastChunk[channel][i];
            count++;
        }
    }

    for (int i = window_start < 0 ? 0 : window_start; i < window_end && i < g_currentChunkSize; ++i) {
        sum += g_currentChunk[channel][i] * g_currentChunk[channel][i];
        count++;
    }

    return sqrt(sum / count);
}

// IPC interfaces
int gpio::gpio_submitChunk(int16_t* lpf, int16_t* hpf,
                           int n_samples) {
    lock_guard<mutex> lock(gpio_command_mutex);
    gpio_command_queue.push(GpioCommand::makeSubmit(lpf, hpf, n_samples));
    return 0;
}

int gpio::gpio_exit() {
    g_gpioShouldExit = true;
    DEBUG("GPIO exit signal sent, joining..");

    gpio_thread->join();
    DEBUG("Joined GPIO thread!");

    return 0;
}
