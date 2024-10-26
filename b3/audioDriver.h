#pragma once

#include <stdint.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#include "signalProcessingDefaults.h"



namespace b3 {
    namespace audioDriverDefaults {
        constexpr char DEFAULT_DEVICE[] = "default";

        constexpr _snd_pcm_format __get_default_format__()
        {
            switch (signalProcessingDefaults::DEFAULT_AUDIO_FORMAT) {
                case (signalProcessingDefaults::PCM_16):
                    return SND_PCM_FORMAT_S16;
                case (signalProcessingDefaults::PCM_24):
                    return SND_PCM_FORMAT_S24;
                case (signalProcessingDefaults::PCM_32):
                    return SND_PCM_FORMAT_S32;
                default:
                    return SND_PCM_FORMAT_UNKNOWN;
            }
        }

        constexpr _snd_pcm_format DEFAULT_OUTPUT_FORAMT = __get_default_format__();

    };


    class audioDriver {
    public:
        audioDriver() :
            m_audioDevice(nullptr),
            m_hardwareParams(nullptr),
            m_deviceOpen(false)
        {
            m_deviceName[0] = '\0';
            pthread_mutex_init(&m_audioMutex, nullptr);
        }
        ~audioDriver() { closeDevice(); }

        /**
         * @brief Opens an audio device with the specified parameters.
         *
         * This function initializes and configures an audio device for playback.
         * It sets various hardware parameters such as sample rate, channels, and buffer size.
         * If the device is already open, it will return an error.
         *
         * @param deviceName The name of the audio device to open.
         * @param sampleRate The sample rate of the audio in frames per second.
         * @param channels The number of audio channels (e.g., 1 for mono, 2 for stereo).
         * @param buffSize The buffer size in frames.
         * @return The size of the chunk in frames if successful, or a negative error code if failed.
         *
         * @note This function uses ALSA (Advanced Linux Sound Architecture) for audio device management.
         *
         * @warning Ensure that the device is not already open before calling this function.
         *
         */
        int openDevice(const char *deviceName, uint32_t sampleRate, uint8_t channels, uint64_t buffsize);

        /**
         * @brief Opens the default audio device with the specified parameters.
         *
         * This function initializes and configures an audio device for playback.
         * It sets various hardware parameters such as sample rate, channels, and buffer size.
         * If the device is already open, it will return an error.
         *
         * @param sampleRate The sample rate of the audio in frames per second.
         * @param channels The number of audio channels (e.g., 1 for mono, 2 for stereo).
         * @param buffSize The buffer size in frames.
         * @return The size of the chunk in frames if successful, or a negative error code if failed.
         *
         * @note This function uses ALSA (Advanced Linux Sound Architecture) for audio device management.
         *
         * @warning Ensure that the device is not already open before calling this function.
         *
         */
        inline int openDevice(uint32_t sampleRate, uint8_t channels, uint64_t buffsize) { return openDevice(audioDriverDefaults::DEFAULT_DEVICE, sampleRate, channels, buffsize); }

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
         * @return negotiated frame size in bytes
         */
        int updateAudioChannelData(int sampleRate, int channels, int buffersize);

        /**
         * @brief Writes audio data to the audio device.
         *
         * This function writes the provided audio data to the audio device if it is open.
         * It handles potential buffer underruns and errors by attempting to recover the audio device.
         *
         * @param data Pointer to the audio data to be written.
         * @param frameCount Number of frames of audio data to write.
         * @return int Returns 0 on success, or a negative error code on failure.
         */
        int writeAudioData(uint8_t *data, int size);

    private:
        snd_pcm_t *m_audioDevice;
        snd_pcm_hw_params_t *m_hardwareParams;

        pthread_mutex_t m_audioMutex;
        bool m_deviceOpen;

        char m_deviceName[255];     //todo get rid of magic number
    }; // class audioDriver
}; // namespace b3

