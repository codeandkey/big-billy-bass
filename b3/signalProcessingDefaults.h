


#ifndef __signalProcessingDefaults_h
#define __signalProcessingDefaults_h

#include <stdint.h>

constexpr float CHUNK_SIZE_MS = 64.0f; // 64 ms chunks
constexpr float BUFFER_LENGTH_S = 128.0f; // 128 ms buffer

constexpr uint8_t CHUNK_COUNT = BUFFER_LENGTH_S / CHUNK_SIZE_MS;
constexpr uint8_t FILE_NAME_BUFFER_SIZE = 255;

constexpr float LPF_CUTOFF_DEFAULT = 20000; // most music won't have noise above 20kHz if well mastered
constexpr float HPF_CUTOFF_DEFAULT = 0;     // most music won't have noise below 20Hz if well mastered

typedef struct filterSettings {

    filterSettings() :
        lpfCutoff(LPF_CUTOFF_DEFAULT),
        hpfCutoff(HPF_CUTOFF_DEFAULT)
    {}

    float lpfCutoff;        //low pass filter cutoff (Hz)
    float hpfCutoff;        //high pass filter cutoff (Hz)
};

#endif // __signalProcessingDefaults_h
