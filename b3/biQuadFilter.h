#pragma once

#include <cstdint>
#include <deque>
#include <cstring>

#include "logger.h"

namespace b3 {

#define Q 0.707
#define GAIN 1


    class biQuadFilter {
    public:
        enum filterType {
            LPF,
            HPF,

            _filterTypeCount
        }; // enum filterType


        biQuadFilter(float sampleRate, float cutoff, float q, float gain, filterType type) :
            m_x(biQuadFilter::FF),
            m_y(biQuadFilter::FB),
            m_sampleRate(sampleRate),
            m_cutoff(cutoff),
            m_q(q),
            m_gain(gain),
            m_filterType(type)
        {
            updateCoeffs();
        }

        // setters
#define __setter(method,input,variable)         \
            inline void method(float input)     \
            {                                   \
                if (variable != input){         \
                    variable = input;           \
                    updateCoeffs();             \
                    DEBUG("Updated %d filter parameter: %f", m_filterType, input);  \
                }                                       \
            }                           

        __setter(setSampleRate, sampleRate, m_sampleRate)
        __setter(setQ, q, m_q)
        __setter(setGain, gain, m_gain)
        __setter(setCutoff, cutoff, m_cutoff)


        // updates buffers with new sample. Returns filtered sample
        float update(float sample);



    private:
        // updates coefficients based on filter type
        void updateCoeffs();

        static constexpr uint8_t FF = 3;
        static constexpr uint8_t FB = 2;


        std::deque<float> m_x;   // input buffer
        std::deque<float> m_y;   // output buffer


        float m_sampleRate; // sample rate in Hz
        float m_cutoff;     // cutoff frequency in Hz
        float m_q;          // q factor
        float m_gain;       // gain


        float a[FB];     // feedback coefficients
        float b[FF];     // feedforward coefficients

        filterType m_filterType;
    };  // class biQuadFilter

}; // namespace b3