#pragma once

#include <stdint.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

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
        int openDevice(const char *deviceName, uint32_t sampleRate, uint8_t channels);

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
         * @return 0 on success, error ( < 0) on failures
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

