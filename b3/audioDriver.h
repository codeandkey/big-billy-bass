#pragma once


extern "C" {
#include <stdint.h>
#include <pthread.h>

#ifndef DUMMY_ALSA_DRIVERS
#include <alsa/asoundlib.h>
#endif
}
#include "signalProcessingDefaults.h"

namespace b3 {
    namespace audioDriverDefaults {
        namespace SPD = signalProcessingDefaults;
        constexpr const char *DEFAULT_DEVICE = "default";
#ifndef DUMMY_ALSA_DRIVERS
        constexpr _snd_pcm_format OUTPUT_FORMAT = SPD::__default_fmt_selector(
            SND_PCM_FORMAT_S16,
            SND_PCM_FORMAT_S32,
            SND_PCM_FORMAT_FLOAT,
            SND_PCM_FORMAT_FLOAT64
        );
#endif
    };

    class audioDriver {
    public:
        audioDriver() :
#ifndef DUMMY_ALSA_DRIVERS
            m_audioDevice(nullptr),
            m_hardwareParams(nullptr),
#endif
            m_deviceOpen(false)
        {
            m_deviceName[0] = '\0';
            pthread_mutex_init(&m_audioMutex, nullptr);
        }
        ~audioDriver() { close_driver(); }

        /**
         * @brief
         *
         * Opens the alsa device specified by dev_name. Initializes alsa context.
         * @param dev_name ALSA device to open
         * @param sample_rate Desired sample rate for playback
         * @param channels Desired channesl for playback
         * @param frames_per_chunk Desired chunks size in frames/chunk
         * @return 0 on success, 1 on failure
         */
        int open_driver(const char *dev_name, uint32_t sample_rate, pcm_t channels, uint64_t frames_per_chunk);

        /**
         * @brief
         *
         * Opens the `default` alsa device specified by `audioDriverDefaults::DEFAULT_DEVICE`. Initializes alsa context.
         * @param sample_rate Desired sample rate for playback
         * @param channels Desired channesl for playback
         * @param frames_per_chunk Desired chunks size in frames/chunk
         * @return 0 on success, 1 on failure
         */
        inline int open_driver(uint sample_rate, uint channels, size_t frame_per_chunk)
        {
            return open_driver(audioDriverDefaults::DEFAULT_DEVICE, sample_rate, channels, frame_per_chunk);
        }

        /**
         * @brief
         * Closes the audio device and frees alsa context
         */
        void close_driver();

        /**
         * @brief 
         * 
         * Re-configured alsa hw params. This will call `close_driver()` returns an `open_driver()` call.
         * @param sample_rate Desired sample rate for playback
         * @param channels Desired channesl for playback
         * @param frames_per_chunk Desired chunks size in frames/chunk
         * @return 0 on success, 1 on failure
         */
        int set_output_params(int sampleRate, int channels, int frames_per_chunk);

        /**
         * @brief 
         * Writes `frame_count` samples of pcm data from `data` to alsa device.
         * @param data buffer to write from.
         * @param frame_count Samples to write. This is independent of channel count, so buffer should be scaled up based on the channel count.
         * @return 
         */
        int write_chunk(pcm_t *data, size_t frame_count);

    private:
#ifndef DUMMY_ALSA_DRIVERS
        snd_pcm_t *m_audioDevice;
        snd_pcm_hw_params_t *m_hardwareParams;
#endif
        pthread_mutex_t m_audioMutex;
        bool m_deviceOpen;

        char m_deviceName[255];     //todo get rid of magic number
    }; // class audioDriver
}; // namespace b3

