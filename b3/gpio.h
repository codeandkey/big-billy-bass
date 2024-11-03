#pragma once

#include "signalProcessingDefaults.h"
#include <cstdint>

namespace b3 {
namespace gpio {

constexpr uint8_t BODY_DUTY = 255 * 90 / 100; // 95%
constexpr uint8_t MOUTH_DUTY = 0;

struct GpioConfig {
    int m_sampleRate = signalProcessingDefaults::DEFAULT_SAMPLE_RATE; // audio sample rate in hz
    int m_windowSize = 250;   // length of processing window in ms

    int m_pinBodyDirectionA = 17;
    int m_pinBodyDirectionB = 27;
    int m_pinMouthDirectionA = 24;
    int m_pinMouthDirectionB = 25;
    int m_pinBodySpeed = 12;
    int m_pinMouthSpeed = 13;

    int m_bodyMinRMS = 10000;
    int m_mouthMinRMS = 10000;

    void getPins(int dst[6]) {
        dst[0] = m_pinBodyDirectionA;
        dst[1] = m_pinBodyDirectionB;
        dst[2] = m_pinMouthDirectionA;
        dst[3] = m_pinMouthDirectionB;
        dst[4] = m_pinBodySpeed;
        dst[5] = m_pinMouthSpeed;
    }
};

int gpio_spawn(GpioConfig config=GpioConfig());
int gpio_exit();
int gpio_submitChunk(int16_t* lpf, int16_t* hpf, int n_samples);

} // namespace gpio
} // namespace b3
