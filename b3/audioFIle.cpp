#include "audioFile.h"

extern "C" {
#include <libavutil/opt.h>
}
#include <cassert>

#include "logger.h"


#define MIN(a, b) ((a) < (b) ? (a) : (b))

b3::audioFile::~audioFile()
{
    closeFile();
}


int b3::audioFile::openFile(const char *fileName)
{
    pthread_mutex_lock(&m_fileMutex);
    
    // open the file
    if (avformat_open_input(&m_formatContext, fileName, nullptr, nullptr) < 0) {
        WARNING("Failed to open file: %s", fileName);
        goto openFileErrorCleanup;
    }
    DEBUG("Opened %s", fileName);

    // get the stream info
    if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
        WARNING("Failed to find stream info");
        goto openFileErrorCleanup;
    }
    // find the audio stream
    m_streamIndx = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, (const AVCodec **)&m_decoder, 0);
    if (m_streamIndx < 0) {
        WARNING("Failed to find audio stream");
        goto openFileErrorCleanup;
    }
    DEBUG("...Found audio in stream %d", m_streamIndx);
    // get the codec context
    m_decoderContext = avcodec_alloc_context3(m_decoder);

    // set packet time base?
    m_decoderContext->pkt_timebase = m_formatContext->streams[m_streamIndx]->time_base;

    // get the codec parameters
    avcodec_parameters_to_context(m_decoderContext, m_formatContext->streams[m_streamIndx]->codecpar);

    // set sample format
    m_decoderContext->sample_fmt = audioFileDefaults::DEFAULT_DECODER_FORMAT;

    if (avcodec_open2(m_decoderContext, m_decoder, nullptr) < 0) {
        WARNING("Failed to open codec");
        //de-allocated decoder context
        goto openFileErrorCleanup;
    }
    DEBUG("Audio decoder Settings:");
    DEBUG("--channel layout: %lu", m_decoderContext->ch_layout.nb_channels);
    DEBUG("--sample rate: %d", m_decoderContext->sample_rate);
    DEBUG("--input sample format: %d", m_decoderContext->sample_fmt);

    // allocate the converter
    swr_alloc_set_opts2(&m_swrContext,
        &m_decoderContext->ch_layout,
        audioFileDefaults::DEFAULT_DECODER_FORMAT,
        signalProcessingDefaults::DEFAULT_SAMPLE_RATE,
        &m_decoderContext->ch_layout,
        m_decoderContext->sample_fmt,
        m_decoderContext->sample_rate,
        0, nullptr);

    m_frame = av_frame_alloc();

    strncpy(m_audioFileName, fileName, audioFileDefaults::FILE_NAME_BUFFER_SIZE);
    m_fileOpen = true;

    pthread_mutex_unlock(&m_fileMutex);
    return 0;

openFileErrorCleanup:
    pthread_mutex_unlock(&m_fileMutex);
    closeFile();
    return -1;
}

void b3::audioFile::closeFile()
{
    pthread_mutex_lock(&m_fileMutex);
    if (m_fileOpen) {
        if (m_decoderContext) {
            avcodec_free_context(&m_decoderContext);
            m_decoderContext = nullptr;
        }
        if (m_formatContext) {
            avformat_close_input(&m_formatContext);
            m_formatContext = nullptr;
        }
        if (m_frame) {
            av_frame_free(&m_frame);
            m_frame = nullptr;
        }
        if (m_swrContext) {
            swr_free(&m_swrContext);
            m_swrContext = nullptr;
        }
        m_audioFileName[0] = '\0';
        m_streamIndx = -1;
        m_fileOpen = false;
    }

    // debug checks
    assert(m_decoderContext == nullptr);
    assert(m_formatContext == nullptr);
    assert(m_swrContext == nullptr);
    assert(m_decoder == nullptr);
    assert(m_frame == nullptr);
    assert(m_streamIndx == -1);
    assert(m_audioFileName[0] == '\0');
    assert(!m_fileOpen);
    pthread_mutex_unlock(&m_fileMutex);
}


