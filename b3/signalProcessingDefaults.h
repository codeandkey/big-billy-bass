#pragma once

extern "C" {
#include <stdint.h>
#include <stdlib.h>
}

#if defined(USE_PCM_I16)
typedef int16_t pcm_t;
#elif defined(USE_PCM_I32)
typedef int32_t pcm_t;
#elif defined(USE_PCM_F32)
typedef float pcm_t;
#elif defined(USE_PCM_F64)
typedef double pcm_t;
#else
typedef int16_t pcm_t;
#endif

namespace signalProcessingDefaults {


    constexpr float CHUNK_SIZE_MS = 50.; //  ms chunks
    constexpr size_t CHUNK_COUNT = 2;

    constexpr float BUFFER_LENGTH_MS = CHUNK_SIZE_MS * CHUNK_COUNT;

    constexpr float LPF_CUTOFF = 5000;      
    constexpr float HPF_CUTOFF = 5000;      

    // nice defaults for audio processing
    enum audioFormat {
        PCM_I16,     // 16 bit PCM, 2 bytes per sample
        PCM_I32,     // 32 bit PCM, 4 bytes per sample
        PCM_F32,     // 32 bit PCM, 4 bytes per sample
        PCM_F64,     // 32 bit PCM, 4 bytes per sample
    };

// this is the magic constant that changes the audio format type for all the various drivers
#if defined(USE_PCM_S16)
    constexpr enum audioFormat DEFAULT_AUDIO_FORMAT = PCM_I16;
#elif defined(USE_PCM_S32)
    constexpr enum audioFormat DEFAULT_AUDIO_FORMAT = PCM_I32;
#elif defined(USE_PCM_F32)
    constexpr enum audioFormat DEFAULT_AUDIO_FORMAT = PCM_F32;
#elif defined(USE_PCM_F64)
    constexpr enum audioFormat DEFAULT_AUDIO_FORMAT = PCM_F34;
#else
    constexpr enum audioFormat AUDIO_FORMAT = PCM_I16;
#endif

    constexpr int SAMPLE_RATE = 44100; // enforce sample rate

    template<typename fmt_value>
    constexpr fmt_value __default_fmt_selector(const fmt_value s16, const fmt_value s32, const fmt_value f32, const fmt_value f64)
    {
        return (AUDIO_FORMAT == audioFormat::PCM_I16) ? s16 :
            (AUDIO_FORMAT == audioFormat::PCM_I32) ? s32 :
            (AUDIO_FORMAT == audioFormat::PCM_F32) ? f32 :
            (AUDIO_FORMAT == audioFormat::PCM_F64) ? f64 :
            s16; // Default case
    }
    constexpr size_t BYTES_PER_SAMPLE = __default_fmt_selector(
        sizeof(uint16_t),
        sizeof(uint32_t),
        sizeof(float),
        sizeof(double)
    );

}; // namespace signalProcessingDefaults


