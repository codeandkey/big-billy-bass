#pragma once

#include "audioSource.h"

#include <pthread.h>
#include <vector>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#include <cassert>

#include "signalProcessingDefaults.h"

namespace b3 {
    namespace audioFileDefaults {
        namespace SPD = signalProcessingDefaults;
        constexpr const char *FILE_NAME = "test.mp3";
        constexpr AVSampleFormat DECODER_FORMAT = SPD::__default_fmt_selector(
            AV_SAMPLE_FMT_S16,
            AV_SAMPLE_FMT_S32,
            AV_SAMPLE_FMT_FLT,
            AV_SAMPLE_FMT_DBLP
        );
    };


    class audioFile : public audioSource {
    public:
        audioFile() :
            m_format_ctx(nullptr),
            m_decoder_ctx(nullptr),
            m_swr_ctx(nullptr),
            m_decoder(nullptr),
            m_frame(nullptr),
            m_stream_ndx(-1),
            m_frame_smpl_stored(0),
            m_fileOpen(false),
            m_current_timetag_uS(0)
        {}

        ~audioFile();

        /**
         * @brief
         * Opens audio file via the ffmpeg libraries. This initializes ffmpeg context.
         * @param file_name full file path
         * @param seek_time time stamp to seek to to start playback. Set to zero to start from the beginning
         * @return 0 on success, -1 on error
         */
        int open_file(const char *fileName, uint64_t timetag);

        /**
         * @brief 
         * Tears down active ffmpeg context components, frees all pointers
         * 
         */
        void close_file();

        /**
         * @brief Reads frm_cnt # of frames into buffer. 
         * 
         * @param buffer Buffer to place frames into. 
         * @param frm_cnt Number of frames to place into buffer. Note that the frame count is independent of channel count, so reading 15 frames of 2 channels of data will attempt to place 30 pcm_t samples into buffer.
         * @return number of frames read on success, -1 on failure 
         */
        int read_chunk(pcm_t *buffer, size_t frm_cnt) override;

        /**
         * @brief 
         * The sample rate of the ffmpeg stream.
         * @return sample rate, in Hz
         */
        int sample_rate() override;

        /**
         * @brief  
         * The current timestamp of the current stream
         * @return 
         * TimeStamp in microSeconds
         */
        inline uint64_t stream_timestamp_uS() override { return m_current_timetag_uS; }


        /**
         * @brief 
         * Number of channels in the current audio stream
         * @return
         * channel count. 
         */
        inline int ch_count() override
        {
            if (!m_fileOpen)
                return 0;
            assert(m_decoder_ctx != nullptr);
            return m_decoder_ctx->ch_layout.nb_channels;
        }

    private:

        /**
         * @brief Reads the next AVFrame into frame.
         * @param frame the next AVFrame will be stored here. Sample rate is automatically convered to `signalprocessingDefaults::DEFAULT_SAMPLE_RATE`
         * @return number of samples in the AVFrame        
         */
        int _readFrame(AVFrame *frame);

        AVFormatContext *m_format_ctx;
        AVCodecContext *m_decoder_ctx;
        SwrContext *m_swr_ctx;
        AVCodec *m_decoder;
        AVFrame *m_frame;   // used for reading frames, most recent frame read is stored here
        int8_t m_stream_ndx;

        int m_frame_smpl_stored;
        bool m_fileOpen;
        bool m_packetSent;
        uint64_t m_current_timetag_uS;
    }; // class audioFile
}; // namespace b3