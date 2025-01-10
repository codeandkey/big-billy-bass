#include "audioFile.h"

extern "C" {
#include <libavutil/opt.h>
}
#include <cassert>

#include "logger.h"
#include "sighandler.h"
#include "timeManager.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

b3::audioFile::~audioFile()
{
    close_file();
}

int b3::audioFile::open_file(const char *file_name, uint64_t seek_time)
{
    // open the file
    if (avformat_open_input(&m_format_ctx, file_name, nullptr, nullptr) < 0) {
        ERROR("Failed to open file: %s", file_name);
        goto openFileErrorCleanup;
    }
    DEBUG("Opened %s", file_name);

    // get the stream info
    if (avformat_find_stream_info(m_format_ctx, nullptr) < 0) {
        WARNING("Failed to find stream info");
        goto openFileErrorCleanup;
    }
    // find the audio stream
    m_stream_ndx = av_find_best_stream(m_format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, (const AVCodec **)&m_decoder, 0);
    if (m_stream_ndx < 0) {
        WARNING("Failed to find audio stream");
        goto openFileErrorCleanup;
    }
    DEBUG("...Found audio in stream %d", m_stream_ndx);
    // get the codec context
    m_decoder_ctx = avcodec_alloc_context3(m_decoder);

    // set packet time base?
    m_decoder_ctx->pkt_timebase = m_format_ctx->streams[m_stream_ndx]->time_base;

    // get the codec parameters
    avcodec_parameters_to_context(m_decoder_ctx, m_format_ctx->streams[m_stream_ndx]->codecpar);

    // set sample format
    m_decoder_ctx->sample_fmt = audioFileDefaults::DECODER_FORMAT;

    if (avcodec_open2(m_decoder_ctx, m_decoder, nullptr) < 0) {
        WARNING("Failed to open codec");
        goto openFileErrorCleanup;
    }
    DEBUG("Audio decoder Settings:");
    DEBUG("--channel layout: %lu", m_decoder_ctx->ch_layout.nb_channels);
    DEBUG("--sample rate: %d", m_decoder_ctx->sample_rate);
    DEBUG("--input sample format: %d", m_decoder_ctx->sample_fmt);

    // allocate the converter
    swr_alloc_set_opts2(
        &m_swr_ctx,
        &m_decoder_ctx->ch_layout,
        audioFileDefaults::DECODER_FORMAT,
        signalProcessingDefaults::SAMPLE_RATE,
        &m_decoder_ctx->ch_layout,
        m_decoder_ctx->sample_fmt,
        m_decoder_ctx->sample_rate,
        0, nullptr
    );

    m_fileOpen = true;
    DEBUG("Resampler Settings:");
    DEBUG("--sample rate: %d", sample_rate());

    // seek to timetag
    if (av_seek_frame(m_format_ctx, m_stream_ndx, seek_time, AVSEEK_FLAG_BACKWARD) < 0)
        ERROR("Failed to seek to %llu", seek_time);

    m_frame = av_frame_alloc();

    return 0;

openFileErrorCleanup:
    close_file();
    return -1;
}

void b3::audioFile::close_file()
{
    timeManager tm;
    if (m_fileOpen) {
        if (m_decoder_ctx) {
            avcodec_free_context(&m_decoder_ctx);
            m_decoder_ctx = nullptr;
        }
        if (m_format_ctx) {
            avformat_close_input(&m_format_ctx);
            m_format_ctx = nullptr;
        }
        if (m_frame) {
            av_frame_free(&m_frame);
            m_frame = nullptr;
        }
        if (m_swr_ctx) {
            swr_free(&m_swr_ctx);
            m_swr_ctx = nullptr;
        }
        m_stream_ndx = -1;
        m_fileOpen = false;
    }

    // debug checks
    assert(m_decoder_ctx == nullptr);
    assert(m_format_ctx == nullptr);
    assert(m_swr_ctx == nullptr);
    assert(m_frame == nullptr);
    assert(m_stream_ndx == -1);
    assert(!m_fileOpen);
    DEBUG("closeFile(): %llu", tm.lap());
}


