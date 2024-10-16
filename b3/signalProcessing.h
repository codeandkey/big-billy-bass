#pragma once


#include <cassert>
#include <cstring>
#include <cstdio>

#include "signalProcessingDefaults.h"
#include "audioFile.h"
#include "task.h"
#include "state.h"
#include "timeManager.h" 
#include "logger.h"
#include "biQuadFilter.h"
#include "audioDriver.h"

namespace b3 {
    class signalProcessor : public Task {
    public:
        signalProcessor() :
            m_fileLoaded(0),
            m_activeState(State::STOPPED),
            m_audioFile(nullptr),
            m_lpf(nullptr),
            m_hpf(nullptr),
            m_alsaDriver(nullptr),
            m_chunkTimestamp(timeManager::getUsSinceEpoch())
        {}
        
        ~signalProcessor();

        /**
         * @brief Processes a frame based on the current state.
         *
         * This function processes a frame of data if the current state matches the active state.
         * It first checks if there is time remaining until the next chunk should be processed.
         * If there is time remaining, the function returns early.
         *
         * When the active state is PLAYING, it processes chunks of data. If the buffer needs to be filled,
         * it processes all but the last chunk and then processes the final chunk.
         *
         * @param state The current state to be checked against the active state.
         */
        virtual void frame(State state) override;


        /**
         * @brief Handles state transitions for the signal processor.
         *
         * This function processes transitions between different states of the signal processor.
         * It ensures that the necessary conditions are met before transitioning to a new state.
         *
         * @param from The current state of the signal processor.
         * @param to The state to transition to.
         *
         * @note Logs the state transition with the previous and new state values.
         * @note The function will return immediately if the signal processor is already in the target state.
         * @note The function will assert if the current state does not match the 'from' state.
         *
         * @note State Transition Logic:
         *
         * PLAYING:
         *   - Requires an audio driver to be loaded.
         *
         *   - Requires an audio file to be loaded.
         *
         *   - Sets the buffer fill flag to true.
         *
         * PAUSED:
         *
         *   - If transitioning from STOPPED, requires an audio driver and file to be loaded.
         *
         * STOPPED:
         *
         *   - Unloads the audio file.
         *
         *   - Closes the ALSA device.
         *
         * @warning If the required conditions for a state transition are not met, an error message will be logged and the transition will not occur.
         *
         * 
         */
        virtual void onTransition(State from, State to) override;


        // getters / setters
        
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
            delete m_lpf;
            delete m_hpf;
            m_lpf = nullptr;
            m_hpf = nullptr;
        }

        /**
         * @brief sets the low pass filter cutoff frequency
         * @param cutoff  cutoff frequency in Hz
         */
        inline void setLPF(float cutoff)
        {
            m_filterSettings.lpfCutoff = cutoff;
            if (m_fileLoaded) {
                assert(m_lpf);
                m_lpf->setCutoff(cutoff);
            }
        }

     /**
      * @brief sets the high pass filter cutoff frequency
      * @param cutoff cutoff frequency in Hz
      */
        inline void setHPF(float cutoff)
        {
            m_filterSettings.hpfCutoff = cutoff;
            if (m_fileLoaded) {
                assert(m_hpf);
                m_hpf->setCutoff(cutoff);
            }
        }

        const inline filterSettings getFilterSettings() const { return m_filterSettings; }

        uint64_t usToNextChunk();


    private:

        /**
         * @brief
         * processes the next chunk of the loaded audio file.
         * @return  0 on success, -1 on failure.
         */
        int _processChunk();

        // status fields
        union {
            struct {
                uint8_t m_fileLoaded : 1;
                uint8_t m_driverLoaded : 1;
                uint8_t __state_padding : 6;
            };
            uint8_t m_flags;
        };

        union {
            struct {
                uint8_t m_fillBuffer : 1;
                uint8_t m_stopCommand :1;
                uint8_t __signal_padding : 6;
            };
            uint8_t m_signalFlags;
        };

        audioFile *m_audioFile;
        filterSettings m_filterSettings;
        State m_activeState;

        biQuadFilter *m_lpf;
        biQuadFilter *m_hpf;
        
        audioDriver *m_alsaDriver;

        uint64_t m_chunkTimestamp;
        uint16_t m_chunkSize;
        uint64_t m_chunkSizeMS;
    };
};
