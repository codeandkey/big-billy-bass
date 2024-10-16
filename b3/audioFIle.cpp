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

    m_formatContext = avformat_alloc_context();
    if (!m_formatContext) {
        ERROR("Failed to allocate format context");
        goto openFileErrorCleanup;
    }

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
    m_streamIndx = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &m_decoder, 0);
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

    if (avcodec_open2(m_decoderContext, m_decoder, nullptr) < 0) {
        WARNING("Failed to open codec");
        //de-allocated decoder context
        goto openFileErrorCleanup;
    }
    DEBUG("Setting up resampler");
    DEBUG("...channel layout: %lu", m_decoderContext->channel_layout);
    DEBUG("...sample rate: %d", m_decoderContext->sample_rate);
    DEBUG("...input sample format: %d", m_decoderContext->sample_fmt);

    // setup resampler to go from PCM floating point to PCM16
    m_swResampler = swr_alloc();
    av_opt_set_int(m_swResampler, "in_channel_layout", m_decoderContext->channel_layout, 0);
    av_opt_set_int(m_swResampler, "in_sample_rate", m_decoderContext->sample_rate, 0);
    av_opt_set_sample_fmt(m_swResampler, "in_sample_fmt", m_decoderContext->sample_fmt, 0);

    av_opt_set_int(m_swResampler, "out_channel_layout", m_decoderContext->channel_layout, 0);
    av_opt_set_int(m_swResampler, "out_sample_rate", m_decoderContext->sample_rate, 0);
    av_opt_set_sample_fmt(m_swResampler, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    swr_init(m_swResampler);


    strncpy(m_audioFileName, fileName, FILE_NAME_BUFFER_SIZE);
    m_frame = av_frame_alloc();
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
        if (m_swResampler) {
            swr_free(&m_swResampler);
            m_swResampler = nullptr;
        }
        m_audioFileName[0] = '\0';
        m_streamIndx = -1;
        m_fileOpen = false;
    }

    // debug checks
    assert(m_swResampler == nullptr);
    assert(m_decoderContext == nullptr);
    assert(m_formatContext == nullptr);
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
        return -1;
    }

    assert(m_frame != nullptr);

    int totalRead = 0;
    int frameSize;

    while (totalRead < readSize) {
        if (m_frameHead == 0 && (frameSize = _readFrame(m_frame) < 0))
            break;  // no more frames to read or bad frame

        int copySize = MIN(readSize - totalRead, frameSize - m_frameHead);

        // copy the frame data to the buffer
        swr_convert(m_swResampler, &buffer, m_frame->nb_samples, (const uint8_t **)m_frame->data, m_frame->nb_samples);
        // update the read count
        totalRead += copySize;
        m_frameHead = (m_frameHead + copySize) % frameSize;
    }

    assert(totalRead <= readSize);

    pthread_mutex_unlock(&m_fileMutex);

    return totalRead;
}

int b3::audioFile::_readFrame(AVFrame *frame)
{
    if (!m_fileOpen) {
        ERROR("audioFile::readFrame() - File not open");
        return -1;
    }

    AVPacket packet;
    av_new_packet(&packet, 0);
    packet.data = nullptr;
    packet.size = 0;
    int ret;

    if (av_read_frame(m_formatContext, &packet) < 0) {
        WARNING("Failed to read frame");
        goto errorCleanup;
    }

    if (packet.stream_index != m_streamIndx) {
        WARNING("Packet stream index does not match audio stream index");
        goto errorCleanup;
    }

    if (avcodec_send_packet(m_decoderContext, &packet) < 0) {
        WARNING("Failed to send packet to decoder");
        goto errorCleanup;
    }

    ret = avcodec_receive_frame(m_decoderContext, frame);

    if (ret == AVERROR(EAGAIN)) {
        WARNING("Decoder needs more packets to produce a frame");
        return 0;
    } else if (ret == AVERROR_EOF) {
        DEBUG("Decoder reached end of file");
        return 0;
    } else if (ret < 0) {
        WARNING("Failed to receive frame from decoder");
        goto errorCleanup;
    }

    av_packet_unref(&packet);
    // return the buffer size in the frame
    return av_samples_get_buffer_size(NULL, m_decoderContext->ch_layout.nb_channels, frame->nb_samples, m_decoderContext->sample_fmt, 1);

errorCleanup:
    av_packet_unref(&packet);
    return -1;
}


int b3::audioFile::chunkSizeBytes() const
{
    // calculates chunks size based on constants defined in signalProcessingDefaults.h and the file sample rate
    if (!m_fileOpen) {
        WARNING("File not open");
        return 0;
    }
    assert(m_decoderContext != nullptr);
    /*     [frames/second] * [samples / frame] * [chunk size in s] * [bytes/ pcm16 frames] */
    return getSampleRate() * getChannels() * CHUNK_SIZE_MS / 1000 * 2;
}
