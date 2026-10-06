#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_I2C_PORT      0
#define OLED_SDA_PIN       1
#define OLED_SCL_PIN       3
#define OLED_I2C_ADDR_7BIT 0x3C
#define OLED_I2C_HZ        400000
#define OLED_REFRESH_HZ    10 /* info refresh rate, default 10Hz */
#define BOOT_PIN           9  /* ESP32-C3 BOOT, active low */
#define BOOT_HOLD_MS       3000

#define ENC_A_PIN          5
#define ENC_B_PIN          6
#define ENC_BTN_PIN        7

#if OLED_REFRESH_HZ < 1
#error OLED_REFRESH_HZ must be >= 1
#endif
#define OLED_REFRESH_MS    (1000 / OLED_REFRESH_HZ)

typedef int (*oled_esc_xfer_fn)(const char *cmd, char *resp, size_t resp_sz);

void oled_ui_init(void);
void oled_ui_draw_info(const char *text);
void oled_ui_set_message(const char *msg);
void oled_ui_power_off(void);
void oled_ui_show_wifi(const char *ssid);
void oled_ui_wifi_tick(void);

/* Blocking menu loop. Returns after BOOT held BOOT_HOLD_MS. */
void oled_ui_loop(oled_esc_xfer_fn xfer);

#ifdef __cplusplus
}
#endif
