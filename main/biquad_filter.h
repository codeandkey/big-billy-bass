
#ifndef BIQUAD_FILTER_H
#define BIQUAD_FILTER_H

#include <stdint.h>

enum {
    LPF,
    HPF
};

typedef struct {
    float *buffer;
    int head;
    int tail;
    int size;
    int buff_size;
} CircularBuffer;


typedef struct bqf_t {
    int type;                // Filter type (LPF or HPF)
    float cutoff_freq;       // Cutoff frequency of the filter
    float sampling_freq;     // Sampling frequency
    float a[2];              // ff coefficients
    float b[3];              // fb coefficients
    float x[3];              // fb buffer
    float y[2];              // ff buffer
    CircularBuffer x_p;      // Circular buffer parameters for input samples
    CircularBuffer y_p;      // Circular buffer parameters for output samples
} FilterParams;

void bqf_init(FilterParams *params);

void bqf_set_params(FilterParams *params, int type, float cutoff_freq, float sampling_freq);

int bqf_update(FilterParams *params, float input);

int pcm16_to_mono(const int16_t *data);

#endif // BIQUAD_FILTER_H
