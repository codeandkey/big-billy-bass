#pragma once

extern "C" {
#include <stdint.h>
}

namespace signalProcessingDefaults {


    constexpr float CHUNK_SIZE_MS = 50.; //  ms chunks
    
    constexpr uint8_t CHUNK_COUNT = 2;
    constexpr float BUFFER_LENGTH_MS = CHUNK_SIZE_MS * CHUNK_COUNT;

    
    constexpr uint8_t FILE_NAME_BUFFER_SIZE = 255;

    constexpr float LPF_CUTOFF_DEFAULT = 5000; // most music won't have noise above 20kHz if well mastered
    constexpr float HPF_CUTOFF_DEFAULT = 5000;     // most music won't have noise below 20Hz if well mastered

    // nice defaults for audio processing
    enum audioFormat {
        PCM_16 = 2,     // 16 bit PCM, 2 bytes per sample
        PCM_24 = 3,     // 24 bit PCM, 3 bytes per sample
        PCM_32 = 4,     // 32 bit PCM, 4 bytes per sample
    };

    // this is the magic constant that changes the audio format type for all the various drivers
    constexpr enum audioFormat DEFAULT_AUDIO_FORMAT = PCM_16;

    constexpr uint8_t __get_bytes_per_frame_per_channel()
    {
        switch (DEFAULT_AUDIO_FORMAT) {
        case PCM_16:
            return PCM_16;
        case PCM_24:
            return PCM_24;
        case PCM_32:
            return PCM_32;
        default:
            return 0;
        }
    }

    constexpr uint8_t BYTES_PER_SAMPLE = __get_bytes_per_frame_per_channel();
    constexpr int DEFAULT_SAMPLE_RATE = 44100; // enforce sample rate

}; // namespace signalProcessingDefaults
