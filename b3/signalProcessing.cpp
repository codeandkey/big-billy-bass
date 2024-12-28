#include "signalProcessing.h"

extern "C" {
#include <sys/un.h>
#include <unistd.h>
}

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gpio.h"
#include "audioFile.h"
#include "logger.h"
#include "sighandler.h"


#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

using namespace b3;

signalProcessor::~signalProcessor()
{}



void signalProcessor::update(State state)
{
    if (m_state != state)
        set_state(state);


    if (m_state == State::PLAYING && !m_stop_flag) {
        // update filters
        m_lpf->set_cutoff(m_config.LPF_CUTOFF);
        m_hpf->set_cutoff(m_config.HPF_CUTOFF);

        uint64_t dt = us_to_next_chunk();
        usleep(MIN(dt, m_chunk_size_us));

        if (m_fillBuffer) {
            m_fillBuffer = false;
            for (int i = 0; i < m_config.BUFFER_LENGTH_MS - 1; i++)
                _process_chunk();
        }
        _process_chunk();

        if (dt == 0) {
            m_underun_count = MIN(m_underun_count + 1, m_config.CHUNK_COUNT);
            if (m_underun_count == m_config.BUFFER_LENGTH_MS) {
                m_fillBuffer = true;
                DEBUG("Possible chunk underrun likely due to process timing: %d uS", m_tm.last_lap());
            }
        } else
            m_underun_count = MAX(m_underun_count - 1, 0);
    }

    if (m_stop_flag)
        set_state(STOPPED);
}



void signalProcessor::set_state(State to)
{
    switch (to) {
    case State::PLAYING:
        if (!m_audio_driver) {
            ERROR("audioProcessor - No audio driver loaded");
            return;
        }
        if (!m_audio_source) {
            ERROR("audioProcessor - No audio file loaded");
            return;
        }

        m_chunk_time_stamp = m_tm.uS_since_epoch();
        m_tm.start();
        // set flags
        m_stop_flag = 0;
        m_fillBuffer = true;
        break;


    case State::STOPPED:
        clear_audio_source();
        m_audio_driver->close_driver();
        break;
    case State::PAUSED:
        break; // no longer does anything

    }
    INFO("SignalProcessor State Transition: %d\n", m_state, to);
    m_state = to;
}


void signalProcessor::set_audio_driver(audioDriver *driver)
{
    if (!driver) {
        ERROR("audioProcessor - Null audio driver pointer");
        return;
    }
    m_audio_driver = driver;
}

void signalProcessor::set_audio_source(audioSource *S)
{
    if (!S) {
        ERROR("audioProcessor - Null audio file pointer");
        return;
    }
    if (m_audio_source) {
        WARNING("File Already Loaded, Unloading previous source");
        clear_audio_source();
    }
    m_audio_source = S;

    _negotiate_chunk_size();

    // create filters   
    m_lpf = biQuadFilter::new_lpf(m_audio_source->sample_rate(), m_config.LPF_CUTOFF, Q);
    m_hpf = biQuadFilter::new_hpf(m_audio_source->sample_rate(), m_config.LPF_CUTOFF, Q);
}

void signalProcessor::_negotiate_chunk_size()
{
    // this is the desired chunk size based on the audio source settings
    m_frames_per_chunk = _calculate_chunk_size_frms(
        m_config.CHUNK_SIZE_MS,
        m_audio_source->sample_rate()
    );

    // see if we can set the alsa drivers to the same settings
    DEBUG("Expected chunks size (frames/chunk) %d", m_frames_per_chunk);
    int audioDriverChunkSize = m_audio_driver->set_output_params(
        m_audio_source->sample_rate(),
        m_audio_source->ch_count(),
        m_frames_per_chunk
    );

    if (m_frames_per_chunk != audioDriverChunkSize) {
        INFO(
            "Processing chunks size of %d bytes does not match with audio driver which configured to %d bytes",
            m_frames_per_chunk,
            audioDriverChunkSize
        );
        m_frames_per_chunk = audioDriverChunkSize;
    }

    if (m_audio_source->ch_count() == 0) {
        // grr bad
        WARNING("Negotiation resulted in a channel count of zero!");
        return;
    }
    m_chunk_size_us = m_frames_per_chunk * 1e6 / m_audio_source->sample_rate();

    DEBUG("Setting final chunk size to %u", m_frames_per_chunk);
    DEBUG("Setting final uS chunk size to %llu", m_chunk_size_us);
    // TODO - likely want to go back to the audio source and tell it to tweak input config (sample rate/channel count) based on output.
}

int signalProcessor::_process_chunk()
{
    if (!m_audio_source) {
        ERROR("audioProcessor - No audio file loaded");
        return -1;
    }
    assert(m_lpf != nullptr);
    assert(m_hpf != nullptr);

    // create buffers for new PCM data
    // need to know how big buffer is going to be
    int ch_count = m_audio_source->ch_count();
    pcm_t pcm_buff[m_frames_per_chunk * ch_count];
    pcm_t lpf_buff[m_frames_per_chunk], hpf_buff[m_frames_per_chunk];
    // read into buffers
    int samples_read = m_audio_source->read_chunk(pcm_buff, m_frames_per_chunk);

    // check for early exit
    if (signalHandler::g_shouldExit)
        return 0;

    // check for eof
    if (samples_read < m_frames_per_chunk ||
        samples_read == 0) {
        m_stop_flag = true;
        return 0;
    }


    // process data
    for (int frm = 0; frm < samples_read; frm += ch_count) {
        lpf_buff[frm / ch_count] = m_lpf->update(_pcm_to_mono(&pcm_buff[frm], ch_count));
        hpf_buff[frm / ch_count] = m_hpf->update(_pcm_to_mono(&pcm_buff[frm], ch_count));
    }

    // send data to GPIO
    GPIO::submitFrame(lpf_buff, hpf_buff, samples_read);
    m_chunk_time_stamp += m_chunk_size_us;

    // write audio data to the audio driver
    m_audio_driver->write_chunk(pcm_buff, samples_read);
    m_tm.lap();

    return 0;
}
