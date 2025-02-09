#ifndef LED_H
#define LED_H

#include "ws2812_control.h"

// Color format is 0xGGRRBB
#define LED_COL_INIT 0x00ff00
#define LED_COL_CONNECTED 0x0000ff
#define LED_COL_DISCONNECTED LED_COL_INIT
#define LED_COL_STREAMING LED_COL_CONNECTED

#define LED_FLASH_ERROR 0xff0000
#define LED_FLASH_MS 50
#define LED_FLASH_COUNT 3

#define LED_BRIGHTNESS 80

void led_set_constant_color(uint32_t color);
void led_start_flash(uint32_t color);

int led_dispatch();

#endif