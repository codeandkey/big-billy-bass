#include "signalProcessing.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gpio.h"
#include "audioFile.h"
#include "logger.h"


#define PCM_STEREO_TO_MONO(buffer, ndx)  (buffer[ndx] + buffer[ndx+1])/2

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

using namespace b3;

signalProcessor::~signalProcessor()
{}



void signalProcessor::update(State state)
{
    if (m_activeState != state)
        setState(state);

    if (m_activeState == State::PLAYING && !m_stopCommand) {

        uint64_t dt = usToNextChunk();
        usleep(MIN(dt, m_chunkSizeUs));

        if (m_fillBuffer) {
            m_fillBuffer = false;
            for (int i = 0; i < m_config.BUFFER_LENGTH_MS - 1; i++)
                _processChunk();
        }
        _processChunk();

        if (dt == 0) {
            m_underRunCounter = MIN(m_underRunCounter + 1, m_config.BUFFER_LENGTH_MS);
            if (m_underRunCounter == m_config.BUFFER_LENGTH_MS) {
                m_fillBuffer = true;
                DEBUG("Possible chunk underrun likely due to process timing: %d uS", m_tm.lastLap());
            }
        } else
            m_underRunCounter = MAX(m_underRunCounter - 1, 0);


    }

#ifdef DEBUG_FILTER_DATA
    if (m_closeFile) {
        fclose(m_signalDebugFile);
        m_closeFile = false;
    }
#endif 

    if (m_stopCommand)
        setState(STOPPED);
}



void signalProcessor::setState(State to)
{
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

        m_chunkTimestamp = m_tm.getUsSinceEpoch();
        m_tm.start();
        // set flags
        m_stopCommand = 0;
        m_fillBuffer = true;
#ifdef DEBUG_FILTER_DATA
        m_signalDebugFile = fopen("debugLpf.bin", "wb");
        if (!m_signalDebugFile)
            WARNING("failed to open " "debugLpf.bin" ": %s", strerror(errno));
#endif
        break;


    case State::STOPPED:
        unLoadFile();
        m_alsaDriver->closeDevice();
        break;
    case State::PAUSED:
        break; // no longer does anything

    }
    INFO("SignalProcessor State Transition: %d\n", m_activeState, to);
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
    for (int fltrNdx = 0; fltrNdx < biQuadFilter::_filterTypeCount; fltrNdx++)
        m_filters[fltrNdx] = new biQuadFilter(
            m_audioFile->getSampleRate(),
            m_filterSettings[fltrNdx],
            Q,
            GAIN,
            (biQuadFilter::filterType)fltrNdx
        );
}

void signalProcessor::_negotiateChunkSize()
{
    // this is the desired chunk size based on the audio file's settings
    m_chunkSize = m_audioFile->chunkSizeBytes(m_config.CHUNK_SIZE_MS);
    // see if we can set the alsa drivers to the same settings
    int chunkSizeFrames = m_chunkSize / m_audioFile->getChannels() / SPD::BYTES_PER_SAMPLE;
    DEBUG("Expected chunks size (frames/chunk) %d", chunkSizeFrames);

    int audioDriverChunkSize = m_alsaDriver->updateAudioChannelData(
        m_audioFile->getSampleRate(),
        m_audioFile->getChannels(),
        chunkSizeFrames
    );

    if (m_chunkSize != audioDriverChunkSize) {
        WARNING("Processing chunks size of %d bytes does not match with audio driver which configured to %d bytes", m_chunkSize, audioDriverChunkSize);
        m_chunkSize = audioDriverChunkSize;
    }

    if (m_audioFile->getChannels() > 0) {
        DEBUG("Setting final chunk size to %u", m_chunkSize);
        m_chunkSizeUs = m_chunkSize * 1e6 / m_audioFile->getSampleRate() / SPD::BYTES_PER_SAMPLE / m_audioFile->getChannels();
        DEBUG("Setting final uS chunk size to %llu", m_chunkSizeUs);
    }
}

uint64_t signalProcessor::usToNextChunk()
{
    uint64_t t = m_tm.getUsSinceEpoch();
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
    assert(m_filters[biQuadFilter::LPF] != nullptr);
    assert(m_filters[biQuadFilter::HPF] != nullptr);

    int channels = m_audioFile->getChannels();
    int sampleCount = m_chunkSize / SPD::BYTES_PER_SAMPLE / m_audioFile->getChannels();
    int16_t pcm16Buff[sampleCount * channels];  // this has multiple channels
    int16_t fltrSignal[biQuadFilter::_filterTypeCount][sampleCount];             // these are mono
    // read PCM16 data from the audio file
    int bytesRead = m_audioFile->readChunk((uint8_t *)pcm16Buff, m_chunkSize);


    // check for eof
    if (m_chunkSize == 0 || bytesRead < m_chunkSize || bytesRead == AVERROR_EOF) {
        INFO("EOF Detected");
        m_stopCommand = 1;
        if (bytesRead <= 0)
            return 0;
    }
    int samplesRead = bytesRead / SPD::BYTES_PER_SAMPLE / channels;

    for (int i = 0; i < (samplesRead * channels) - 1; i += channels) {
        // convert stereo to mono, apply filters
        for (int fltrNdx = 0; fltrNdx < biQuadFilter::_filterTypeCount; fltrNdx++)
            fltrSignal[fltrNdx][i / channels] = m_filters[fltrNdx]->update((float)PCM_STEREO_TO_MONO(pcm16Buff, i));

    }

    // GPIO API call
#ifndef DISABLE_GPIO
    GPIO::submitFrame(fltrSignal[biQuadFilter::LPF], fltrSignal[biQuadFilter::HPF], samplesRead);
#endif
    m_chunkTimestamp += m_chunkSizeUs;
    // usleep(100000);

    // write audio data to the audio driver
    m_alsaDriver->writeAudioData((uint8_t *)pcm16Buff, samplesRead);
    m_tm.lap();

#ifdef DEBUG_FILTER_DATA
    if (m_signalDebugFile)
        fwrite(fltrSignal[biQuadFilter::LPF], sizeof(fltrSignal[0][0]), samplesRead, m_signalDebugFile);
#endif

    return 0;
}
