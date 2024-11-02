#pragma once

#include <cstdint>

namespace b3 {
namespace gpio {

struct GpioConfig {
    int m_sampleRate = 44100; // audio sample rate in hz
    int m_windowSize = 250;   // length of processing window in ms
};

int gpio_spawn(GpioConfig config=GpioConfig());
int gpio_exit();
int gpio_submitChunk(int16_t* lpf, int16_t* hpf, int n_samples);

} // namespace gpio
} // namespace b3
