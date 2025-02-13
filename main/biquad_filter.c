#include "biquad_filter.h"
#include <math.h>



#define PI 3.14159265358979323846
#define LEN(x) (sizeof(x) / sizeof(x[0]))

int cb_is_full(CircularBuffer *cb)
{
    return cb->size == cb->buff_size;
}

int cb_is_empty(CircularBuffer *cb)
{
    return cb->size == 0;
}

int cb_at(CircularBuffer *cb, int index)
{
    return cb->buffer[(cb->tail + index) % cb->buff_size];
}

void cb_enqueue(CircularBuffer *cb, int item)
{
    if (cb_is_full(cb))
        return;
    cb->buffer[cb->head] = item;
    cb->head = (cb->head + 1) % cb->buff_size;
    cb->size++;
}

void cb_init(CircularBuffer *cb)
{
    cb->head = 0;
    cb->tail = 0;
    cb->size = 0;
}

int cb_dequeue(CircularBuffer *cb)
{
    if (cb_is_empty(cb)) {
        return -1; // Return an error value
    }
    int item = cb->buffer[cb->tail];
    cb->tail = (cb->tail + 1) % cb->buff_size;
    cb->size--;
    return item;
}


void bqf_init(FilterParams *params)
{
    for (int i = 0; i < 3; i++) {
        params->x[i] = 0;
        params->y[i] = 0;
    }
    params->x_p.buff_size = sizeof(params->x) / sizeof(params->x[0]);
    params->y_p.buff_size = sizeof(params->y) / sizeof(params->y[0]);
    params->x_p.buffer = params->x;
    params->y_p.buffer = params->y;
    cb_init(&params->x_p);
    cb_init(&params->y_p);
}
void bqf_set_params(FilterParams *params, int type, float cutoff_freq, float sampling_freq)
{
    params->type = type;
    params->cutoff_freq = cutoff_freq;
    params->sampling_freq = sampling_freq;

    float w0 = 2 * PI * cutoff_freq / sampling_freq;
    float alpha = sin(w0) / 2;
    float a[3], b[3];
    switch (type) {
    case LPF:
        b[0] = (1 - cos(w0)) / 2;
        b[1] = 1 - cos(w0);
        b[2] = (1 - cos(w0)) / 2;
        a[0] = 1 + alpha;
        a[1] = -2 * cos(w0);
        a[2] = 1 - alpha;
        break;
    case HPF:
        b[0] = (1 + cos(w0)) / 2;
        b[1] = -(1 + cos(w0));
        b[2] = (1 + cos(w0)) / 2;
        a[0] = 1 + alpha;
        a[1] = -2 * cos(w0);
        a[2] = 1 - alpha;
        break;
    default:
        break;
    }

    params->b[0] = b[0] / a[0];
    params->b[1] = b[1] / a[0];
    params->b[2] = b[2] / a[0];
    params->a[0] = a[1] / a[0];
    params->a[1] = a[2] / a[0];
}

int bqf_update(FilterParams *params, float input)
{
    for (int i = 0; i < LEN(params->x); i++)
        cb_enqueue(&params->x_p, params->x[i]);

    float y = 0;
    for (int i = 0; i < LEN(params->x); i++)
        y += cb_at(&params->x_p, i) * params->b[i];
    for (int i = 0; i < LEN(params->y); i++)
        y -= cb_at(&params->y_p, i) * params->a[i];
    cb_enqueue(&params->y_p, y);
    return y;
}

int pcm16_to_mono(const int16_t *data)
{
    return (data[0] + data[1]) / 2;
}

