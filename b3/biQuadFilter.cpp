#include "biQuadFilter.h"

#define _USE_MATH_DEFINES
#include <cmath>

using namespace b3;

float biQuadFilter::update(float sample)
{
    m_x.push_front(sample);
    while (m_x.size() > FF)
        m_x.pop_back();


    float y = 0;
    for (int i = 0; i < m_x.size(); i++)
        y += m_x[i] * b[i];
    for (int i = 0;i < m_y.size(); i++)
        y -= m_y[i] * a[i];

    m_y.push_front(y);
    while (m_y.size() > FB)
        m_y.pop_back();
    return y;
}

void biQuadFilter::updateCoeffs()
{
    /** ref: https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
     * coefficients in eq(2) are normalized to feed-forward and feed-backward coefficients.
     *  - FF: (b0/a0, b1/a0, b2/a0)
     *  - FB: (a1/a0, a2/a0)
     * where a0 is the coefficient for the current sample, a1 is the coefficient for the previous sample, and a2 is the coefficient for the sample before that. (same for b)
     */

    float w0 = 2 * M_PI * m_cutoff / m_sampleRate;

    // Q value is used in the calculation of the coefficients for LPF and HPF 
    float alpha = sin(w0) / (2 * m_q);

    float b0, b1, b2, a0, a1, a2;

    switch (m_filterType) {
    case LPF:
        b0 = (1 - cos(w0)) / 2;
        b1 = b0 * 2;
        b2 = b0;
        a0 = 1 + alpha;
        a1 = -2 * cos(w0);
        a2 = 1 - alpha;
        break;
    case HPF:
        b0 = (1 + cos(w0)) / 2;
        b1 = -b0 * 2;
        b2 = b0;
        a0 = 1 + alpha;
        a1 = -2 * cos(w0);
        a2 = 1 - alpha;
        break;
    }

    // normalize the coefficients
    b[0] = b0 / a0;
    b[1] = b1 / a0;
    b[2] = b2 / a0;
    a[0] = a1 / a0;
    a[1] = a2 / a0;

}