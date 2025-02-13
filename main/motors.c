#include "motors.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#define MOTOR_CONTROL_FREQ (1000)
#define MOTOR_BUFSIZE (1024)
#define MOTOR_HEALTH_INTERVAL_S (10)
#define TAG "MOTORS"

#define PIN_MOTOR_LPF_BWD GPIO_NUM_20
#define PIN_MOTOR_LPF_FWD GPIO_NUM_21
#define PIN_MOTOR_LPF_SPD GPIO_NUM_22
#define PIN_MOTOR_HPF_BWD GPIO_NUM_23
#define PIN_MOTOR_HPF_FWD GPIO_NUM_26
#define PIN_MOTOR_HPF_SPD GPIO_NUM_25

#define MOTOR_HPF_DUTY_PCT (80) // 0-100
#define MOTOR_LPF_DUTY_PCT (80) // 0-100

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz

static TaskHandle_t s_motor_task_handle = NULL;
static RingbufHandle_t s_motor_ringbuf = NULL;
static SemaphoreHandle_t s_motor_semaphore = NULL;

static int s_motor_sample_rate = 0;
static int s_motor_chunk_timestamp_us = 0;
static int s_motor_health_timestamp_us = 0;
static int s_motor_n_health_reports = 0;
static int s_motor_write_count = 0;

void motors_submit_filter_rms(uint8_t* lpf, uint8_t* hpf, int size) {
    xRingbufferSend(s_motor_ringbuf, (void*)lpf, size, 0);
    xRingbufferSend(s_motor_ringbuf, (void*)hpf, size, 0);
}

void motors_set_sample_rate(int rate) {
    xSemaphoreTake(s_motor_semaphore, portMAX_DELAY);
    s_motor_sample_rate = rate;
    xSemaphoreGive(s_motor_semaphore);
}

void motors_handle_sample(uint8_t lpf, uint8_t hpf) {
    ++s_motor_write_count;
}

void motors_handle_chunk(void* data, size_t size) {
    uint8_t* lpf = (uint8_t*)data;
    uint8_t* hpf = (uint8_t*)(data + size / 2);

    xSemaphoreTake(s_motor_semaphore, portMAX_DELAY);
    int sample_rate = s_motor_sample_rate;
    xSemaphoreGive(s_motor_semaphore);

    int elapsed_health_s = (esp_timer_get_time() - s_motor_health_timestamp_us) / 1000000;

    if (elapsed_health_s / MOTOR_HEALTH_INTERVAL_S > s_motor_n_health_reports) {
        ++s_motor_n_health_reports;
        ESP_LOGI(TAG, "Effective rate: %lli w/s", ((long) s_motor_write_count * 1000000) / esp_timer_get_time());
    }

    for (;;) {
        int chunk_elapsed_us = esp_timer_get_time() - s_motor_chunk_timestamp_us;
        int cursor = (chunk_elapsed_us * sample_rate) / 1000000;

        if (cursor > size) {
            break;
        }

        motors_handle_sample(lpf[cursor], hpf[cursor]);
        vTaskDelay(pdMS_TO_TICKS(1000 / MOTOR_CONTROL_FREQ));
    }

    s_motor_chunk_timestamp_us += (size * 1000000) / sample_rate;
}

void motors_write(int is_lpf, int direction) {
    static const int direction_pins[2][2] = {
        { PIN_MOTOR_LPF_FWD, PIN_MOTOR_LPF_BWD },
        { PIN_MOTOR_HPF_FWD, PIN_MOTOR_HPF_BWD }
    };

    static const int channels[2] = { LEDC_CHANNEL_0, LEDC_CHANNEL_1 };
    static const int duty[2] = {
        MOTOR_LPF_DUTY_PCT * (1 << LEDC_DUTY_RES) / 100,
        MOTOR_HPF_DUTY_PCT * (1 << LEDC_DUTY_RES) / 100
    };

    switch (direction) {
    case -1:
        gpio_set_level(direction_pins[is_lpf][1], 0);
        gpio_set_level(direction_pins[is_lpf][0], 1);
        ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, channels[is_lpf], duty[is_lpf], 0);
        break;
    case 1:
        gpio_set_level(direction_pins[is_lpf][0], 0);
        gpio_set_level(direction_pins[is_lpf][1], 1);
        ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, channels[is_lpf], duty[is_lpf], 0);
        break;
    case 0:
        gpio_set_level(direction_pins[is_lpf][0], 0);
        gpio_set_level(direction_pins[is_lpf][1], 0);
        ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, channels[is_lpf], 0, 0);
        break;
    }
}

void motors_init_pins() {
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_MOTOR_LPF_SPD,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };

    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.gpio_num = PIN_MOTOR_HPF_SPD;
    ledc_channel.channel = LEDC_CHANNEL_1;

    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // Set up the GPIO pins
    gpio_config_t gpio_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_conf.pin_bit_mask = (1ULL << PIN_MOTOR_LPF_BWD) | (1ULL << PIN_MOTOR_LPF_FWD) |
                               (1ULL << PIN_MOTOR_HPF_BWD) | (1ULL << PIN_MOTOR_HPF_FWD);

    ESP_ERROR_CHECK(gpio_config(&gpio_conf));

    ESP_LOGI(TAG, "Initialized motor pins");

    motors_write(0, 0);
    motors_write(1, 0);

    ESP_LOGI(TAG, "Wrote initial motor pin states");
}

void motors_task_handler() {
    motors_init_pins();

    s_motor_semaphore = xSemaphoreCreateBinary();
    s_motor_ringbuf = xRingbufferCreate(MOTOR_BUFSIZE, RINGBUF_TYPE_BYTEBUF);

    size_t recv_size = 0;
    void* recv_data = NULL;
    
    for (;;) {
        recv_data = xRingbufferReceive(s_motor_ringbuf, &recv_size, 1);

        if (!recv_data) {
            s_motor_chunk_timestamp_us = 0;
            ESP_LOGW(TAG, "Motors interrupted");
            continue;
        }

        if (s_motor_chunk_timestamp_us == 0) {
            s_motor_chunk_timestamp_us = esp_timer_get_time();
            s_motor_health_timestamp_us = s_motor_chunk_timestamp_us;
            s_motor_write_count = 0;
        }

        motors_handle_chunk(recv_data, recv_size);
        vRingbufferReturnItem(s_motor_ringbuf, recv_data);
    }
}

int motors_dispatch() {
    xTaskCreate(motors_task_handler, "MotorsTask", 2048, NULL, configMAX_PRIORITIES - 3, &s_motor_task_handle);
    return 0;
}