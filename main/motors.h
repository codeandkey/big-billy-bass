#ifndef MOTORS_H
#define MOTORS_H

int motors_dispatch();

void motors_submit_filter_rms(uint8_t* lpf, uint8_t* hpf);