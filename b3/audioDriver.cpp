
#include "audioDriver.h"

#include <cassert>

#include "logger.h"
#include "signalProcessingDefaults.h"

using namespace b3;


void b3::audioDriver::closeDevice()
{
    pthread_mutex_lock(&m_audioMutex);
    if (m_deviceOpen) {
        snd_pcm_drain(m_audioDevice);
        snd_pcm_close(m_audioDevice);
        snd_pcm_hw_params_free(m_hardwareParams);
        m_audioDevice = nullptr;
        m_hardwareParams = nullptr;
        m_deviceOpen = false;
    }
    assert(!m_audioDevice);
    assert(!m_hardwareParams);
    pthread_mutex_unlock(&m_audioMutex);
}

int b3::audioDriver::updateAudioChannelData(int sampleRate, int channels, int bufferSize)
{
    // note: everything here is already thread safe.
    if (m_deviceOpen)
        closeDevice();
        
    assert(!m_audioDevice);

    return openDevice(DEFAULT_DEVICE, sampleRate, channels, bufferSize);
}

int b3::audioDriver::writeAudioData(uint8_t *data, int frameCount)
{
    pthread_mutex_lock(&m_audioMutex);
    int ret;
    if (m_deviceOpen) {
        ret = snd_pcm_writei(m_audioDevice, data, frameCount);
        if (ret == -EPIPE)
            WARNING("Audio buffer underrun");
        else if (ret < 0)
            ERROR("Failed to write audio data %s:", snd_strerror(ret));

        if (ret < 0) {
            snd_pcm_recover(m_audioDevice, ret, 0);
            pthread_mutex_unlock(&m_audioMutex);
            return ret;
        }
    }
    pthread_mutex_unlock(&m_audioMutex);
    return 0;
}

int b3::audioDriver::openDevice(const char *deviceName, uint32_t bitrate, uint8_t channels, uint64_t buffSize)
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
        WARNING("Audio device already open");
        return -1;
    }
    pthread_mutex_lock(&m_audioMutex);
    int err = 0;
    uint32_t chnls, rate;
    uint64_t chunkSize;

    if (err = snd_pcm_open(&m_audioDevice, deviceName, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        ERROR("Failed to open audio device %s", deviceName);
        goto badInitCleanup;
    }

    if (err = snd_pcm_hw_params_malloc(&m_hardwareParams) < 0) goto badInitCleanup;
    if (err = snd_pcm_hw_params_any(m_audioDevice, m_hardwareParams) < 0) goto badInitCleanup;
    // set stream parameters
    if (err = snd_pcm_hw_params_set_access(m_audioDevice, m_hardwareParams, SND_PCM_ACCESS_RW_INTERLEAVED))     goto badInitCleanup;
    if (err = snd_pcm_hw_params_set_format(m_audioDevice, m_hardwareParams, SND_PCM_FORMAT_S16_LE) < 0)         goto badInitCleanup;
    if (err = snd_pcm_hw_params_set_channels(m_audioDevice, m_hardwareParams, channels) < 0)                    goto badInitCleanup;
    if (err = snd_pcm_hw_params_set_rate_near(m_audioDevice, m_hardwareParams, &bitrate, 0) < 0)                goto badInitCleanup;
    if (err = snd_pcm_hw_params_set_period_size_near(m_audioDevice, m_hardwareParams, &buffSize, 0) < 0)        goto badInitCleanup;
    // write parameters to driver
    if (err = snd_pcm_hw_params(m_audioDevice, m_hardwareParams) < 0) {
        ERROR("Failed to set hardware parameters");
        goto badInitCleanup;
    }
    snd_pcm_hw_params_free(m_hardwareParams);

    if (err = snd_pcm_prepare(m_audioDevice) < 0) {
        ERROR("Failed to prepare audio device");
        goto badInitCleanup;
    }

    m_deviceOpen = true;
    snd_pcm_hw_params_get_period_size(m_hardwareParams, &chunkSize, 0);
    snd_pcm_hw_params_get_channels(m_hardwareParams, &chnls);
    snd_pcm_hw_params_get_rate(m_hardwareParams, &rate, 0);
    pthread_mutex_unlock(&m_audioMutex);

    DEBUG("Opened audio device %s", snd_pcm_name(m_audioDevice));
    DEBUG("--%d Hz (%d bps)", bitrate / 8 / channels / 2, bitrate);
    DEBUG("--%d channels, %d frames/chunk (%d bytes)", channels, chunkSize / channels / 2, chunkSize);
    DEBUG("--%d ms chunks", chunkSize * 8 * 1000 / bitrate);
    return chunkSize;


badInitCleanup:
    ERROR("%s", strerror(err));
    if (m_audioDevice) {
        snd_pcm_drain(m_audioDevice);
        snd_pcm_close(m_audioDevice);
    }
    if (m_hardwareParams)
        snd_pcm_hw_params_free(m_hardwareParams);

    pthread_mutex_unlock(&m_audioMutex);
    return err;
}
