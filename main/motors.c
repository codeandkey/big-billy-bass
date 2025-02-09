#include "motors.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#define MOTOR_CONTROL_FREQ 1000
#define MOTOR_BUFSIZE 1024

static TaskHandle_t s_motor_task_handle = NULL;
static RingBufferHandle_t s_motor_ringbuf = NULL;
static SemaphoreHandle_t s_motor_semaphore = NULL;

static int s_motor_sample_rate = 0;

void motors_submit_filter_rms(uint8_t* lpf, uint8_t* hpf, int size) {
    xRingbufferSend(s_motor_ringbuf, (void*)lpf, size, 0);
    xRingbufferSend(s_motor_ringbuf, (void*)hpf, size, 0);
}

void motors_set_sample_rate(int rate) {
    xSemaphoreTake(s_motor_semaphore, portMAX_DELAY);
    s_motor_sample_rate = rate;
    xSemaphoreGive(s_motor_semaphore);
}

void motors_task_handler() {
    xSemaphoreCreateBinary(s_motor_semaphore);
}

int motors_dispatch() {
    vTaskCreate(motors_task_handler, "MotorsTask", 2048, NULL, configMAX_PRIORITIES - 3, &s_motor_task_handle);
}