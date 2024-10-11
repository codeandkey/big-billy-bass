#include "signalProcessing.h"
#include "gpioStream.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <libavformat/avformat.h>


#define FFMPEG "ffmpeg"
#define SAMPLE_RATE "'([0-9]+) Hz'"
#define CHANNELS "\"stereo|mono\""

#define DEFAULT_CODEC "s16le"
#define ACODEC "pcm_s16le"

#define FFMPEG_COMMAND_PREFIX FFMPEG " -i " 
#define FFMPEG_INFO_COMMAND_SUFFIX(DATA) " 2>&1 | grep Stream | grep -oP " DATA " | head -1"


#define U8_TO_U16_LE(a, b) ((uint16_t)(((uint16_t)a << 8) | b))
#define PCM_STEREO_TO_MONO(buffer, ndx)  (\
    (U8_TO_U16_LE(buffer[ndx], buffer[ndx + 1]) +  \
    U8_TO_U16_LE(buffer[ndx + 2], buffer[ndx + 3])) / 2) 


using namespace b3;

audioProcessor::~audioProcessor()
{
    _cleanUp();
}

void b3::audioProcessor::frame(State state)
{
    if (m_activeState != state)
        requestState(state);

    if (m_activeState != state) {
        printf("Error: State Transition Failed\n");
        return;
    }

    usleep(usToNextChunk() * 95 / 100);
    update();
}

void b3::audioProcessor::onTransition(State from, State to)
{
    requestState(to);
}


void audioProcessor::requestState(State command)
{
    // process State transitions
    switch (command) {
    case State::PLAYING:
        if (m_activeState == State::PAUSED || m_activeState == State::STOPPED) {
            m_activeState = State::PLAYING;
            m_fillBuffer = true;
            m_loadFile = true;
	    m_chunkTimestamp = timeManager::getUsSinceEpoch();
        }
        break;
    case State::PAUSED:
        if (m_activeState == State::PLAYING) {
            m_activeState = State::PAUSED;
        } else if (m_activeState == State::STOPPED) {
            m_activeState = State::PAUSED;
        }
        break;
    case State::STOPPED:
        if (m_activeState == State::PLAYING || m_activeState == State::PAUSED) {
            m_activeState = State::STOPPED;
        }
        break;
    }
}

int audioProcessor::update()
{
    switch (m_activeState) {
    case State::PLAYING:
        if (m_loadFile) {
            m_loadFile = false;
            if (_openFile(m_inputFileName) < 0)
                return -1;

        }
        if (m_fillBuffer) {
            m_fillBuffer = false;
            // pre-buffer data
            for (int i = 0; i < CHUNK_COUNT - 1; i++)
                _processChunk();
        }
        _processChunk();
        break;
    case State::PAUSED:
        // do nothing
        break;

    case State::STOPPED:
        if (m_fileLoaded) {
            m_fileLoaded = false;
        }
        break;
    }

    return 0;
}

int audioProcessor::_readChunk(uint8_t *buffer, FILE *fp, int readSize)
{
    // read until the current chunk is filled
    int currRead = 0;
    int totalRead = 0;
    uint8_t readCount = 0;
    while ((currRead = fread(&buffer[readSize], 1, readSize - totalRead, fp)) > 0 &&
        totalRead < readSize) {
        totalRead += currRead;
        readCount++;
    }
    return totalRead;
}

uint64_t audioProcessor::usToNextChunk()
{
    uint64_t t = timeManager::getUsSinceEpoch();
    if (t > m_chunkTimer + CHUNK_SIZE_MS * 1000)
        return 0;

    return m_chunkTimer + CHUNK_SIZE_MS * 1000 - t;
}

