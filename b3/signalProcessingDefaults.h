#pragma once
#include <stdint.h>

constexpr float CHUNK_SIZE_MS = 100.; //  ms chunks
constexpr float BUFFER_LENGTH_S = CHUNK_SIZE_MS * 3;

constexpr uint8_t CHUNK_COUNT = BUFFER_LENGTH_S / CHUNK_SIZE_MS;
constexpr uint8_t FILE_NAME_BUFFER_SIZE = 255;

constexpr float LPF_CUTOFF_DEFAULT = 500; // most music won't have noise above 20kHz if well mastered
constexpr float HPF_CUTOFF_DEFAULT = 5000;     // most music won't have noise below 20Hz if well mastered

struct filterSettings {

    filterSettings() :
        lpfCutoff(LPF_CUTOFF_DEFAULT),
        hpfCutoff(HPF_CUTOFF_DEFAULT)
    {}

    float lpfCutoff;        //low pass filter cutoff (Hz)
    float hpfCutoff;        //high pass filter cutoff (Hz)
};