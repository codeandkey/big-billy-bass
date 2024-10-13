
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
            pthread_mutex_unlock(&m_audioMutex);
            return -1;
        } else if (ret < 0) {
            ERROR("Failed to write audio data");
            pthread_mutex_unlock(&m_audioMutex);
            return -1;
        }
    }
    pthread_mutex_unlock(&m_audioMutex);
    return 0;
}

int b3::audioDriver::openDevice(char *deviceName, uint32_t sampleRate, uint8_t channels)
{
    if (m_deviceOpen) {
        WARNING("Audio device already open");
        return -1;
    }
    pthread_mutex_lock(&m_audioMutex);
    if (snd_pcm_open(&m_audioDevice, deviceName, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        ERROR("Failed to open audio device %s", deviceName);
        return -1;
    }

    snd_pcm_hw_params_alloc(&m_audioParams);
    snd_pcm_hw_params_any(m_audioDevice, m_audioParams);

    // set stream parameters
    snd_pcm_hw_params_set_access(m_audioDevice, m_audioParams, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(m_audioDevice, m_audioParams, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(m_audioDevice, m_audioParams, channels);
    snd_pcm_hw_params_set_rate_near(m_audioDevice, m_audioParams, &sampleRate, 0);

    // write parameters to driver
    if (snd_pcm_hw_params(m_audioDevice, m_audioParams) < 0) {
        ERROR("Failed to set hardware parameters");
        return -1;
    }

    if (snd_pcm_prepare(m_audioDevice) < 0) {
        ERROR("Failed to prepare audio device");
        return -1;
    }

    m_deviceOpen = true;
    pthread_mutex_unlock(&m_audioMutex);
    DEBUG("Opened audio device %s, bitrate %d, %d channels", deviceName, sampleRate, channels);
    return 0;
}
