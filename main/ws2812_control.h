#ifndef WS2812_CONTROL_H
#define WS2812_CONTROL_H

#define CONFIG_WS2812_NUM_LEDS 1
#define CONFIG_WS2812_LED_RMT_TX_GPIO 21
#define CONFIG_WS2812_LED_RMT_TX_CHANNEL 0
#define CONFIG_WS2812_T0H 14
#define CONFIG_WS2812_T1H 52
#define CONFIG_WS2812_T0L 52
#define CONFIG_WS2812_T1L 14
#define BITS_PER_LED_CMD 24

#include <stdint.h>
#include "sdkconfig.h"
#include "esp_err.h"

#define NUM_LEDS	CONFIG_WS2812_NUM_LEDS

#ifdef __cplusplus
extern "C" {
#endif

// This structure is used for indicating what the colors of each LED should be set to.
// There is a 32bit value for each LED. Only the lower 3 bytes are used and they hold the
// Red (byte 2), Green (byte 1), and Blue (byte 0) values to be set.
struct led_state {
    uint32_t leds[NUM_LEDS];
};

// Setup the hardware peripheral. Only call this once.
esp_err_t ws2812_control_init(void);

// Update the LEDs to the new state. Call as needed.
// This function will block the current task until the RMT peripheral is finished sending 
// the entire sequence.
esp_err_t ws2812_write_leds(struct led_state new_state);

#endif

#ifdef __cplusplus
}
#endif
