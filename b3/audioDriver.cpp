
#include "audioDriver.h"

#include <cassert>

#include "logger.h"
#include "signalProcessingDefaults.h"
#include "timeManager.h"

using namespace b3;
using namespace audioDriverDefaults;



void b3::audioDriver::closeDevice()
{
    timeManager tm;
    pthread_mutex_lock(&m_audioMutex);
    if (m_deviceOpen) {
#ifndef DUMMY_ALSA_DRIVERS
        // snd_pcm_drain(m_audioDevice);
        snd_pcm_close(m_audioDevice);
        m_audioDevice = nullptr;
        m_hardwareParams = nullptr;
#endif
        m_deviceOpen = false;
    }
#ifndef DUMMY_ALSA_DRIVERS
    assert(!m_audioDevice);
    assert(!m_hardwareParams);
#endif
    pthread_mutex_unlock(&m_audioMutex);
    DEBUG("Audio Driver %llu", tm.lap());
}

int b3::audioDriver::updateAudioChannelData(int sampleRate, int channels, int bufferSize)
{
    // note: everything here is already thread safe.
    if (m_deviceOpen)
        closeDevice();
#ifndef DUMMY_ALSA_DRIVERS
    assert(!m_audioDevice);
#endif
    return openDevice(DEFAULT_DEVICE, sampleRate, channels, bufferSize);
}

int b3::audioDriver::writeAudioData(uint8_t *data, int frameCount)
{
    pthread_mutex_lock(&m_audioMutex);
    int ret;
    if (m_deviceOpen) {
#ifndef DUMMY_ALSA_DRIVERS
        ret = snd_pcm_writei(m_audioDevice, data, frameCount);
        if (ret == -EPIPE)
            WARNING("Audio buffer underrun");
        else if (ret < 0)
            ERROR("Failed to write audio data %s", snd_strerror(ret));

        if (ret < 0) {
            snd_pcm_recover(m_audioDevice, ret, 0);
            pthread_mutex_unlock(&m_audioMutex);
            return ret;
        }
#endif
    }
    pthread_mutex_unlock(&m_audioMutex);
    return 0;
}

int b3::audioDriver::openDevice(const char *deviceName, uint32_t sampleRate, uint8_t channels, uint64_t samplesPerChunk)
{
    /**
     * Clearing up some nomenclature here becuase I got very confused and it lead to some bugs
     *
     * audio rate: sample rate of the audio file. Usually in bits/second (bps)
     * Channels: # of channels (stereo vs mono usually) in an audio file
     * Frame: A single audio sample for both left and right channels. If in PCM16, then a sample for a SINGLE channel is 2 bytes (1 16bit int)
     * Period: # of frames expected per chunk in an audio file
     * Chunk: a chunk of data used to buffer frames to the audio drivers
     */


    if (m_deviceOpen) {
        WARNING("Audio device already open, close device before opening a new one");
        return -1;
    }
    pthread_mutex_lock(&m_audioMutex);
    int err = 0;
    uint32_t chnls, rate, frameRate;
    uint64_t chunkSize;
    int chunkSizeBytes;
    

#ifndef DUMMY_ALSA_DRIVERS
    if ((err = snd_pcm_open(&m_audioDevice, deviceName, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        ERROR("Failed to open audio device %s", deviceName);
        goto badInitCleanup;
    }

    if ((err = snd_pcm_hw_params_malloc(&m_hardwareParams)) < 0)
        goto badInitCleanup;
    if ((err = snd_pcm_hw_params_any(m_audioDevice, m_hardwareParams)) < 0)
        goto badInitCleanup;
    // set stream parameters
    if ((err = snd_pcm_hw_params_set_access(m_audioDevice, m_hardwareParams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        goto badInitCleanup;
    if ((err = snd_pcm_hw_params_set_format(m_audioDevice, m_hardwareParams, DEFAULT_OUTPUT_FORAMT)) < 0)
        goto badInitCleanup;
    if ((err = snd_pcm_hw_params_set_channels(m_audioDevice, m_hardwareParams, channels)) < 0)
        goto badInitCleanup;

    frameRate = sampleRate;
    if ((err = snd_pcm_hw_params_set_rate_near(m_audioDevice, m_hardwareParams, &frameRate, 0)) < 0)
        goto badInitCleanup;
    if ((err = snd_pcm_hw_params_set_period_size_near(m_audioDevice, m_hardwareParams, &samplesPerChunk, 0)) < 0)
        goto badInitCleanup;
    // write parameters to driver
    if ((err = snd_pcm_hw_params(m_audioDevice, m_hardwareParams)) < 0) {
        goto badInitCleanup;
    }

    if ((err = snd_pcm_prepare(m_audioDevice)) < 0) {
        goto badInitCleanup;
    }

    m_deviceOpen = true;
    snd_pcm_hw_params_get_period_size(m_hardwareParams, &chunkSize, 0);
    snd_pcm_hw_params_get_channels(m_hardwareParams, &chnls);
    snd_pcm_hw_params_get_rate(m_hardwareParams, &rate, 0);
    pthread_mutex_unlock(&m_audioMutex);
    snd_pcm_hw_params_free(m_hardwareParams);

    chunkSizeBytes = chunkSize * chnls * signalProcessingDefaults::BYTES_PER_SAMPLE;

    DEBUG("Opened audio device %s", snd_pcm_name(m_audioDevice));
    DEBUG("--%d Hz (%d bps)", rate, rate * 8 * chnls * signalProcessingDefaults::BYTES_PER_SAMPLE);
    DEBUG("--%d channels, %d frames/chunk (%d bytes)", chnls, chunkSize, chunkSizeBytes);
    DEBUG("--%d ms chunks", chunkSize * 1000 / signalProcessingDefaults::DEFAULT_SAMPLE_RATE);

#endif 
    return chunkSizeBytes;

#ifndef DUMMY_ALSA_DRIVERS
badInitCleanup:
    ERROR("%s", snd_strerror(err));
    if (m_audioDevice) {
        snd_pcm_drain(m_audioDevice);
        snd_pcm_close(m_audioDevice);
    }
    if (m_hardwareParams)
        snd_pcm_hw_params_free(m_hardwareParams);

    pthread_mutex_unlock(&m_audioMutex);
    return err;
#endif
}
