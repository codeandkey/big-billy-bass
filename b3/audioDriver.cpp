
#include "audioDriver.h"

#include <cassert>

#include "logger.h"


using namespace b3;


void b3::audioDriver::closeDevice()
{
    pthread_mutex_lock(&m_audioMutex);
    if (m_deviceOpen) {
        snd_pcm_drain(m_audioDevice);
        snd_pcm_close(m_audioDevice);
        snd_pcm_hw_params_free(m_audioParams);
        m_audioDevice = nullptr;
        m_audioParams = nullptr;
        m_deviceOpen = false;
    }
    assert(!m_audioDevice);
    assert(!m_audioParams);
    pthread_mutex_unlock(&m_audioMutex);

}

void b3::audioDriver::updateAudioChannelData(int sampleRate, int channels)
{
    // note: everything here is already thread safe.
    if (m_deviceOpen)
        closeDevice();

    openDevice(DEFAULT_DEVICE, sampleRate, channels);
}

int b3::audioDriver::writeAudioData(uint8_t *data, int size)
{
    pthread_mutex_lock(&m_audioMutex);
    int ret;
    if (m_deviceOpen) {
        ret = snd_pcm_writei(m_audioDevice, data, size);
        if (ret == -EPIPE) {
            ERROR("Audio buffer overrun");
            snd_pcm_prepare(m_audioDevice);
            pthread_mutex_unlock(&m_audioMutex);
            return -EPIPE;
        } else if (ret < 0) {
            ERROR("Failed to write audio data %s:", strerror(ret));
            pthread_mutex_unlock(&m_audioMutex);
            return -1;
        }
    }
    pthread_mutex_unlock(&m_audioMutex);
    return 0;
}

int b3::audioDriver::openDevice(const char *deviceName, uint32_t sampleRate, uint8_t channels)
{
    if (m_deviceOpen) {
        WARNING("Audio device already open");
        return -1;
    }
    pthread_mutex_lock(&m_audioMutex);
    int err = 0;

    if (err = snd_pcm_open(&m_audioDevice, deviceName, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        ERROR("Failed to open audio device %s", deviceName);
        goto badInitCleanup;
    }
    if (err = snd_pcm_hw_params_malloc(&m_audioParams) < 0) goto badInitCleanup;
    if (err = snd_pcm_hw_params_any(m_audioDevice, m_audioParams) < 0) goto badInitCleanup;
    // set stream parameters
    if (err = snd_pcm_hw_params_set_access(m_audioDevice, m_audioParams, SND_PCM_ACCESS_RW_INTERLEAVED))    goto badInitCleanup;
    if (err = snd_pcm_hw_params_set_format(m_audioDevice, m_audioParams, SND_PCM_FORMAT_S16_LE) < 0)        goto badInitCleanup;
    if (err = snd_pcm_hw_params_set_channels(m_audioDevice, m_audioParams, channels) < 0)                   goto badInitCleanup;
    if (err = snd_pcm_hw_params_set_rate_near(m_audioDevice, m_audioParams, &sampleRate, 0) < 0)            goto badInitCleanup;

    // write parameters to driver
    if (err = snd_pcm_hw_params(m_audioDevice, m_audioParams) < 0) {
        ERROR("Failed to set hardware parameters");
        goto badInitCleanup;
    }

    if (err = snd_pcm_prepare(m_audioDevice) < 0) {
        ERROR("Failed to prepare audio device");
        goto badInitCleanup;
    }

    m_deviceOpen = true;
    pthread_mutex_unlock(&m_audioMutex);
    DEBUG("Opened audio device %s, bitrate %d, %d channels", deviceName, sampleRate, channels);
    return 0;

badInitCleanup:
    ERROR("%s", strerror(err));
    if (m_audioDevice) {
        snd_pcm_drain(m_audioDevice);
        snd_pcm_close(m_audioDevice);
    }
    if (m_audioParams)
        snd_pcm_hw_params_free(m_audioParams);

    pthread_mutex_unlock(&m_audioMutex);
    return err;
}