void audioProcessor::_processChunk()
{
    uint8_t buffer2filteredSignal = (sizeof(uint16_t) / sizeof(uint8_t) * m_activeFile.channels);
    uint8_t buffer[m_chunkSize];
    int16_t lpfSignal[m_chunkSize / buffer2filteredSignal];
    int16_t hpfSignal[m_chunkSize / buffer2filteredSignal];
    int16_t leftChannel, rightChannel;

    int readSize = _readChunk(buffer, m_activePipes.decodePipe, m_chunkSize);

    // combine the left and right channels
    int ndx = 0;
    for (int i = 0; i < readSize; i += 4) {
        // average the left and right channels
        hpfSignal[ndx] = PCM_STEREO_TO_MONO(buffer, i);
        ndx++;
    }

    if (readSize > 0) {
        // write the center channel to each filter pipes
        fwrite(hpfSignal, sizeof(hpfSignal[0]), ndx, m_activePipes.hpfPipe);
        fwrite(hpfSignal, sizeof(lpfSignal[0]), ndx, m_activePipes.lpfPipe);
    }
    int lpfRead = _readChunk((uint8_t *)lpfSignal, m_activePipes.lpfPipe, m_chunkSize);
    int hpfRead = _readChunk((uint8_t *)hpfSignal, m_activePipes.hpfPipe, m_chunkSize);

    m_chunkTimer = timeManager::getUsSinceEpoch();
    GpioStream::get().submit(lpfSignal, hpfSignal, m_chunkSize, m_chunkTimestamp);
    m_chunkTimestamp += CHUNK_SIZE_MS * 1000;

    fwrite(buffer, 1, readSize, m_activePipes.playPipe);
}


int audioProcessor::_openFile(char *inputFile)
{
#ifdef TEST_AUDIO
#warning Overriding input file with test audio
    inputFile = (char*) TEST_AUDIO;
#endif // TEST_AUDIO

    _cleanUp();

    if (m_fileLoaded) {
        perror("File Already Loaded\n");
        return -1;
    }

    FILE *fp = fopen(inputFile, "rb");
    if (fp == nullptr) {
        perror("Error Opening File\n");
        return -1;
    }
    fclose(fp);

#ifdef INCLUDE_FFMPEG_LIBS
    if ((m_usePipes && audioProcessor::pipeFileInfo(inputFile, m_activeFile) < 0) ||
        (!m_usePipes && audioProcessor::getFileInfo(inputFile, m_activeFile) < 0))
        return -1;
#else
    audioProcessor::pipeFileInfo(inputFile, m_activeFile);
#endif // INCLUDE_FFMPEG_LIBS

    m_fileLoaded = true;
    // determine buffer size
    m_chunkSize = m_activeFile.sampleRate * CHUNK_SIZE_MS / 1000 * m_activeFile.channels;

    // initialize the audio streams
    if (initAudioStreams(inputFile) < 0)
        return -1;

    return 0;
}


int audioProcessor::initAudioStreams(char *inputFile)
{
    char ffmpegCmdLpf[512];
    char ffmpegCmdHpf[512];
    char ffmpegCmdPLAYING[512];
    char ffmpegCmdDecode[512];

    // Create FFmpeg command to decode audio file to raw PCM
    sprintf(ffmpegCmdDecode,
        "" FFMPEG_COMMAND_PREFIX "\"%s\" -f " DEFAULT_CODEC " -acodec " ACODEC " ar %hu -ac %hu - ",
        inputFile, m_activeFile.sampleRate, m_activeFile.channels);

    /** Create FFmpeg command to PLAYING raw PCM with low-pass filter
     *  - Note that a single channel is specified for the filtered audio since this is intended to drive a motor
     */
    sprintf(ffmpegCmdLpf,
        "" FFMPEG " -f " DEFAULT_CODEC " -ar %hu -ac 1 -i - -af \"lowpass=%d\" -f ",
        m_activeFile.sampleRate, m_filterSettings.lpfCutoff);

    /** Create FFmpeg command to PLAYING raw PCM with a high-pass filter
     * - Note that again a single channel is specified for the filtered audio since this is intended to drive a motor
     */
    sprintf(ffmpegCmdHpf,
        "" FFMPEG " -f " DEFAULT_CODEC " -ar %hu -ac 1 -i - -af \"highpass=%d\" -f ",
        m_activeFile.sampleRate, m_filterSettings.hpfCutoff);

    sprintf(ffmpegCmdPLAYING,
        "" FFMPEG " -f " DEFAULT_CODEC " -ar %hu -ac %hu -i - -f alsa default - | " "ffPLAYING -",
        m_activeFile.sampleRate, m_activeFile.channels);

    m_activePipes.lpfPipe = popen(ffmpegCmdLpf, "r+");
    m_activePipes.hpfPipe = popen(ffmpegCmdHpf, "r+");
    m_activePipes.decodePipe = popen(ffmpegCmdDecode, "r");
    m_activePipes.playPipe = popen(ffmpegCmdPLAYING, "r+");

    if (!m_activePipes.decodePipe || !m_activePipes.lpfPipe || !m_activePipes.hpfPipe || !m_activePipes.playPipe) {
        if (m_activePipes.decodePipe) pclose(m_activePipes.decodePipe);
        if (m_activePipes.lpfPipe) pclose(m_activePipes.lpfPipe);
        if (m_activePipes.hpfPipe) pclose(m_activePipes.hpfPipe);
        if (m_activePipes.playPipe) pclose(m_activePipes.playPipe);
        perror("Error Opening Pipes\n");
        return -1;
    }
    m_activePipes.m_flags = 0x0F;

    return 0;
}



