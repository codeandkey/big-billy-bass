#include "gpioStream.h"

#include <cstring>

extern "C" {
#include <signal.h>
}

#include "logger.h"
#include "timeManager.h"
#include "sigint.h"

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

uint64_t g_startTime = timeManager::getUsSinceEpoch();

GpioStream::GpioStream(GpioStreamConfig config) : m_config(config) {
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
    m_stream_end_warning = true;

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

    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
}

void GpioStream::onStop() {
    if (m_init_ok) {
        gpioTerminate();
        gpioWrite(PIN_HEAD_HIGH, 0);
        gpioWrite(PIN_HEAD_LOW, 0);
        gpioWrite(PIN_MOUTH_HIGH, 0);
        gpioWrite(PIN_MOUTH_LOW, 0);
        gpioWrite(PIN_TAIL_HIGH, 0);
        gpioWrite(PIN_TAIL_LOW, 0);
        //gpioPWM(PIN_BODY_SPEED, 0);
        //gpioPWM(PIN_MOUTH_SPEED, 0);
        gpioWrite(PIN_BODY_SPEED, 0);
        gpioWrite(PIN_MOUTH_SPEED, 0);
        m_init_ok = false;

        INFO("Cleaned up GPIO pins");
    }
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
    if (m_live_chunk._extractGpioFrame(now - m_live_chunk_start, config, gpio_frame)) {
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
	    				           config.sample_rate));

		DEBUG("GPIO loaded test chunk");
		return;
	}
#endif

        // Verify the stream isn't dead
        if (m_pending_chunks.empty()) {
#ifndef DISABLE_GPIO
            // No chunks available, try again later.
            // Write a warning if we're in the PLAYING state
            if (state == State::PLAYING && m_stream_end_warning) {
                WARNING("GPIO stream ran out of chunks during playing (t=%lu)", now);
                m_stream_end_warning = false;
            }
#endif

            m_live_chunk_start = now;
            return;
        }

        m_stream_end_warning = true;

        // Pull and start the next chunk
        uint64_t last_start, last_end;

        // Find the next chunk with available GPIO data
        m_live_chunk_start += m_live_chunk.lengthUs();
        m_live_chunk = std::move(m_pending_chunks.front());

        m_pending_chunks.pop();

        DEBUG("GPIO pulled next chunk, lengthUs = %d", m_live_chunk.lengthUs());
    }
}

void GpioStream::_applyGpioFrame(const GpioFrame& data) {
    // Convert LPF/HPF to fish limbs
    // TODO : switch between head and tail actuation
    int move_mouth = (data.hpf > 5000);

    // Periodically flip head/tail actuation
    int seconds = (timeManager::getUsSinceEpoch() / 1000000);
    int ms = (timeManager::getUsSinceEpoch() / 10000);
    int move_body = (data.lpf > 18060), move_head, move_tail;

    if (seconds % 2 < 1) {
        move_head = 0;
        move_tail = move_body;
    } else {
        move_tail = 0;
        move_head = move_body;
    }

    // Generally try to keep PWMs in low state
    if (move_head || move_tail || move_mouth) {
        //gpioPWM(PIN_BODY_SPEED, BODY_SPEED);
        //gpioPWM(PIN_MOUTH_SPEED, MOUTH_SPEED);
        gpioWrite(PIN_BODY_SPEED, 1);
        gpioWrite(PIN_MOUTH_SPEED, 1);
    } else {
        //gpioPWM(PIN_BODY_SPEED, 0);
        //gpioPWM(PIN_MOUTH_SPEED, 0);
        gpioWrite(PIN_BODY_SPEED, 0);
        gpioWrite(PIN_MOUTH_SPEED, 0);
    }

    if (!(ms % 100)) {
        DEBUG("GPIO write mouth %d head %d tail %d LPF %d HPF %d", move_mouth, move_head, move_tail, data.lpf, data.hpf);
    }

    // TODO consolidate logic here after deciding how to share
    // head/tail time
    if (move_head) {
	    gpioWrite(PIN_HEAD_LOW, 0);
	    gpioWrite(PIN_HEAD_HIGH, 1);
    } else if (move_tail) {
	    gpioWrite(PIN_TAIL_LOW, 0);
	    gpioWrite(PIN_TAIL_HIGH, 1);
    } else {
	    gpioWrite(PIN_TAIL_LOW, 0);
	    gpioWrite(PIN_TAIL_HIGH, 0);
    }

    if (move_mouth) {
	    gpioWrite(PIN_MOUTH_LOW, 0);
	    gpioWrite(PIN_MOUTH_HIGH, 1);
    } else {
        gpioWrite(PIN_MOUTH_HIGH, 0);
        gpioWrite(PIN_MOUTH_LOW, 1);
    }

    //DEBUG("Write head %d, tail %d LPF %d HPF %d", move_head, move_tail, data.lpf, data.hpf);
}

int GpioChunk::_extractGpioFrame(uint64_t now, const GpioStreamConfig& config,
                                 GpioFrame& frame) {
    int sample_index = now * config.sample_rate / 1000000;
    //DEBUG("Chunk lookup now=%lu rate=%d, index=%d (count=%d)", now, config.sample_rate, sample_index, lpf.size());

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

    return avg;

    // Premul + clamp
    //avg = min(int(avg * gain), upper);

    // Threshold
    //return (avg < threshold * upper) ? 0 : avg;
}

void GpioStream::submit(AudioSample* lpf, AudioSample* hpf, int samples,
                        uint64_t start_time) {
    if (!m_init_ok) {
        DEBUG("GPIO ignoring submission because init failed");
        return;
    }

    // Quickly copy data to pending chunks and return
    lock_guard<mutex> lock(m_pending_chunks_mutex);

    m_pending_chunks.emplace(GpioChunk(vector<AudioSample>(lpf, lpf + samples),
                                       vector<AudioSample>(hpf, hpf + samples),
                                       m_config.sample_rate));

    if (m_pending_chunks.size() == 1) {
        m_live_chunk_start = timeManager::getUsSinceEpoch();
        DEBUG("GPIO marked first pending chunk point at t=%lu", m_live_chunk_start - g_startTime);
    }

    DEBUG("GPIO accepted chunk %lu (%d samples) into pending queue (%d)", start_time - g_startTime, samples, m_pending_chunks.size());
}
