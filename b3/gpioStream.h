#pragma once

#include <cassert>
#include <cstdint>
#include <optional>
#include <queue>
#include <vector>

#include "task.h"

namespace b3 {

constexpr int PIN_HEAD = 0;   // GPIO pin for fish head
constexpr int PIN_TAIL = 0;   // GPIO pin for fish tail
constexpr int PIN_MOUTH = 0;  // GPIO pin for fish mouth

constexpr int AUDIO_SAMPLE_RATE = 44100;  // default audio hz
constexpr int GPIO_WINDOW = 250;          // GPIO sample window in ms
constexpr float GPIO_GAIN = 1.0f;         // default GPIO gain
constexpr float GPIO_THRESHOLD = 0.5f;    // default GPIO threshold

typedef int16_t AudioSample;

enum Channel {
    LPF,
    HPF,
    N_CHANNELS,
};

struct GpioStreamConfig {
    int gpio_window;  // The milliseconds of audio samples to average for GPIO,
                      // centered about the currently playing audio sample
    int sample_rate;  // The audio sample rate

    // The gain applied to the GPIO signal, before threshold filter
    float gain[2];

    float threshold[2];  // Samples below the threshold are considered 0

    constexpr int SAMPLES_PER_GPIO() {
        assert(sample_rate % gpio_window == 0);
        return sample_rate / gpio_window;
    }

    GpioStreamConfig() {
        gpio_window = GPIO_WINDOW;
        sample_rate = AUDIO_SAMPLE_RATE;
        for (int i = 0; i < N_CHANNELS; i++) {
            gain[i] = GPIO_GAIN;
            threshold[i] = GPIO_THRESHOLD;
        }
    }

    // Shared parameters
    inline GpioStreamConfig& setGpioWindow(int gpio_window) {
        this->gpio_window = gpio_window;
        return *this;
    }

    inline GpioStreamConfig& setSampleRate(int sample_rate) {
        this->sample_rate = sample_rate;
        return *this;
    }

    // Per-channel parameters
#define defChannelParam(name, setter)                                     \
    inline GpioStreamConfig& setter(float value, int chan = N_CHANNELS) { \
        if (chan == N_CHANNELS) {                                         \
            for (int i = 0; i < N_CHANNELS; i++) {                        \
                this->name[i] = value;                                    \
            }                                                             \
        } else {                                                          \
            this->name[chan] = value;                                     \
        }                                                                 \
        return *this;                                                     \
    }

    defChannelParam(gain, setGain);
    defChannelParam(threshold, setThreshold);
};

/**
 * A pairing of GPIO signals extracted from the high-pass and low-pass filter
 * streams.
 */
struct GpioFrame {
    uint16_t lpf;
    uint16_t hpf;
};

struct GpioChunk {
    GpioChunk(std::vector<AudioSample>&& lpf, std::vector<AudioSample>&& hpf,
              uint64_t start_time)
        : lpf(std::move(lpf)), hpf(std::move(hpf)), start_time(start_time) {}

    GpioChunk() : start_time(0) {}

    // Audio samples
    std::vector<AudioSample> lpf;
    std::vector<AudioSample> hpf;

    // Absolute start time of the chunk
    uint64_t start_time;

    /**
     * Extract a GPIO frame from this audio chunk, if possible. Partial frames
     * are considered successfully extracted. Chunk bounaries are NOT crossed-
     * if a window is split between two chunks, only the samples from this chunk
     * are considered.
     *
     * @param now Current time in us after epoch
     * @param config Current GPIO stream configuration (for rates)
     * @param out Output GPIO frame
     *
     * @return 1 if a frame was extracted, 0 otherwise
     */
    int _extractGpioFrame(uint64_t now, const GpioStreamConfig& config,
                          GpioFrame& out);

    // Process a range of audio frames into a GPIO sample
    uint16_t _filterSamples(const AudioSample* samples, int n_samples,
                            float gain, float threshold);
};

class GpioStream : public Task {
   public:
    GpioStream(GpioStreamConfig conf = GpioStreamConfig());
    ~GpioStream();

    // Config manipulation
    GpioStreamConfig getConfig();
    void loadConfig(GpioStreamConfig config);

    // Task interfaces
    void onStart() override;
    void onStop() override;
    void frame(State) override;

    /**
     * Submit a chunk of audio samples to the GPIO stream
     *
     * @param hpf High-pass filter audio samples
     * @param lpf Low-pass filter audio samples
     * @param samples Number of samples in the chunk
     * @param start_time Timestamp of the first sample in the chunk (us after
     * epoch)
     */
    void submit(AudioSample* hpf, AudioSample* lpf, int samples,
                uint64_t start_time);

    /**
     * Return a reference to the singleton instance of the GPIO stream
     */
    static GpioStream& get();

   private:
    static GpioStream* s_instance;  // Singleton instance

    // Apply a single GPIO frame to the output pins
    void _applyGpioFrame(const GpioFrame& frame);

    // Current GPIO chunk
    GpioChunk m_live_chunk;

    // Pending chunk queue + sync
    std::queue<GpioChunk> m_pending_chunks;
    std::mutex m_pending_chunks_mutex;

    // Live configuration + sync
    std::mutex m_config_mutex;
    GpioStreamConfig m_config;
};

}  // namespace b3