int audioProcessor::pipeFileInfo(char *inputFile, audioFileSettings &F)
{
    char command[256];
    sprintf(command, "" FFMPEG_COMMAND_PREFIX "%s" FFMPEG_INFO_COMMAND_SUFFIX(SAMPLE_RATE), inputFile);

    FILE *grepCmd = popen(command, "r");

    if (grepCmd == nullptr) {
        perror("popen");
        return -1;
    }

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), grepCmd) != nullptr)
        F.sampleRate = atoi(buffer);
    else {
        perror("Error Reading File Sample Rate");
        pclose(grepCmd);
        return -1;
    }
    pclose(grepCmd);

    sprintf(command, "" FFMPEG_COMMAND_PREFIX "%s" FFMPEG_INFO_COMMAND_SUFFIX(CHANNELS), inputFile);
    grepCmd = popen(command, "r");

    if (grepCmd == nullptr) {
        perror("popen");
        return -1;
    }
    if (fgets(buffer, sizeof(buffer), grepCmd) != nullptr) {
        if (strstr(buffer, "stereo") != nullptr) {
            F.channels = 2;
        } else if (strstr(buffer, "mono") != nullptr) {
            F.channels = 1;
        } else {
            perror("Error Reading File Channels");
            pclose(grepCmd);
            return -1;
        }
    }

    return 0;
}


void audioProcessor::_cleanUp()
{
    if (m_activePipes.decodePipe) pclose(m_activePipes.decodePipe);
    if (m_activePipes.lpfPipe) pclose(m_activePipes.lpfPipe);
    if (m_activePipes.hpfPipe) pclose(m_activePipes.hpfPipe);
    if (m_activePipes.playPipe) pclose(m_activePipes.playPipe);

    m_activePipes.m_flags = 0;
    m_activeFile.sampleRate = 0;
    m_activeFile.channels = 0;
    m_fileLoaded = false;
}

#ifdef INCLUDE_FFMPEG_LIBS
int audioProcessor::streamAudioViaLibav(char *inputFile)
{
    return 0;
}

int audioProcessor::getFileInfo(char *inputFile, audioFileSettings &F)
{

    AVFormatContext *formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, inputFile, nullptr, nullptr) != 0) {
        perror("Error Opening File\n");
        avformat_close_input(&formatContext);
        return -1;
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        perror("Error Finding Stream Info\n");
        avformat_close_input(&formatContext);
        return -1;
    }

    for (int streamNdx = 0; streamNdx < formatContext->nb_streams; streamNdx++) {
        //todo: improve stream selection logic
        if (formatContext->streams[streamNdx]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
            continue;
        F.sampleRate = formatContext->streams[streamNdx]->codecpar->sample_rate;
        F.channels = formatContext->streams[streamNdx]->codecpar->ch_layout.nb_channels;
        break;
    }
    avformat_close_input(&formatContext);

    return F.streamNo;
}

#endif // INCLUDE_FFMPEG_LIBS


