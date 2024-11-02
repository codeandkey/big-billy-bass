#include "gpio.h"
#include "timeManager.h"

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

#define MAXCHUNK 1024

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cmath>

extern "C" {
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
}

using namespace b3;

// Command types
enum GpioCommandType { GPIO_CMDTYPE_SUBMIT, GPIO_CMDTYPE_EXIT };

static uint8_t _computeNormalizedRMS(int channel, int window_start, int window_end);
static int _waitRead(int fd, char* buf, size_t count);
static void _handleChunk();
static int _gpioMain(gpio::GpioConfig config);

// Program state
static int g_audioSampleRate;
static gpio::GpioConfig g_config;
static uint64_t g_chunkStartTime;

static int16_t g_lastChunk[2][MAXCHUNK];
static int g_lastChunkSize;
static int16_t g_currentChunk[2][MAXCHUNK];
static int g_currentChunkSize;

// Parent process state
static pid_t g_gpioPid;
static int g_gpioStdinFd;

int gpio::gpio_spawn(gpio::GpioConfig config) {
    assert(!g_gpioPid);

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        fprintf(stderr, "Failed to create pipe: %s\n", strerror(errno));
        return -1;
    }

    g_gpioStdinFd = pipefd[1];

    if (!(g_gpioPid = fork())) {
        // Replace child stdin with parent pipe
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);

        return _gpioMain(config);
    }

    return 0;
}

int _gpioMain(gpio::GpioConfig config) {
    int res;
    uint8_t mode;

    // Set stdin to non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if ((res = fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK)) < 0) {
        fprintf(stderr, "Failed to set stdin to non-blocking: %s\n",
                strerror(errno));
        return res;
    }

    while (1) {
        res = read(STDIN_FILENO, &mode, sizeof(mode));

        if (res < 0) {
            if (errno == EAGAIN) {
                g_chunkStartTime = timeManager::getUsSinceEpoch();
                g_lastChunkSize = 0;
                continue;
            } else {
                goto cleanup;
            }
        }

        switch (mode) {
            case GPIO_CMDTYPE_SUBMIT:
                // Process the whole chunk
                if ((res = _waitRead(STDIN_FILENO, (char*) &g_currentChunkSize, sizeof(g_currentChunkSize))) < 0) {
                    goto cleanup;
                }

                g_lastChunkSize = g_currentChunkSize;

                memcpy(g_lastChunk[0], g_currentChunk[0], g_currentChunkSize * sizeof(*g_currentChunk[0]));
                memcpy(g_lastChunk[1], g_currentChunk[1], g_currentChunkSize * sizeof(*g_currentChunk[1]));

                if ((res = _waitRead(STDIN_FILENO, (char*) g_currentChunk[0], g_currentChunkSize * sizeof(*g_currentChunk))) < 0) {
                    goto cleanup;
                }

                if ((res = _waitRead(STDIN_FILENO, (char*) g_currentChunk[1], g_currentChunkSize * sizeof(*g_currentChunk))) < 0) {
                    goto cleanup;
                }

                _handleChunk();
                break;
            case GPIO_CMDTYPE_EXIT:
                fprintf(stderr, "GPIO exiting\n");
                goto cleanup;
            default:
                fprintf(stderr, "Unknown GPIO command: %d\n", mode);
                break;
        }
    }

cleanup:
    if (res < 0) {
        fprintf(stderr, "GPIO read fail: %s\n", strerror(errno));
        return res;
    }

    return 0;
}

int _waitRead(int fd, char* buf, size_t count) {
    int res;
    size_t bytesRead = 0;

    while (bytesRead < count && (res = read(fd, buf + bytesRead, count - bytesRead)) > 0) {
        bytesRead += res;
    }

    if (res < 0) {
        return res;
    }

    return bytesRead;
}

void _handleChunk() {
    uint64_t end_time = g_chunkStartTime + g_currentChunkSize * 1000000 / g_audioSampleRate;
    uint64_t now;

    const int window_samples = g_config.m_windowSize * g_audioSampleRate / 1000;

    while ((now = timeManager::getUsSinceEpoch()) < end_time) {
        int cursor = (now - g_chunkStartTime) * g_audioSampleRate / 1000000;
        int window_start = cursor - window_samples;
        int window_end = cursor;

        uint8_t rms[2];

        for (int i = 0; i < 2; ++i) {
            rms[i] = _computeNormalizedRMS(i, window_start, window_end);
        }

        // TODO: handle gpio
    }
}

// Sample processing impl
uint8_t _computeNormalizedRMS(int channel, int window_start, int window_end) {
    int64_t sum = 0;
    int count = 0;

    if (window_start < 0 && g_lastChunkSize) {
        for (int i = g_lastChunkSize + window_start; i < g_lastChunkSize; ++i) {
            sum += g_lastChunk[channel][i] * g_lastChunk[channel][i];
            count++;
        }
    }

    for (int i = window_start < 0 ? 0 : window_start; i < window_end && i < g_currentChunkSize; ++i) {
        sum += g_currentChunk[channel][i] * g_currentChunk[channel][i];
        count++;
    }

    sum = sqrt(sum / count);
    return sum / 32768;
}

// IPC interfaces
int gpio::gpio_submitChunk(int16_t* lpf, int16_t* hpf,
                           int n_samples) {
    int res;
    uint8_t mode = GPIO_CMDTYPE_SUBMIT;

    if ((res = write(g_gpioStdinFd, &mode, sizeof(mode))) < 0) {
        goto write_fail;
    }

    if ((res = write(g_gpioStdinFd, lpf, n_samples * sizeof(*lpf))) < 0) {
        goto write_fail;
    }

    if ((res = write(g_gpioStdinFd, hpf, n_samples * sizeof(*hpf))) < 0) {
        goto write_fail;
    }

    return 0;

write_fail:
    fprintf(stderr, "GPIO write fail: %s\n", strerror(errno));
    return res;
}

int gpio::gpio_exit() {
    int res;
    uint8_t mode = GPIO_CMDTYPE_EXIT;

    if ((res = write(g_gpioStdinFd, &mode, sizeof(mode))) < 0) {
        fprintf(stderr, "GPIO write fail: %s\n", strerror(errno));
        return res;
    }

    if (!waitpid(g_gpioPid, NULL, 0)) {
        fprintf(stderr, "Failed to wait for GPIO process: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}
