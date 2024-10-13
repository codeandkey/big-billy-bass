#include "gpioStream.h"

#include <cstring>

#include "logger.h"
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

using namespace b3;
using namespace std;

GpioStream* GpioStream::s_instance = nullptr;

GpioStream::GpioStream(GpioStreamConfig config) : m_config(config), m_low_mouth_frames(0) {
    assert(!s_instance);
    s_instance = this;
}

GpioStream::~GpioStream() {
    assert(s_instance == this);
    s_instance = nullptr;
}

GpioStream& GpioStream::get() {
    assert(s_instance);
    return *s_instance;
}

void GpioStream::onStart() {
    m_init_ok = true;

    if (gpioInitialise() < 0) {
        ERROR("GPIO init failed");
	m_init_ok = false;
    }

    gpioSetMode(PIN_HEAD_HIGH, PI_OUTPUT);
    gpioSetMode(PIN_HEAD_LOW, PI_OUTPUT);
    gpioSetMode(PIN_TAIL_HIGH, PI_OUTPUT);
    gpioSetMode(PIN_TAIL_LOW, PI_OUTPUT);
    gpioSetMode(PIN_MOUTH_HIGH, PI_OUTPUT);
    gpioSetMode(PIN_MOUTH_LOW, PI_OUTPUT);
    gpioSetMode(PIN_HEAD_SPEED, PI_OUTPUT);
    gpioSetMode(PIN_TAIL_SPEED, PI_OUTPUT);
    gpioSetMode(PIN_MOUTH_SPEED, PI_OUTPUT);
}

void GpioStream::onStop() {
    if (m_init_ok)
        gpioTerminate();
}

void GpioStream::frame(State state) {
    if (!m_init_ok)
        return;

    uint64_t now = timeManager::getUsSinceEpoch();

    // Lock config for the duration of this frame
    GpioStreamConfig config;
    {
        lock_guard<mutex> lock(m_config_mutex);
        config = m_config;
    }

    GpioFrame gpio_frame;

    // Try and extract GPIO data from the current live chunk
    if (m_live_chunk._extractGpioFrame(now, config, gpio_frame)) {
        _applyGpioFrame(gpio_frame);
        return;
    }

    // No GPIO data available in this chunk, try and load the next one
    {
        lock_guard<mutex> lock(m_pending_chunks_mutex);

        // If we're in testing mode, generate a chunk with on/off switching cycles
#ifdef GPIO_TEST
	if (m_pending_chunks.empty()) {
		AudioSample chunk[config.sample_rate * 4]; // 1 second worth of samples
		memset(chunk, 0, sizeof(chunk));

		// half on, half off
		for (int j = 0; j < sizeof(chunk) / sizeof(chunk[0]) / 2; ++j) {
		    chunk[j] = 32767;
		}

	        m_pending_chunks.emplace(GpioChunk(vector<AudioSample>(chunk, chunk + sizeof(chunk) / sizeof(chunk[0])),
					           vector<AudioSample>(chunk, chunk + sizeof(chunk) / sizeof(chunk[0])),
	    				           now));

		DEBUG("GPIO loaded test chunk");
		return;
	}
#endif

        // Verify the stream isn't dead
        if (m_pending_chunks.empty()) {
            // No chunks available, try again later.
            // Write a warning if we're in the PLAYING state
            if (state == State::PLAYING) {
                WARNING("GPIO stream ran out of chunks while playing");
            }
            return;
        }

        // Find the next chunk with available GPIO data
        while (!m_pending_chunks.empty()) {
            if (m_pending_chunks.front()._extractGpioFrame(now, config,
                                                           gpio_frame)) {
                _applyGpioFrame(gpio_frame);
                m_live_chunk = std::move(m_pending_chunks.front());

                DEBUG("GPIO pulled frame for t=%lu", m_live_chunk.start_time);
                return;
            }

            // Discard this chunk
            m_pending_chunks.pop();
        }

        // There were chunks in the stream, but none mapping to a good GPIO
        // frame Write a warning if we're in the PLAYING state
        if (state == State::PLAYING) {
            WARNING("GPIO stream fell significantly behind (>=1 chunk)");
        }
    }
}

void GpioStream::_applyGpioFrame(const GpioFrame& data) {
    // Convert LPF/HPF to fish limbs
    // TODO : switch between head and tail actuation
    if (data.hpf > 32768) {
	    gpioWrite(PIN_MOUTH_HIGH, 1);
	    gpioWrite(PIN_MOUTH_LOW, 0);
            m_low_mouth_frames = 0; 
    } else {
	    gpioWrite(PIN_MOUTH_LOW, 1);
	    gpioWrite(PIN_MOUTH_HIGH, 0);
            m_low_mouth_frames++;
    }

    gpioWrite(PIN_TAIL_HIGH, 1);
    gpioWrite(PIN_TAIL_LOW, 0);

    int mouth_speed = 0;

    if (m_low_mouth_frames < 500) {
        mouth_speed = 255;
    } else {
        mouth_speed = 0;
    }

    gpioPWM(PIN_HEAD_SPEED, data.lpf >> 8);
    gpioPWM(PIN_MOUTH_SPEED, mouth_speed);

    DEBUG("Recv data hpf %d lpf %d", data.hpf, data.lpf);
    DEBUG("Write mouth speed %d, tail speed %d", mouth_speed, data.lpf >> 8);
}

int GpioChunk::_extractGpioFrame(uint64_t now, const GpioStreamConfig& config,
                                 GpioFrame& frame) {
    int sample_index = (now - start_time) / (1000000 / config.sample_rate);

    if (sample_index < 0 || sample_index >= lpf.size()) {
        return 0;
    }

    // Convert gpio_window milliseconds to samples
    int window_width = config.gpio_window * config.sample_rate /
                       1000;  // TODO: moveme to const
    int window_low = max(0, sample_index - window_width / 2);
    int window_high = min((int)lpf.size() - 1, sample_index + window_width / 2);
    int window_size = window_high - window_low;

    frame.lpf = _filterSamples(&lpf[window_low], window_size, config.gain[LPF],
                               config.threshold[LPF]);

    frame.hpf = _filterSamples(&hpf[window_low], window_size, config.gain[HPF],
                               config.threshold[HPF]);
    return window_size;
}

uint16_t GpioChunk::_filterSamples(const AudioSample* sample, int count,
                                   float gain, float threshold) {
    int avg = 0;
    int upper = int(numeric_limits<uint16_t>::max());

    for (int i = 0; i < count; i++) {
        avg += abs(sample[i]) * 2;
    }

    avg /= count;

    // Premul + clamp
    avg = min(int(avg * gain), upper);

    // Threshold
    return (avg < threshold * upper) ? 0 : avg;
}

void GpioStream::submit(AudioSample* lpf, AudioSample* hpf, int samples,
                        uint64_t start_time) {
    if (!m_init_ok)
        return;

    // Quickly copy data to pending chunks and return
    lock_guard<mutex> lock(m_pending_chunks_mutex);

    m_pending_chunks.emplace(GpioChunk(vector<AudioSample>(lpf, lpf + samples),
                                       vector<AudioSample>(hpf, hpf + samples),
                                       start_time));

    DEBUG("GPIO accepted chunk %lu into pending queue", start_time);
}
