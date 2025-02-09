#include "led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/gpio.h"

static TaskHandle_t s_led_task_handle = NULL;

static SemaphoreHandle_t s_led_semaphore = NULL;
static uint32_t s_led_const_color = LED_COL_INIT;
static uint32_t s_led_flash_color = 0;

void led_set_constant_color(uint32_t color) {
    ESP_LOGI("LED", "Setting LED to color 0x%08x", (unsigned int) color);
    xSemaphoreTake(s_led_semaphore, portMAX_DELAY);
    s_led_const_color = color;
    xSemaphoreGive(s_led_semaphore);
}

void led_start_flash(uint32_t color) {
    ESP_LOGI("LED", "Flashing LED to color 0x%08x", (unsigned int) color);
    xSemaphoreTake(s_led_semaphore, portMAX_DELAY);
    s_led_flash_color = color;
    xSemaphoreGive(s_led_semaphore);
}

uint32_t apply_brightness(uint32_t color) {
    uint8_t r = (color >> 16) & 0xff;
    uint8_t g = (color >> 8) & 0xff;
    uint8_t b = color & 0xff;

    r = (r * LED_BRIGHTNESS) / 255;
    g = (g * LED_BRIGHTNESS) / 255;
    b = (b * LED_BRIGHTNESS) / 255;

    return (r << 16) | (g << 8) | b;
}

void write_led_color(uint32_t color) {
    static struct led_state led_state = {0};
    led_state.leds[0] = apply_brightness(color);
    ws2812_write_leds(led_state);
}

void led_task_handler(void *args) {
    ws2812_control_init();
    vSemaphoreCreateBinary(s_led_semaphore);

    for (;;) {
        uint32_t const_color = 0;
        uint32_t flash_color = 0;

        xSemaphoreTake(s_led_semaphore, portMAX_DELAY);
        const_color = s_led_const_color;
        flash_color = s_led_flash_color;
        xSemaphoreGive(s_led_semaphore);

        if (flash_color) {
            for (int i = 0; i < LED_FLASH_COUNT; ++i) {
                write_led_color(flash_color);
                vTaskDelay(LED_FLASH_MS / portTICK_PERIOD_MS);
                write_led_color(0);
                vTaskDelay(LED_FLASH_MS / portTICK_PERIOD_MS);
            }

            xSemaphoreTake(s_led_semaphore, portMAX_DELAY);
            s_led_flash_color = 0;
            xSemaphoreGive(s_led_semaphore);
        }

        write_led_color(const_color);
        vTaskDelay(10);
    }
}

int led_dispatch() {
    xTaskCreate(led_task_handler, "LedTask", 2048, NULL, configMAX_PRIORITIES - 3, &s_led_task_handle);

    return 0;
}