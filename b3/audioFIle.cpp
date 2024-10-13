#include "audioFile.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

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
    
    m_formateContext = avformat_alloc_context();
    if (!m_formateContext) {
        ERROR("Failed to allocate format context");
        goto openFileErrorCleanup;
    }

    // open the file
    if (avformat_open_input(&m_formateContext, m_audioFileName, nullptr, nullptr) < 0) {
        WARNING("Failed to open file: %s", m_audioFileName);
        goto openFileErrorCleanup;
    }

    // get the stream info
    if (avformat_find_stream_info(m_formateContext, nullptr) < 0) {
        WARNING("Failed to find stream info");
        goto openFileErrorCleanup;
    }
    // find the audio stream
    m_streamIndx = av_find_best_stream(m_formateContext, AVMEDIA_TYPE_AUDIO, -1, -1, &m_decoder, 0);
    if (m_streamIndx < 0) {
        WARNING("Failed to find audio stream");
        goto openFileErrorCleanup;
    }
    // get the codec context
    m_decoderContext = avcodec_alloc_context3(m_decoder);

    // get the codec parameters
    avcodec_parameters_to_context(m_decoderContext, m_formateContext->streams[m_streamIndx]->codecpar);

    if (avcodec_open2(m_decoderContext, m_decoder, nullptr) < 0) {
        WARNING("Failed to open codec");
        //de-allocated decoder context
        goto openFileErrorCleanup;
    }

    strncpy(m_audioFileName, fileName, FILE_NAME_BUFFER_SIZE);
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
        if (m_formateContext) {
            avformat_close_input(&m_formateContext);
            m_formateContext = nullptr;
        }
        if (m_frame) {
            av_frame_free(&m_frame);
            m_frame = nullptr;
        }
        m_audioFileName[0] = '\0';
        m_streamIndx = -1;
        m_fileOpen = false;
    }

    // debug checks
    assert(m_decoderContext == nullptr);
    assert(m_formateContext == nullptr);
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
        if (m_frameHead == 0 && _readFrame(m_frame) <= 0)
            break;  // no more frames to read or bad frame

        frameSize = _getFrameSize();

        int copySize = MIN(readSize - totalRead, frameSize - m_frameHead);

        // copy the frame data to the buffer
        memcpy(buffer + totalRead, m_frame->data[0] + m_frameHead, copySize);

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
    av_init_packet(&packet);
    packet.data = nullptr;
    packet.size = 0;
    int ret;

    if (av_read_frame(m_formateContext, &packet) < 0) {
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
    } else if (ret == AVERROR_EOF) {
        DEBUG("Decoder reached end of file");
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


int b3::audioFile::calculateChunkSize() const
{
    // calculates chunks size based on constants defined in signalProcessingDefaults.h and the file sample rate
    if (!m_fileOpen) {
        WARNING("File not open");
        return 0;
    }
    assert(m_decoderContext != nullptr);

    return (m_decoderContext->sample_rate * m_decoderContext->ch_layout.nb_channels * av_get_bytes_per_sample(m_decoderContext->sample_fmt) * CHUNK_SIZE_MS) / 1000;
}
