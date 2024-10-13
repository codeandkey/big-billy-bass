#pragma once

#include <stdint.h>
#include <pthread.h>

#ifdef __WIN32__
// dummy definitions for windows
struct snd_pcm_t;
struct snd_pcm_hw_params_t;
void snd_pcm_hw_params_alloc(snd_pcm_hw_params_t **) {}
void snd_pcm_hw_params_any(snd_pcm_t *, snd_pcm_hw_params_t *) {}
void snd_pcm_hw_params_set_access(snd_pcm_t *, snd_pcm_hw_params_t *, int) {}
void snd_pcm_hw_params_set_format(snd_pcm_t *, snd_pcm_hw_params_t *, int) {}
void snd_pcm_hw_params_set_channels(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int) {}
void snd_pcm_hw_params_set_rate_near(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int *, int *) {}
int snd_pcm_hw_params(snd_pcm_t *, snd_pcm_hw_params_t *) { return 0; }
int snd_pcm_prepare(snd_pcm_t *) { return 0; }
int snd_pcm_open(snd_pcm_t **, const char *, int, int) { return 0; }
int snd_pcm_writei(snd_pcm_t *, const void *, unsigned long) { return 0; }
int snd_pcm_drain(snd_pcm_t *) { return 0; }
int snd_pcm_close(snd_pcm_t *) { return 0; }
void snd_pcm_hw_params_free(snd_pcm_hw_params_t *) {}
enum salsa_defs {
    SND_PCM_STREAM_PLAYBACK,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    SND_PCM_FORMAT_S16_LE,
};

#else
#include <alsa/asoundlib.h>
#endif
#define DEFAULT_DEVICE "default"



namespace b3 {
    class audioDriver {
    public:
        audioDriver() :
            m_audioDevice(nullptr),
            m_audioParams(nullptr),
            m_deviceOpen(false)
        {
            m_deviceName[0] = '\0';
            pthread_mutex_init(&m_audioMutex, nullptr);
        }
        ~audioDriver() { closeDevice(); }

        /**
         * @brief
         * Opens audio device. Thread safe.
         *
         * @param deviceName device name to open
         * @param sampleRate sample rate to open device at
         * @param channels # of audio channels
         * @return 0 on success, -1 on failure
         */
        int openDevice(char *deviceName, uint32_t sampleRate, uint8_t channels);

        /**
         * @brief
         * Opens the default audio device. Thread safe.
         *
         * @param sampleRate sample rate to open device at
         * @param channels # of audio channels
         * @return 0 on success, -1 on failure
         */
        inline int openDevice(uint32_t sampleRate, uint8_t channels) { return openDevice(DEFAULT_DEVICE, sampleRate, channels); }

        /**
         * @brief
         * Closes the audio device. Thread safe.
         */
        void closeDevice();

        /**
         * @brief
         * Updates the audio device with new channel data. Thread safe.
         *
         * @param sampleRate new sample rate
         * @param channels new # of audio channels
         */
        void updateAudioChannelData(int sampleRate, int channels);

        /**
         * @brief
         * Writes audio data to the audio device. Thread safe.
         *
         * @param data audio data to write
         * @param size size of data in bytes
         * @return # of bytes written
         */
        int writeAudioData(uint8_t *data, int size);

    private:
        snd_pcm_t *m_audioDevice;
        snd_pcm_hw_params_t *m_audioParams;

        pthread_mutex_t m_audioMutex;
        bool m_deviceOpen;

        char m_deviceName[255];     //todo get rid of magic number
    }; // class audioDriver
}; // namespace b3

