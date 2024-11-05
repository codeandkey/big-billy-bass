#pragma once

#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <vector>

#include "signalProcessingDefaults.h"
#include "b3Config.h"

namespace b3 {

namespace gpio {
namespace defaults {
    // Audio processing defaults
    constexpr int SAMPLE_RATE = signalProcessingDefaults::DEFAULT_SAMPLE_RATE;

    // PWM duty cycles per motor (0-255)
    constexpr uint8_t BODY_DUTY = 255 * 90 / 100;  // 95%
    constexpr uint8_t MOUTH_DUTY = 0;

    // GPIO pin numbers
    constexpr int PIN_BODY_DIRECTION_A = 17;
    constexpr int PIN_BODY_DIRECTION_B = 27;
    constexpr int PIN_MOUTH_DIRECTION_A = 24;
    constexpr int PIN_MOUTH_DIRECTION_B = 25;
    constexpr int PIN_BODY_SPEED = 12;
    constexpr int PIN_MOUTH_SPEED = 13;

    // Audio sample type
    typedef int16_t Sample;

    // Debug interval (seconds)
    constexpr int DEBUG_INTERVAL_S = 3;
} // namespace defaults
} // namespace gpio

class GPIO {
   public:
    GPIO(b3Config* config);
    ~GPIO();

    /**
     * Starts the GPIO service.
     *
     * @param sigintHandler The handler to install for SIGINT.
     */
    void start(void(*sigintHandler)(int));

    /**
     * Stops the GPIO thread.
     */
    void stop();

    /**
     * Submits a chunk of audio samples to the GPIO thread.
     *
     * @param lpf The low-pass filtered audio samples.
     * @param hpf The high-pass filtered audio samples.
     * @param n_samples The number of samples in the arrays.
     */
    static void submitFrame(gpio::defaults::Sample* lpf,
                            gpio::defaults::Sample* hpf, int n_samples);

   private:
    // Configuration instance
    b3Config* m_config;

    // Frame queue management
    struct Frame {
        Frame(gpio::defaults::Sample* lpf,
              gpio::defaults::Sample* hpf, int n_samples)
            : lpf(lpf, lpf + n_samples), hpf(hpf, hpf + n_samples) {}

        Frame() {}

        std::vector<gpio::defaults::Sample> lpf, hpf;
    };

    std::mutex m_frameQueueMutex;
    std::queue<Frame> m_frameQueue;
    Frame m_previousFrame;

    // Time management
    uint64_t m_currentFrameStartUs;

    // Debug management
    uint64_t m_lastDebugUs;

    // Thread management
    std::thread* m_thread;
    std::atomic<bool> m_running;

    // GPIO management
    bool m_gpioInitialized;
    unsigned m_pinWriteCount;

    // Internal methods

    /**
     * The main thread method.
     *
     * @param sigint The handler to install for SIGINT.
     * @return 0 on success, nonzero otherwise.
     */
    int _threadMain(void(*sigintHandler)(int));

    /**
     * Processes a chunk of samples. Blocks until the chunk has been fully processed.
     *
     * @param frame The frame to process.
     */
    void _processFrame(const Frame& frame);

    /**
     * Computes the normalized RMS of a frame for a given time point.
     *
     * @param now The current time (us since epoch)
     * @param frame The current frame
     * @param lpf Whether to use the low-pass filtered audio
     *
     * @return the RMS if the cursor is within the frame, -1 otherwise
     */
    int _computeRMS(uint64_t now, const Frame& frame, bool lpf);

    /**
     * Enumerates the GPIO pins over a callback method.
     *
     * @param f The callback method, called for each pin number.
     * @return the union of each invocation's return value.
     */
    uint8_t _enumPins(uint8_t (*f)(int));

    /**
     * Sets all pin outputs to the low state.
     */
    void _flushPins();

    /**
     * Writes the GPIO pins based on the RMS values.
     *
     * @param rmsLpf The RMS value of the low-pass filtered audio.
     * @param rmsHpf The RMS value of the high-pass filtered audio.
     */
    void _writeGPIO(int rmsLpf, int rmsHpf);
}; // class GPIO

}  // namespace b3
