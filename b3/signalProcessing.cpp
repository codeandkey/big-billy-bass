#include "signalProcessing.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "audioFile.h"
#include "gpioStream.h"
#include "logger.h"


#define U8_TO_U16_LE(a, b) ((uint16_t)(((uint16_t)a << 8) | b))
#define PCM_STEREO_TO_MONO(buffer, ndx)  (\
    (U8_TO_U16_LE(buffer[ndx], buffer[ndx + 1]) +  \
    U8_TO_U16_LE(buffer[ndx + 2], buffer[ndx + 3])) / 2) 

#define MIN(a,b) ((a < b) ? (a) : (b))

using namespace b3;

signalProcessor::~signalProcessor()
{}



void signalProcessor::frame(State state)
{
    assert(m_activeState == state);


    if (m_activeState == State::PLAYING && !m_stopCommand) {

        uint64_t dt = usToNextChunk();
        usleep(MIN(dt, CHUNK_SIZE_MS * 1000 * 9 / 10));

        if (m_fillBuffer) {
            m_fillBuffer = false;
            for (int i = 0; i < CHUNK_COUNT - 1; i++)
                _processChunk();
        }
        _processChunk();

        if (dt == 0)
            DEBUG("Possible chunk underrun likely due to process timing");
    }
    
}



void signalProcessor::onTransition(State from, State to)
{
    // process State transitions
    if (m_activeState == to)
        return;

    assert(m_activeState == from);

    int pcm16BitRate;

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

        // need to calculate new bitrate since we are converting to pcm16 data
        pcm16BitRate = m_audioFile->getSampleRate() * m_audioFile->getChannels() * 8 * 2;
        // chunk size is re-negotiated here
        m_chunkSize = m_alsaDriver->updateAudioChannelData(pcm16BitRate,
            m_audioFile->getChannels(),
            m_chunkSize);

        // set
        m_chunkSizeMS = m_chunkSize * 1000 / 4 / m_audioFile->getSampleRate();
        m_chunkTimestamp = timeManager::getUsSinceEpoch();
        
        // set flags
        m_stopCommand = 0;
        m_fillBuffer = true;
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
    m_audioFile = F;
    m_fileLoaded = true;
    m_chunkSize = m_audioFile->chunkSizeBytes();

    // create filters
    m_lpf = new biQuadFilter(m_audioFile->getSampleRate(), m_filterSettings.lpfCutoff, Q, GAIN, filterType::LPF_type);
    m_hpf = new biQuadFilter(m_audioFile->getSampleRate(), m_filterSettings.hpfCutoff, Q, GAIN, filterType::HPF_type);
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

    int bytes2pcm16 = m_audioFile->getChannels() * sizeof(int16_t) / sizeof(uint8_t);

    uint8_t pcm16Buff[m_chunkSize];
    int16_t lpfSignal[m_chunkSize / bytes2pcm16];
    int16_t hpfSignal[m_chunkSize / bytes2pcm16];
    // read PCM16 data from the audio file
    int bytesRead = m_audioFile->readChunk(pcm16Buff, m_chunkSize);

    // check for eof
    if (bytesRead <= 0) {
        INFO("EOF Detected");
        m_stopCommand = 1;
        return 0;
    }

    for (int i = 0; i < bytesRead; i += 4) {
        // convert stereo to mono, apply filters
        lpfSignal[i / 4] = m_lpf->update((float)PCM_STEREO_TO_MONO(pcm16Buff, i));
        hpfSignal[i / 4] = m_hpf->update((float)PCM_STEREO_TO_MONO(pcm16Buff, i));
    }

    // GPIO API call
    GpioStream::get().submit(lpfSignal, hpfSignal, m_chunkSize / bytes2pcm16, m_chunkTimestamp);
    m_chunkTimestamp += m_chunkSizeMS;

    // write audio data to the audio driver
    if (m_alsaDriver->writeAudioData(pcm16Buff, bytesRead / bytes2pcm16)) {
        // do something
    }

    return 0;
}
