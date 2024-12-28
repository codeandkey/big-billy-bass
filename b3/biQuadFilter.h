#pragma once

#include <cstdint>
#include <deque>
#include <cstring>

#include "logger.h"
#include "signalProcessingDefaults.h"

namespace b3 {

#define Q 0.707
#define GAIN 1


    class biQuadFilter {
    public:
        enum filterType {
            HPF,
            LPF,
        };

        biQuadFilter(float sampleRate, float cutoff, float q, float gain, filterType filter_type) :
            m_x(biQuadFilter::FF),
            m_y(biQuadFilter::FB),
            m_sample_rate(sampleRate),
            m_cutoff(cutoff),
            m_q(q),
            m_gain(gain),
            m_type(filter_type)
        {
            update_coeficients();
        }
        /**
         * @brief Genereates a biQuadFilter initialzed as a HPF.
         *
         * @param sampleRate sample rate in Hz
         * @param cutoff cuttof frequency in Hz
         * @param q Q-factor
         * @return returns a `biQuadFilter *`
         */
        inline static biQuadFilter *new_hpf(float sampleRate, float cutoff, float q)
        {
            return new biQuadFilter(sampleRate, cutoff, q, GAIN, HPF);
        }

        /**
         * @brief Genereates a biQuadFilter initialzed as a LPF.
         *
         * @param sampleRate sample rate in Hz
         * @param cutoff cuttof frequency in Hz
         * @param q Q-factor
         * @return returns a `biQuadFilter *`
         */
        inline static biQuadFilter *new_lpf(float sampleRate, float cutoff, float q)
        {
            return new biQuadFilter(sampleRate, cutoff, q, GAIN, LPF);
        }

        /**
         * @brief updates filter with new sample. Returns filtered sample
         *
         * @param sample
         */
        float update(float sample);

        // setters
#define __setter(method,input,variable)         \
            inline void method(float input)     \
            {                                   \
                if (variable != input){         \
                    variable = input;           \
                    update_coeficients();             \
                    DEBUG("Updated filter parameter: %f", input);  \
                }                                       \
            }                           

        __setter(set_sample_rate, sampleRate, m_sample_rate)
            __setter(set_q, q, m_q)
            __setter(set_gain, gain, m_gain)
            __setter(set_cutoff, cutoff, m_cutoff)



    protected:
        // updates coefficients based on filter type
        void update_coeficients();

        static constexpr size_t FF = 3;
        static constexpr size_t FB = 2;


        std::deque<float> m_x;   // input buffer
        std::deque<float> m_y;   // output buffer


        float m_sample_rate; // sample rate in Hz
        float m_cutoff;     // cutoff frequency in Hz
        float m_q;          // q factor
        float m_gain;       // gain
        filterType m_type;

        float a[FB];     // feedback coefficients
        float b[FF];     // feedforward coefficients

    };  // class biQuadFilter

}; // namespace b3