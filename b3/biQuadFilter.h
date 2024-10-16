#pragma once

#include <cstdint>
#include <cstring>

namespace b3 {

    enum filterType {
        LPF_type,
        HPF_type
    }; // enum filterType


#define Q 0.707
#define GAIN 1


    class biQuadFilter {
    public:
        biQuadFilter(float sampleRate, float cutoff, float q, float gain, filterType type) :
            m_sampleRate(sampleRate),
            m_cutoff(cutoff),
            m_q(q),
            m_gain(gain),
            m_x(biQuadFilter::FF),
            m_y(biQuadFilter::FB),
            m_filterType(type)
        {
            updateCoeffs();
            zero();
        }

        // setters

        inline void setSampleRate(float sampleRate) { m_sampleRate = sampleRate; updateCoeffs(); }
        inline void setCutoff(float cutoff) { m_cutoff = cutoff; updateCoeffs(); }
        inline void setQ(float q) { m_q = q; updateCoeffs(); }
        inline void setGain(float gain) { m_gain = gain; updateCoeffs(); }

        // updates buffers with new sample. Returns filtered sample
        float update(float sample);

        inline void zero()
        {
            m_x.zero();
            m_y.zero();
        }

    private:
        // updates coefficients based on filter type
        void updateCoeffs();

        static constexpr uint8_t FF = 3;
        static constexpr uint8_t FB = 2;


        template <typename sampleType>
        class cBuff {
        public:
            cBuff(uint8_t order) : m_ndx(0), m_order(order)
            {
                m_buffer = new sampleType[order];
                memset(m_buffer, 0, order * sizeof(sampleType));
            }
            ~cBuff() { delete[] m_buffer; }

            inline float operator[](int i) const { return m_buffer[(m_ndx + i) % m_order]; }

            inline void push(float sample)
            {
                m_buffer[m_ndx] = sample;
                m_ndx = (m_ndx + 1) % m_order;
            }

            inline void zero() { memset(m_buffer, 0, m_order); }

        private:
            sampleType *m_buffer;
            uint8_t m_ndx;
            uint8_t m_order;
        };

        cBuff<float> m_x;   // input buffer
        cBuff<float> m_y;   // output buffer


        float m_sampleRate; // sample rate in Hz
        float m_cutoff;     // cutoff frequency in Hz
        float m_q;          // q factor
        float m_gain;       // gain


        float a[FB];     // feedback coefficients
        float b[FF];     // feedforward coefficients

        filterType m_filterType;
    };  // class biQuadFilter

}; // namespace b3