int b3::audioFile::read_chunk(pcm_t *buffer, size_t sample_count)
{
    if (!m_fileOpen) {
        WARNING("File not open");
        return -1;
    }

    // assert frame has been allocated & initialized
    assert(m_frame != nullptr);

    int samples_stored = 0;
    do {
        if (m_frame_smpl_stored == 0 && _readFrame(m_frame) <= 0)
            break;

        int samples_2_copy = MIN(m_frame->nb_samples - m_frame_smpl_stored, (int)sample_count - samples_stored);
        int buffer_offset = samples_stored * ch_count();
        int frame_offset = m_frame_smpl_stored * ch_count();
        int copy_size_bytes = samples_2_copy * ch_count() * av_get_bytes_per_sample(audioFileDefaults::DECODER_FORMAT);

        memcpy(&buffer[buffer_offset], &m_frame->data[0][frame_offset], copy_size_bytes);

        samples_stored += samples_2_copy;
        m_frame_smpl_stored = ((m_frame_smpl_stored + samples_2_copy) % (m_frame->nb_samples));
    } while (samples_stored < (int)sample_count && !signalHandler::g_shouldExit);


    return samples_stored;
}

int b3::audioFile::_readFrame(AVFrame *frame)
{
    if (!m_fileOpen) {
        ERROR("audioFile::readFrame() - File not open");
        return -1;
    }


    int ret;
    AVPacket *packet = av_packet_alloc();
    AVFrame *tempFrame = av_frame_alloc();

    av_new_packet(packet, 0);

    // get most current frame
    for (uint pckCnt = 0; pckCnt < m_format_ctx->nb_streams; pckCnt++) {
        if ((ret = av_read_frame(m_format_ctx, packet)) < 0) {
            if (ret == AVERROR_EOF)
                continue;
                
            WARNING("Failed to read frame");
            goto errorCleanup;
        }
        if (packet->stream_index == m_stream_ndx)
            break;
        else
            av_packet_unref(packet); // skip this packet if we aren't in the right stream...
    }
    if (packet->stream_index != m_stream_ndx) {
        ERROR("Could not find valid packet");
        ret = -1;
        goto errorCleanup;
    }

    if (avcodec_send_packet(m_decoder_ctx, packet) < 0) {
        WARNING("Failed to send packet to decoder");
        goto errorCleanup;
    }

    ret = avcodec_receive_frame(m_decoder_ctx, tempFrame);
    if (ret == AVERROR_EOF) {
        DEBUG("Decoder reached end of file");
        // reset timestamp
        m_current_timetag_uS = 0;
        goto errorCleanup;
    } else if (ret < 0) {
        WARNING("Failed to receive frame from decoder");
        goto errorCleanup;
    }

    av_frame_unref(frame);

    // convert frame to the data we like
    frame->sample_rate = signalProcessingDefaults::SAMPLE_RATE;
    frame->ch_layout = tempFrame->ch_layout;
    frame->format = audioFileDefaults::DECODER_FORMAT;
    m_current_timetag_uS = tempFrame->pts;

    ret = swr_convert_frame(m_swr_ctx, frame, tempFrame);
    if (ret < 0) {
        ERROR("Failed to convert frame");
        goto errorCleanup;
    }
    av_frame_free(&tempFrame);
    av_packet_free(&packet);

    // return the # of read samples in the frame
    return frame->nb_samples;
errorCleanup:
    // deallocate function only needed stuff
    av_frame_free(&tempFrame);
    av_packet_free(&packet);
    return ret;
}

int b3::audioFile::sample_rate()
{
    if (!m_fileOpen)
        return 0;
    assert(m_swr_ctx != nullptr);
    int64_t fsOut;
    av_opt_get_int(m_swr_ctx, "out_sample_rate", 0, &fsOut);
    return fsOut;
}
