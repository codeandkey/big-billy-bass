#pragma once

#include <pthread.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <cassert>

#include "signalProcessingDefaults.h"

namespace b3 {
    class audioFile {
    public:
        audioFile() :
            m_formatContext(nullptr),
            m_decoderContext(nullptr),
            m_decoder(nullptr),
            m_streamIndx(-1),
            m_frame(nullptr),
            m_frameHead(0),
            m_fileOpen(false)
        {
            m_audioFileName[0] = '\0';
            pthread_mutex_init(&m_fileMutex, nullptr);
        }

        audioFile(char *fileName) :
            m_formatContext(nullptr),
            m_decoderContext(nullptr),
            m_decoder(nullptr),
            m_streamIndx(-1),
            m_frameHead(0),
            m_fileOpen(false)
        {
            m_audioFileName[0] = '\0';
            pthread_mutex_init(&m_fileMutex, nullptr);
            openFile(fileName);
        }
        ~audioFile();

        /**
         * @brief Opens the audio file. Function is thread safe.
         *
         * @param fileName The name of the file to open
         * @return int 0 on success, -1 on failure
         */
        int openFile(const char *fileName);

        /**
         * @brief Closes the active audio file (if open). Function is thread safe.
         */
        void closeFile();


        /**
         * @brief reads the next chunk of audio data from the file, blocking call until data is read or there is no more data. Function is thread safe.
         *
         * @param buffer The buffer to read into
         * @param readSize The number of bytes to read
         * @return The number of bytes read, or -1 on failure
         */
        int readChunk(uint8_t *buffer, int readSize);

        int calculateChunkSize() const;

        inline int getChannels() const
        {
            if (!m_fileOpen)
                return 0;
            assert(m_decoderContext != nullptr);
            return m_decoderContext->ch_layout.nb_channels;
        }

        inline int getBitRate() const
        {
            if (!m_fileOpen)
                return 0;
            assert(m_decoderContext != nullptr);
            return m_decoderContext->bit_rate;
        }

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
            return av_samples_get_buffer_size(NULL, m_decoderContext->ch_layout.nb_channels, m_frame->nb_samples, m_decoderContext->sample_fmt, 1);
        }

        /**
         * @brief reads the next frame from the audio file. Function is NOT thread safe.
         *
         * @param frame The frame to read into
         * @return int 0 on success, -1 on failure
         * @note
         * This function does not allocate memory for the frame, it is expected to be pre-allocated
         */
        int _readFrame(AVFrame *frame);



        AVFormatContext *m_formatContext;
        AVCodecContext *m_decoderContext;
        const AVCodec *m_decoder;
        AVFrame *m_frame;   // used for reading frames, most recent frame read is stored here
        int8_t m_streamIndx;

        char m_audioFileName[FILE_NAME_BUFFER_SIZE];

        int m_frameHead;
        bool m_fileOpen;


        pthread_mutex_t m_fileMutex;
    }; // class audioFile
}; // namespace b3