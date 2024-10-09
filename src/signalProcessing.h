

#ifndef __signalProcessing_h
#define __signalProcessing_h


#include "task.h"
#include "state.h"
#include "signalProcessingDefaults.h"
#include "timeManager.h" 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define INCLUDE_FFMPEG_LIBS

#ifndef INCLUDE_FFMPEG_LIBS
# define FORCE_USE_PIPES 1
#else
#include <libavformat/avformat.h>
# define FORCE_USE_PIPES 0
#endif

namespace b3 {

    class audioProcessor : public Task {
    public:
        audioProcessor() :
            m_fileLoaded(0),
            m_usePipes(FORCE_USE_PIPES),
            m_chunkTimer(timeManager::getUsSinceEpoch())
        {
            m_inputFileName[0] = '\0';
        }
#ifdef INCLUDE_FFMPEG_LIBS
        audioProcessor(bool usePipes) :
            m_fileLoaded(0),
            m_usePipes(usePipes),
            m_chunkTimer(timeManager::getUsSinceEpoch()),
            m_inputFileName(nullptr)
        {}
#endif
        ~audioProcessor();

        static int pipeFileInfo(char *inputFile, audioFileSettings &F);
#ifdef INCLUDE_FFMPEG_LIBS
        static int getFileInfo(char *inputFile, audioFileSettings &F);
#endif
        // virtual functions
        virtual void frame(State state) override;

        virtual void onTransition(State from, State to) override;


        int update();

        const inline filterSettings getFilterSettings() const { return m_filterSettings; }
        void requestState(State command);
        inline void setLPF(float cutoff) { m_filterSettings.lpfCutoff = cutoff; }
        inline void setHPF(float cutoff) { m_filterSettings.hpfCutoff = cutoff; }

        uint64_t usToNextChunk();

    private:
        void _processChunk();

        int _openFile(char *inputFile);
        int _readChunk(uint8_t *buffer, FILE *fp, int readSize);

        int initAudioStreams(char *inputFile);
#ifdef INCLUDE_FFMPEG_LIBS
        int streamAudioViaLibav(char *inputFile);
#endif

        void _cleanUp();

        typedef struct activePipes {
            activePipes() :
                lpfPipe(nullptr),
                hpfPipe(nullptr),
                playPipe(nullptr),
                decodePipe(nullptr),
                m_flags(0)
            {}
            FILE *lpfPipe;
            FILE *hpfPipe;
            FILE *playPipe;
            FILE *decodePipe;
            union {
                struct {
                    uint8_t lpfPipeActive : 1;
                    uint8_t hpfPipeActive : 1;
                    uint8_t playPipeActive : 1;
                    uint8_t decodePipeActive : 1;
                    uint8_t __padding : 4;
                };
                uint8_t m_flags;
            };
        } activePipes;
        activePipes m_activePipes;

        union {
            struct {
                uint8_t m_fileLoaded : 1;
                uint8_t m_usePipes : 1;
                uint8_t __state_padding : 6;
            };
            uint8_t m_flags;
        };

        union {
            struct {
                uint8_t m_fillBuffer : 1;
                uint8_t m_loadFile : 1;
                uint8_t __signal_padding : 7;
            };
            uint8_t m_signalFlags;
        };

        audioFileSettings m_activeFile;
        filterSettings m_filterSettings;
        State m_activeState;

        uint64_t m_chunkTimer;
        uint8_t m_chunkSize;

        char m_inputFileName[FILE_NAME_BUFFER_SIZE];

    };


};
#endif // __signalProcessing_h