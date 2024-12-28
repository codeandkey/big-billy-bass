#pragma once

#include <cassert>
#include <cstring>
#include <cstdio>

#include "audioSource.h"
#include "timeManager.h" 
#include "logger.h"
#include "biQuadFilter.h"
#include "audioDriver.h"
#include "b3Config.h"

namespace b3 {
    namespace SPD = signalProcessingDefaults;

    enum State {
        STOPPED, // No audio file is loaded or playing
        PLAYING, // The audio file is currently playing
        PAUSED   // There is an audio file loaded, but not playing
    };

    class signalProcessor {
    public:
        signalProcessor(b3Config &conf) :
            m_fillBuffer(false),
            m_stop_flag(false),
            m_config(conf),
            m_state(State::STOPPED),
            m_audio_source(nullptr),
            m_audio_driver(nullptr),
            m_lpf(nullptr),
            m_hpf(nullptr),
            m_underun_count(0),
            m_chunk_time_stamp(timeManager::uS_since_epoch()),
            m_chunk_size_us(0),
            m_frames_per_chunk(0)
        {}

        ~signalProcessor();
        
        void update(State state);

        // getters / setters
        void set_state(State to);

        inline State get_state() const { return m_state; }

        void set_audio_driver(audioDriver *driver);

        void set_audio_source(audioSource *S);

        inline void clear_audio_source()
        {
            m_audio_source = nullptr;

        }


        uint64_t us_to_next_chunk()
        {
            uint64_t t = m_tm.uS_since_epoch();
            if (t > m_chunk_time_stamp)
                return 0;

            return m_chunk_time_stamp - t;
        }


    private:

        inline pcm_t _pcm_to_mono(pcm_t *inBuff, int16_t channels)
        {
            int sum = 0;
            for (int i = 0; i < channels; i++)
                sum += inBuff[i];
            return sum / channels;
        }

        static inline int _calculate_chunk_size_frms(int chunk_size_ms, int sample_rate_hz)
        {
            return sample_rate_hz * chunk_size_ms / 1e3;
        }

        int _process_chunk();

        void _negotiate_chunk_size();

        // flags
        bool m_fillBuffer;
        bool m_stop_flag;

        b3Config &m_config;

        State m_state;

        timeManager m_tm;

        audioSource *m_audio_source;
        audioDriver *m_audio_driver;
        biQuadFilter *m_lpf;
        biQuadFilter *m_hpf;

        int m_underun_count;

        uint64_t m_chunk_time_stamp;
        uint64_t m_chunk_size_us;
        uint16_t m_frames_per_chunk;
    };
};