int b3::audioFile::readChunk(uint8_t *buffer, int readSize)
{
    pthread_mutex_lock(&m_fileMutex);

    if (!m_fileOpen) {
        WARNING("File not open");
        pthread_mutex_unlock(&m_fileMutex);
        return -1;
    }

    // assert frame has been allocated & initialized
    assert(m_frame != nullptr);

    int samplesStored = 0;
    int frameSize;
    do {
        if (m_frameSampleNdx == 0 && _readFrame(m_frame) <= 0)
            break;
        frameSize = _getFrameSize(m_frame);
        int copysize = MIN(frameSize - m_frameSampleNdx, readSize - samplesStored);

        memcpy(buffer + samplesStored, m_frame->data[0] + m_frameSampleNdx, copysize);

        samplesStored += copysize;
        m_frameSampleNdx = ((m_frameSampleNdx + copysize) % (frameSize));
    } while (samplesStored < readSize);

    pthread_mutex_unlock(&m_fileMutex);

    return samplesStored;
}

int b3::audioFile::_readFrame(AVFrame *frame)
{
    if (!m_fileOpen) {
        ERROR("audioFile::readFrame() - File not open");
        return -1;
    }


    int ret;
    bool readAchieved = false;
    AVPacket *packet = av_packet_alloc();
    AVFrame *tempFrame = av_frame_alloc();

    av_new_packet(packet, 0);

    // get most current frame
    for (int pckCnt = 0; pckCnt < m_formatContext->nb_streams; pckCnt++) {
        if ((ret = av_read_frame(m_formatContext, packet)) < 0) {
            if (ret == AVERROR_EOF)
                INFO("Decoder detected EOF");
            else
                WARNING("Failed to read frame");
            av_packet_free(&packet);
            goto errorCleanup;
        }

        if (packet->stream_index == m_streamIndx)
            break;
        else
            av_packet_unref(packet); // skip this packet if we aren't in the right stream...
    }
    if (packet->stream_index != m_streamIndx) {
        ERROR("Could not find valid packet");
        ret = -1;
        goto errorCleanup;
    }


    if (avcodec_send_packet(m_decoderContext, packet) < 0) {
        WARNING("Failed to send packet to decoder");
        goto errorCleanup;
    }

    ret = avcodec_receive_frame(m_decoderContext, tempFrame);

    if (ret == AVERROR(EAGAIN)) {
        m_packetSent = false;
        // continue;
    } else if (ret == AVERROR_EOF) {
        DEBUG("Decoder reached end of file");
        goto errorCleanup;
    } else if (ret < 0) {
        WARNING("Failed to receive frame from decoder");
        goto errorCleanup;
    }

    av_frame_unref(frame);

    // convert frame to the data we like
    frame->sample_rate = tempFrame->sample_rate;
    frame->ch_layout = tempFrame->ch_layout;
    frame->format = audioFileDefaults::DEFAULT_DECODER_FORMAT;

    ret = swr_convert_frame(m_swrContext, frame, tempFrame);
    if (ret < 0) {
        ERROR("Failed to convert frame");
        goto errorCleanup;
    }
    av_frame_free(&tempFrame);
    av_packet_free(&packet);

    // return the buffer size in the frame
    return _getFrameSize(frame);

errorCleanup:
    // deallocate function only needed stuff
    av_frame_free(&tempFrame);
    av_packet_free(&packet);
    return ret;
}



int b3::audioFile::chunkSizeBytes(float chunkSizeMs) const
{
    // calculates chunks size based on constants defined in signalProcessingDefaults.h and the file sample rate
    if (!m_fileOpen) {
        WARNING("File not open");
        return 0;
    }
    assert(chunkSizeMs >= 0);
    assert(m_decoderContext != nullptr);

    return getSampleRate()                                                      // [frames / second]
        * getChannels()                                                         // [channels / frame]
        * chunkSizeMs / 1000                                                    // [frames / chunk]
        * av_get_bytes_per_sample(audioFileDefaults::DEFAULT_DECODER_FORMAT);   // [bytes / frame]
}
