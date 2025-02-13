#ifndef MOTORS_H
#define MOTORS_H

#include <stdint.h>

int motors_dispatch();
void motors_submit_filter_rms(uint8_t* lpf, uint8_t* hpf, int size);

#endif