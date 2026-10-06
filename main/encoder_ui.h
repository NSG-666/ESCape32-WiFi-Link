#pragma once

#include "sdkconfig.h"
#include "encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

void encoder_ui_init(void);
int encoder_ui_poll(rotary_encoder_event_t *ev);
void encoder_ui_set_accel(int enable);

#ifdef __cplusplus
}
#endif
