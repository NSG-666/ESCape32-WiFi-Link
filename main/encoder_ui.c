#include "encoder_ui.h"
#include "oled_ui.h"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define ENC_Q_LEN 16
#define ENC_LONG_US 700000
#define ENC_ACCEL_COEFF 100

static QueueHandle_t enc_q;
static rotary_encoder_handle_t enc;
static int accel_on;

static void encoder_cb(const rotary_encoder_event_t *event, void *ctx)
{
	xQueueSendToBack((QueueHandle_t)ctx, event, 0);
}

void encoder_ui_init(void)
{
	enc_q = xQueueCreate(ENC_Q_LEN, sizeof(rotary_encoder_event_t));

	rotary_encoder_config_t cfg = ROTARY_ENCODER_DEFAULT_CONFIG();
	cfg.pin_a = ENC_A_PIN;
	cfg.pin_b = ENC_B_PIN;
	cfg.pin_btn = ENC_BTN_PIN;
	cfg.btn_pressed_level = 0;
	cfg.enable_internal_pullup = true;
	cfg.btn_long_press_time_us = ENC_LONG_US;
	cfg.callback = encoder_cb;
	cfg.callback_ctx = enc_q;
	ESP_ERROR_CHECK(rotary_encoder_create(&cfg, &enc));
	rotary_encoder_disable_acceleration(enc);
	accel_on = 0;
}

int encoder_ui_poll(rotary_encoder_event_t *ev)
{
	return xQueueReceive(enc_q, ev, 0) == pdTRUE;
}

void encoder_ui_set_accel(int enable)
{
	if (enable == accel_on)
		return;
	accel_on = enable;
	if (enable)
		rotary_encoder_enable_acceleration(enc, ENC_ACCEL_COEFF);
	else
		rotary_encoder_disable_acceleration(enc);
}
