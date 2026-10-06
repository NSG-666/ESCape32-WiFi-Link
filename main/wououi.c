#include "wououi.h"

#include <string.h>
#include <stdio.h>

void wouo_anim(float *v, float trg, float k)
{
	if (*v == trg)
		return;
	*v += (trg - *v) * k;
	if ((trg - *v > -0.4f && trg - *v < 0.4f) || k >= 1.f)
		*v = trg;
}

void wouo_draw_tile(u8g2_t *u8, int select, float highlight, const char *const titles[], int n)
{
	u8g2_SetFont(u8, u8g2_font_6x12_tf);
	u8g2_SetFontPosTop(u8);
	u8g2_SetDrawColor(u8, 1);

	const int box_w = 36;
	const int box_h = 28;
	const int gap = 4;
	int total = n * box_w + (n - 1) * gap;
	int x0 = (WOUO_W - total) / 2;
	int y = 12;

	for (int i = 0; i < n; ++i) {
		int x = x0 + i * (box_w + gap);
		u8g2_DrawRFrame(u8, x, y, box_w, box_h, 3);
		if (i == select) {
			int inset = (int)(4.f - highlight * 3.f);
			if (inset < 1) inset = 1;
			u8g2_DrawRBox(u8, x + inset, y + inset, box_w - inset * 2, box_h - inset * 2, 2);
			u8g2_SetDrawColor(u8, 0);
		}
		char ch[2] = { titles[i][0], 0 };
		int tw = u8g2_GetStrWidth(u8, ch);
		u8g2_DrawStr(u8, x + (box_w - tw) / 2, y + 8, ch);
		u8g2_SetDrawColor(u8, 1);
	}

	u8g2_SetFont(u8, u8g2_font_helvB12_tr);
	u8g2_SetFontPosBaseline(u8);
	const char *t = titles[select];
	int tw = u8g2_GetStrWidth(u8, t);
	u8g2_DrawStr(u8, (WOUO_W - tw) / 2, 58, t);

	u8g2_SetFont(u8, u8g2_font_6x10_tf);
	u8g2_SetFontPosTop(u8);
	const char *hint = "PRESS BOOT 3s TO WiFi";
	int hw = u8g2_GetStrWidth(u8, hint);
	u8g2_DrawStr(u8, (WOUO_W - hw) / 2, 0, hint);
}

void wouo_draw_list(u8g2_t *u8, float y, float box_y, float box_w,
	int count, const char *(*line)(int i, char *buf, int buflen, int *grey, void *ctx), void *ctx)
{
	u8g2_SetFont(u8, u8g2_font_6x12_tf);
	u8g2_SetFontPosTop(u8);
	u8g2_SetDrawColor(u8, 1);

	int bw = (int)box_w;
	if (bw < 8) bw = 8;
	if (bw > WOUO_W - 8) bw = WOUO_W - 8;
	int by = (int)box_y;
	u8g2_DrawRBox(u8, 0, by, bw, WOUO_LINE_H, 1);

	char buf[32];
	for (int i = 0; i < count; ++i) {
		int iy = (int)y + i * WOUO_LINE_H;
		if (iy + WOUO_LINE_H < 0 || iy > WOUO_H)
			continue;
		int grey = 0;
		const char *s = line(i, buf, sizeof buf, &grey, ctx);
		u8g2_SetDrawColor(u8, (iy + 2 >= by && iy < by + WOUO_LINE_H) ? 0 : 1);
		if (grey && u8g2_GetDrawColor(u8) == 1) {
			/* dim: skip extra pixels by drawing as-is */
		}
		u8g2_DrawStr(u8, 4, iy + 2, s ? s : "");
	}

	u8g2_SetDrawColor(u8, 1);
	if (count > WOUO_ROWS) {
		int bar_h = WOUO_H * WOUO_ROWS / count;
		if (bar_h < 6) bar_h = 6;
		int max_y = - (count - WOUO_ROWS) * WOUO_LINE_H;
		int bar_y = 0;
		if (max_y < 0)
			bar_y = (int)((-y) * (WOUO_H - bar_h) / (-max_y));
		u8g2_DrawVLine(u8, WOUO_W - 3, 0, WOUO_H);
		u8g2_DrawBox(u8, WOUO_W - 5, bar_y, 4, bar_h);
	}
}

void wouo_draw_window(u8g2_t *u8, float y, const char *title, const char *value, int bar, int bar_max)
{
	const int w = 108;
	const int h = 40;
	int x = (WOUO_W - w) / 2;
	int yy = (int)y;

	u8g2_SetDrawColor(u8, 0);
	u8g2_DrawBox(u8, x, yy, w, h);
	u8g2_SetDrawColor(u8, 1);
	u8g2_DrawRFrame(u8, x, yy, w, h, 2);

	u8g2_SetFont(u8, u8g2_font_6x12_tf);
	u8g2_SetFontPosTop(u8);
	u8g2_DrawStr(u8, x + 6, yy + 4, title ? title : "");
	int tw = u8g2_GetStrWidth(u8, value ? value : "");
	u8g2_DrawStr(u8, x + w - 6 - tw, yy + 4, value ? value : "");

	int bar_x = x + 8;
	int bar_w = w - 16;
	int bar_y = yy + 24;
	u8g2_DrawFrame(u8, bar_x, bar_y, bar_w, 8);
	int fill = 0;
	if (bar_max > 0) {
		fill = bar * (bar_w - 4) / bar_max;
		if (fill < 0) fill = 0;
		if (fill > bar_w - 4) fill = bar_w - 4;
	}
	u8g2_DrawBox(u8, bar_x + 2, bar_y + 2, fill, 4);
}

void wouo_draw_confirm(u8g2_t *u8, float y, const char *title, int yes_sel)
{
	const int w = 116;
	const int h = 42;
	int x = (WOUO_W - w) / 2;
	int yy = (int)y;

	u8g2_SetDrawColor(u8, 0);
	u8g2_DrawBox(u8, x, yy, w, h);
	u8g2_SetDrawColor(u8, 1);
	u8g2_DrawRFrame(u8, x, yy, w, h, 2);

	u8g2_SetFont(u8, u8g2_font_6x12_tf);
	u8g2_SetFontPosTop(u8);
	int tw = u8g2_GetStrWidth(u8, title);
	u8g2_DrawStr(u8, x + (w - tw) / 2, yy + 4, title);

	const char *a = "Yes";
	const char *b = "No";
	int ax = x + 18;
	int bx = x + 70;
	int oy = yy + 22;
	if (yes_sel) {
		u8g2_DrawRBox(u8, ax - 4, oy - 2, 28, 14, 1);
		u8g2_SetDrawColor(u8, 0);
		u8g2_DrawStr(u8, ax, oy, a);
		u8g2_SetDrawColor(u8, 1);
		u8g2_DrawStr(u8, bx, oy, b);
	} else {
		u8g2_DrawRBox(u8, bx - 4, oy - 2, 24, 14, 1);
		u8g2_DrawStr(u8, ax, oy, a);
		u8g2_SetDrawColor(u8, 0);
		u8g2_DrawStr(u8, bx, oy, b);
		u8g2_SetDrawColor(u8, 1);
	}
}
