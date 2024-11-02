#pragma once


#include <cassert>
#include <cstring>
#include <cstdio>

#include "audioFile.h"
#include "timeManager.h" 
#include "logger.h"
#include "state.h"
#include "biQuadFilter.h"
#include "audioDriver.h"

namespace b3 {
    namespace SPD = signalProcessingDefaults;

    class signalProcessor {
    public:
        signalProcessor() :
            m_driverLoaded(false),
            m_fillBuffer(false),
            m_stopCommand(false),
            m_fileLoaded(false),
            m_closeFile(false),
            m_audioFile(nullptr),
            m_underRunCounter(0),
            m_activeState(State::STOPPED),
            m_alsaDriver(nullptr),
            m_chunkTimestamp(timeManager::getUsSinceEpoch())
        {
            memset(m_filters, 0, sizeof(m_filters));
            m_filterSettings[biQuadFilter::HPF] = SPD::HPF_CUTOFF_DEFAULT;
            m_filterSettings[biQuadFilter::LPF] = SPD::LPF_CUTOFF_DEFAULT;
            m_closeFile = false;
        }

        ~signalProcessor();

        /**
         * To be called every time a new chunk is to be processed
         */
        void update(State state);




        // getters / setters

        inline State getState() const { return m_activeState;}

        /**
         * @brief
         * Sets the audio driver for the audio processor. The audio processor does not own the driver.
         *
         * @param driver
         */
        void setAudioDriver(audioDriver *driver);

        /**
         * @brief
         * Sets the audio file to be processed by the audio processor
         * @param F The audio file to process
         * @note
         * F is NOT owned by the audioProcessor and must be managed by the caller
         */
        void setFile(audioFile *F);


        /**
         * @brief
         * Unloads the current audio file
         * @note
         * This does not delete the audio file object, it only removes the reference
         */
        inline void unLoadFile()
        {
            m_fileLoaded = 0;
            m_audioFile = nullptr;
            for (int fltrNdx = 0; fltrNdx < biQuadFilter::_filterTypeCount; fltrNdx++)
                delete m_filters[fltrNdx];
        }


#define __setFilter(setter, accessor)                                   \
        inline void setter(float cutoff)                                \
        {                                                               \
            m_filterSettings[accessor] = cutoff;                        \
            if (m_filters[accessor]){                                    \
                m_filters[accessor]->setCutoff(cutoff);                  \
            }                                                           \
        }             

        __setFilter(setLPF, biQuadFilter::LPF)
        __setFilter(setHPF, biQuadFilter::HPF)

        uint64_t usToNextChunk();

    private:

        /**
         * @brief Sets processor state.
         * @param to State to set processor to.
         */
        void setState(State to);


        /**
         * @brief
         * processes the next chunk of the loaded audio file.
         * @return  0 on success, -1 on failure.
         */
        int _processChunk();

        void _negotiateChunkSize();

        // status fields
        bool m_fileLoaded;
        bool m_driverLoaded;

        // flags
        bool m_fillBuffer;
        bool m_stopCommand;
        bool m_closeFile;

        State m_activeState;


        audioFile *m_audioFile;
        audioDriver *m_alsaDriver;

        float m_filterSettings[biQuadFilter::_filterTypeCount];
        biQuadFilter *m_filters[biQuadFilter::_filterTypeCount];
        int m_underRunCounter;

        uint64_t m_chunkTimestamp;
        uint64_t m_chunkSizeUs;
        uint16_t m_chunkSize;

        FILE *m_signalDebugFile;
    };
};
