#include "signalProcessing.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "audioFile.h"
#include "gpioStream.h"
#include "logger.h"


#define PCM_STEREO_TO_MONO(buffer, ndx)  (buffer[ndx] + buffer[ndx+1])/2

#define MIN(a,b) ((a < b) ? (a) : (b))

using namespace b3;

signalProcessor::~signalProcessor()
{}



void signalProcessor::frame(State state)
{
    assert(m_activeState == state);


    if (m_activeState == State::PLAYING && !m_stopCommand) {

        uint64_t dt = usToNextChunk();
        usleep(MIN(dt, m_chunkSizeUs * 9 / 10));

        if (m_fillBuffer) {
            m_fillBuffer = false;
            for (int i = 0; i < b3::SPD::CHUNK_COUNT - 1; i++)
                _processChunk();
        }
        _processChunk();

        if (dt == 0)
            DEBUG("Possible chunk underrun likely due to process timing");
    }

    if (m_stopCommand && m_fileClosed) {
        fclose(m_signalDebugFile);
        m_fileClosed = false;
    }
}



void signalProcessor::onTransition(State from, State to)
{
    // process State transitions
    if (m_activeState == to)
        return;

    assert(m_activeState == from);

    switch (to) {
    case State::PLAYING:
        if (!m_driverLoaded) {
            ERROR("audioProcessor - No audio driver loaded");
            return;
        }
        assert(m_alsaDriver);
        if (!m_fileLoaded) {
            ERROR("audioProcessor - No audio file loaded");
            return;
        }
        assert(m_audioFile);

        m_chunkTimestamp = timeManager::getUsSinceEpoch();

        // set flags
        m_stopCommand = 0;
        m_fillBuffer = true;

        m_signalDebugFile = fopen("debugLpf.bin", "wb");
        if (!m_signalDebugFile)
            WARNING("failed to open " "debugLpf.bin" ": %s", strerror(errno));
        break;
    case State::PAUSED:
        if (m_activeState == State::STOPPED) {
            if (!m_driverLoaded) {
                ERROR("audioProcessor - No audio driver loaded");
                return;
            }
            if (!m_fileLoaded) {
                ERROR("audioProcessor - No audio file loaded");
                return;
            }
        }
        break;

    case State::STOPPED:
        unLoadFile();
        m_alsaDriver->closeDevice();
        break;
    }

    INFO("SignalProcessor State Transition: %d -> %d\n", m_activeState, to);
    m_activeState = to;
}


void signalProcessor::setAudioDriver(audioDriver *driver)
{
    if (!driver) {
        ERROR("audioProcessor - Null audio driver pointer");
        return;
    }
    m_alsaDriver = driver;
    m_driverLoaded = true;
}

void signalProcessor::setFile(audioFile *F)
{
    if (!F) {
        ERROR("audioProcessor - Null audio file pointer");
        return;
    }
    if (m_fileLoaded) {
        WARNING("File Already Loaded, Unloading previous audio file");
        unLoadFile();
    }
    assert(F->getSampleRate() == SPD::DEFAULT_SAMPLE_RATE);

    m_audioFile = F;
    m_fileLoaded = true;

    _negotiateChunkSize();

    // create filters
    m_lpf = new biQuadFilter(m_audioFile->getSampleRate(), m_filterSettings.lpfCutoff, Q, GAIN, filterType::LPF_type);
    m_hpf = new biQuadFilter(m_audioFile->getSampleRate(), m_filterSettings.hpfCutoff, Q, GAIN, filterType::HPF_type);
}

void signalProcessor::_negotiateChunkSize()
{
    // this is the desired chunk size based on the audio file's settings
    m_chunkSize = m_audioFile->chunkSizeBytes(signalProcessingDefaults::CHUNK_SIZE_MS);
    // see if we can set the alsa drivers to the same settings
    int audioDriverChunkSize = m_alsaDriver->updateAudioChannelData(m_audioFile->getSampleRate(),
        m_audioFile->getChannels(),
        m_chunkSize);
    if (m_chunkSize != audioDriverChunkSize) {
        ERROR("Processing chunks size of %d bytes does not match with audio driver which configured to %d bytes", m_chunkSize, audioDriverChunkSize);
        m_chunkSize = 0;
    }

    if (m_audioFile->getChannels() > 0)
        m_chunkSizeUs = m_chunkSize / (SPD::BYTES_PER_SAMPLE * m_audioFile->getChannels());
}

uint64_t signalProcessor::usToNextChunk()
{
    uint64_t t = timeManager::getUsSinceEpoch();
    if (t > m_chunkTimestamp)
        return 0;

    return m_chunkTimestamp - t;
}

int signalProcessor::_processChunk()
{
    if (!m_fileLoaded) {
        ERROR("audioProcessor - No audio file loaded");
        return -1;
    }
    assert(m_audioFile != nullptr);
    assert(m_lpf != nullptr);
    assert(m_hpf != nullptr);

    int channels = m_audioFile->getChannels();
    int sampleCount = m_chunkSize / SPD::BYTES_PER_SAMPLE / m_audioFile->getChannels();

    int16_t pcm16Buff[sampleCount * channels];  // this has multiple channels
    int16_t lpfSignal[sampleCount];             // these are mono
    int16_t hpfSignal[sampleCount];             // these are mono
    // read PCM16 data from the audio file
    int bytesRead = m_audioFile->readChunk((uint8_t *)pcm16Buff, m_chunkSize);

    // check for eof
    if (bytesRead < m_chunkSize) {
        INFO("EOF Detected");
        m_stopCommand = 1;
        if (bytesRead <= 0)
            return 0;
    }
    int samplesRead = bytesRead / SPD::BYTES_PER_SAMPLE / channels;

    for (int i = 0; i < (samplesRead * 2) - 1; i += channels) {
// convert stereo to mono, apply filters
        lpfSignal[i / channels] = m_lpf->update((float)PCM_STEREO_TO_MONO(pcm16Buff, i));
        hpfSignal[i / channels] = m_hpf->update((float)PCM_STEREO_TO_MONO(pcm16Buff, i));
    }

    // GPIO API call
#ifndef DISABLE_GPIO
    GpioStream::get().submit(lpfSignal, hpfSignal, samplesRead, m_chunkTimestamp);
    m_chunkTimestamp += m_chunkSizeUs;
#endif
    // usleep(100000);

    // write audio data to the audio driver
    if (m_alsaDriver->writeAudioData((uint8_t *)pcm16Buff, samplesRead)) {
        // do something
    }

    if (m_signalDebugFile)
        fwrite(lpfSignal, sizeof(lpfSignal[0]), samplesRead, m_signalDebugFile);

    return 0;
}
