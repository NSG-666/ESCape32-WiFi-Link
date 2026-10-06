#pragma once

#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WOUO_W 128
#define WOUO_H 64
#define WOUO_LINE_H 16
#define WOUO_ROWS 4

void wouo_anim(float *v, float trg, float k);

void wouo_draw_tile(u8g2_t *u8, int select, float highlight, const char *const titles[], int n);
void wouo_draw_list(u8g2_t *u8, float y, float box_y, float box_w,
	int count, const char *(*line)(int i, char *buf, int buflen, int *grey, void *ctx), void *ctx);
void wouo_draw_window(u8g2_t *u8, float y, const char *title, const char *value, int bar, int bar_max);
void wouo_draw_confirm(u8g2_t *u8, float y, const char *title, int yes_sel);

#ifdef __cplusplus
}
#endif
