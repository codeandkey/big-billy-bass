#pragma once

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
        constexpr uint8_t FILE_NAME_BUFFER_SIZE = signalProcessingDefaults::FILE_NAME_BUFFER_SIZE;
        constexpr const char *DEFAULT_FILE_NAME = "test.mp3";
        constexpr const char *AUDIO_FILES_PATH = "audio";
        constexpr const float DEFAULT_NORMALIZATION_LUFS = -5.;
        constexpr AVSampleFormat __get_default_codec()
        {
            switch (signalProcessingDefaults::DEFAULT_AUDIO_FORMAT) {
            case signalProcessingDefaults::PCM_16:
                return AV_SAMPLE_FMT_S16;
            case signalProcessingDefaults::PCM_24:
                return AV_SAMPLE_FMT_S32;
            case signalProcessingDefaults::PCM_32:
                return AV_SAMPLE_FMT_FLT;
            default:
                return AV_SAMPLE_FMT_NONE;
            }
        }
        constexpr AVSampleFormat DEFAULT_DECODER_FORMAT = __get_default_codec();
    };


    class audioFile {
    public:
        audioFile() :
            m_formatContext(nullptr),
            m_decoderContext(nullptr),
            m_swrContext(nullptr),
            m_decoder(nullptr),
            m_streamIndx(-1),
            m_frame(nullptr),
            m_frameSampleNdx(0),
            m_fileOpen(false),
            m_packetSent(false),
            m_seekTimeTag(0)
        {
            m_audioFileName[0] = '\0';
            pthread_mutex_init(&m_fileMutex, nullptr);
        }

        audioFile(char *fileName, uint64_t timetag) :
            m_formatContext(nullptr),
            m_decoderContext(nullptr),
            m_swrContext(nullptr),
            m_decoder(nullptr),
            m_streamIndx(-1),
            m_frame(nullptr),
            m_frameSampleNdx(0),
            m_fileOpen(false),
            m_packetSent(false),
            m_seekTimeTag(timetag)
        {
            m_audioFileName[0] = '\0';
            pthread_mutex_init(&m_fileMutex, nullptr);
            openFile(fileName);
        }
        ~audioFile();


        int openFile(const char *fileName);

        /**
         * @brief Closes the active audio file (if open). Function is thread safe.
         */
        void closeFile();

        /**
         * @brief Reads a chunk of audio data into the provided buffer.
         *
         * This function reads up to `readSize` bytes of audio data from the audio file
         * into the provided buffer. Function is thread safe.
         *
         * @param buffer Pointer to the buffer where the audio data will be stored.
         * @param readSize The maximum number of bytes to read into the buffer.
         * @return The number of bytes actually read and stored in the buffer, or -1 if the file is not open.
         *
         * @note The function assumes that the frame has been allocated and initialized.
         * @warning If the file is not open, the function will log a warning and return -1.
         */
        int readChunk(uint8_t *buffer, int readSize);

        /**
         * @brief Calculates the chunk size in bytes based on the given chunk size in milliseconds.
         *
         * This function computes the size of a chunk of audio data in bytes, based on:
         *
         * - The output sample rate from the file (sample rate which would be expected in a `readChunk()` call).
         *
         * - The number of channels in the file.
         *
         * - The given chunk size in milliseconds.
         *
         * - The default audio format defined in `signalProcessingDefaults::DEFAULT_DECODER_FORMAT`.
         *
         *
         * @param chunkSizeMs The size of the chunk in milliseconds.
         * @return The size of the chunk in bytes. Returns 0 if the file is not open.
         */
        int chunkSizeBytes(float chunkSizeMs) const;

        /**
         * @return number of audio channels in the loaded audio file. 0 if no file is loaded
         */
        inline int getChannels() const
        {
            if (!m_fileOpen)
                return 0;
            assert(m_decoderContext != nullptr);
            return m_decoderContext->ch_layout.nb_channels;
        }


        /**
         * @brief Returns the sample rate of the loaded audio file.
         * @return sample rate (hz), 0 if no file is loaded

         */
        int getSampleRate() const;

    private:
        /**
         * @brief gets the size of the current frame (frame must be init'd and loaded). Function is NOT thread safe.
         *
         * @return int The size of the frame in bytes
         */
        inline int _getFrameSize() const
        {
            assert(m_frame != nullptr);
            assert(m_decoderContext != nullptr);
            return _getFrameSize(m_frame);
        }

        inline int _getFrameSize(AVFrame *frame) const
        {
            if (frame == nullptr)
                return 0;
            assert(m_decoderContext != nullptr);
            return frame->ch_layout.nb_channels * frame->nb_samples * av_get_bytes_per_sample(audioFileDefaults::DEFAULT_DECODER_FORMAT);
        }

        inline int _normalizeAudio(const char *fileName);

        /**
         * @brief Reads a frame from the audio file.
         *
         * This function reads a frame from the audio file and decodes it. It handles
         * packet reading, decoding, and frame conversion. If the file is not open,
         * it returns an error. The frame is decoded into the format specified by `audioFileDefaults::DEFAULT_DECODER_FORMAT`.
         *
         * @param frame Pointer to an AVFrame structure where the decoded frame will be stored.
         * @return int Returns the size of the frame buffer on success, or a negative error code on failure.
         *
         * Error Codes:
         * - -1: File not open or could not find a valid packet.
         *
         * - AVERROR_EOF: End of file reached.
         *
         * - AVERROR(EAGAIN): Decoder needs more packets to produce a frame.
         *
         * Other negative values:
         *
         * - Errors during packet reading, sending, receiving, or frame conversion.
         */
        int _readFrame(AVFrame *frame);


        AVFormatContext *m_formatContext;
        AVCodecContext *m_decoderContext;
        SwrContext *m_swrContext;
        AVCodec *m_decoder;
        AVFrame *m_frame;   // used for reading frames, most recent frame read is stored here
        int8_t m_streamIndx;

        char m_audioFileName[audioFileDefaults::FILE_NAME_BUFFER_SIZE];

        int m_frameSampleNdx;
        bool m_fileOpen;
        bool m_packetSent;
        uint64_t m_seekTimeTag;

        pthread_mutex_t m_fileMutex;
    }; // class audioFile
}; // namespace b3