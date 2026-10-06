#include "oled_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "encoder.h"
#include "encoder_ui.h"
#include "esp32_hw_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "u8g2.h"
#include "wououi.h"

#define UI_HZ 25
#define UI_MS (1000 / UI_HZ)
#define ANIM_K 0.35f
#define PARAM_MAX 48
#define RESP_MAX 2048
#define THROT_MAX 2000
#define BOOT_ESC_WAIT_MS 3000

enum {
	PG_MAIN,
	PG_STATUS,
	PG_PARAMS,
	PG_THROT,
	PG_WIN,
	PG_SAVE,
};

enum { PK_BOOL, PK_ENUM, PK_INT, PK_STR };
enum { DIS_NONE = 0, DIS_BRUSHED = 1, DIS_ARM = 2 };

typedef struct {
	const char *key;
	int kind;
	int minv, maxv, step;
	const char *opts;
	int dis;
} param_meta_t;

typedef struct {
	const param_meta_t *meta;
	char val[48];
	char snap[48];
	int grey;
} param_row_t;

static const param_meta_t k_meta[] = {
	{ "arm", PK_BOOL, 0, 1, 1, "off;on", DIS_NONE },
	{ "damp", PK_BOOL, 0, 1, 1, "off;on", DIS_NONE },
	{ "revdir", PK_BOOL, 0, 1, 1, "off;on", DIS_NONE },
	{ "brushed", PK_BOOL, 0, 1, 1, "off;on", DIS_NONE },
	{ "timing", PK_INT, 1, 31, 1, "", DIS_BRUSHED },
	{ "sine_range", PK_INT, 0, 25, 1, "", DIS_BRUSHED },
	{ "sine_power", PK_INT, 1, 15, 1, "", DIS_BRUSHED },
	{ "freq_min", PK_INT, 16, 48, 1, "", DIS_BRUSHED },
	{ "freq_max", PK_INT, 16, 96, 1, "", DIS_NONE },
	{ "duty_min", PK_INT, 1, 100, 1, "", DIS_NONE },
	{ "duty_max", PK_INT, 1, 100, 1, "", DIS_NONE },
	{ "duty_spup", PK_INT, 1, 100, 1, "", DIS_BRUSHED },
	{ "duty_ramp", PK_INT, 0, 100, 1, "", DIS_BRUSHED },
	{ "duty_rate", PK_INT, 1, 100, 1, "", DIS_NONE },
	{ "duty_drag", PK_INT, 0, 100, 1, "", DIS_NONE },
	{ "duty_lock", PK_ENUM, 0, 2, 1, "off;soft;hard", DIS_BRUSHED },
	{ "throt_mode", PK_ENUM, 0, 3, 1, "fwd;fwd/rev;fwd/brk/rev;fwd/brk", DIS_NONE },
	{ "throt_rev", PK_ENUM, 0, 3, 1, "100%;75%;50%;25%", DIS_NONE },
	{ "throt_brk", PK_INT, 0, 100, 1, "", DIS_NONE },
	{ "throt_set", PK_INT, 0, 100, 1, "", DIS_ARM },
	{ "throt_ztc", PK_BOOL, 0, 1, 1, "off;on", DIS_BRUSHED },
	{ "throt_cal", PK_BOOL, 0, 1, 1, "off;on", DIS_NONE },
	{ "throt_min", PK_INT, 900, 1900, 10, "", DIS_NONE },
	{ "throt_mid", PK_INT, 1000, 2000, 10, "", DIS_NONE },
	{ "throt_max", PK_INT, 1100, 2100, 10, "", DIS_NONE },
	{ "analog_min", PK_INT, 0, 3200, 10, "", DIS_NONE },
	{ "analog_max", PK_INT, 200, 3400, 10, "", DIS_NONE },
	{ "input_mode", PK_ENUM, 0, 7, 1, "servo;analog;serial;iBUS;SBUS;CRSF;EXBUS;HoTT", DIS_NONE },
	{ "input_ch1", PK_INT, 0, 32, 1, "", DIS_NONE },
	{ "input_ch2", PK_INT, 0, 32, 1, "", DIS_NONE },
	{ "telem_mode", PK_ENUM, 0, 6, 1, "KISS;KISS auto;iBUS;S.Port;CRSF;MSB;HoTT", DIS_NONE },
	{ "telem_phid", PK_INT, 0, 28, 1, "", DIS_NONE },
	{ "telem_poles", PK_INT, 2, 100, 1, "", DIS_NONE },
	{ "telem_volt", PK_INT, -80, 160, 1, "", DIS_NONE },
	{ "telem_curr", PK_INT, -100, 200, 1, "", DIS_NONE },
	{ "prot_stall", PK_INT, 0, 3500, 10, "", DIS_BRUSHED },
	{ "prot_temp", PK_INT, 0, 140, 5, "", DIS_NONE },
	{ "prot_sens", PK_ENUM, 0, 2, 1, "ESC;motor;both", DIS_NONE },
	{ "prot_volt", PK_INT, 0, 38, 1, "", DIS_NONE },
	{ "prot_cells", PK_INT, 0, 24, 1, "", DIS_NONE },
	{ "prot_curr", PK_INT, 0, 999, 1, "", DIS_NONE },
	{ "prot_park", PK_INT, 0, 4, 1, "", DIS_NONE },
	{ "music", PK_STR, 0, 0, 0, "", DIS_NONE },
	{ "volume", PK_INT, 0, 100, 1, "", DIS_NONE },
	{ "beacon", PK_INT, 0, 100, 1, "", DIS_NONE },
	{ "bec", PK_ENUM, 0, 4, 1, "5.5V;6.5V;7.4V;8.4V;12V", DIS_NONE },
	{ "led", PK_INT, 0, 15, 1, "", DIS_NONE },
};

static const char *const k_tiles[] = { "Status", "Params", "Throt" };

static u8g2_t u8g2;
static u8g2_esp32_i2c_ctx_t i2c_ctx;
static oled_esc_xfer_fn esc_xfer;

static int page;
static int tile_sel;
static float tile_hi, tile_hi_trg;
static char info_text[256];
static int info_ok;
static int param_n;
static param_row_t params[PARAM_MAX];
static int param_sel;
static int params_dirty;
static float list_y, list_y_trg;
static float box_y, box_y_trg;
static float box_w, box_w_trg;
static int win_idx;
static int win_val;
static float win_y, win_y_trg;
static int save_yes;
static int throt;
static float throt_bar;
static int status_marquee_x;
static char status_line0[96];
static char esc_resp[RESP_MAX];

static const param_meta_t *find_meta(const char *key)
{
	for (size_t i = 0; i < sizeof k_meta / sizeof k_meta[0]; ++i)
		if (!strcmp(k_meta[i].key, key))
			return &k_meta[i];
	return NULL;
}

static int parse_int(const char *s)
{
	return (int)strtol(s, NULL, 10);
}

static const char *enum_nth(const char *opts, int idx, char *buf, int buflen)
{
	int n = 0;
	const char *p = opts;
	while (*p) {
		const char *e = strchr(p, ';');
		int len = e ? (int)(e - p) : (int)strlen(p);
		if (n == idx) {
			if (len >= buflen) len = buflen - 1;
			memcpy(buf, p, len);
			buf[len] = 0;
			return buf;
		}
		p = e ? e + 1 : p + len;
		if (!e) break;
		++n;
	}
	snprintf(buf, buflen, "%d", idx);
	return buf;
}

static int row_by_key(const char *key)
{
	for (int i = 0; i < param_n; ++i)
		if (params[i].meta && !strcmp(params[i].meta->key, key))
			return i;
	return -1;
}

static void refresh_grey(void)
{
	int brushed = 0, arm = 0;
	int ib = row_by_key("brushed");
	int ia = row_by_key("arm");
	if (ib >= 0) brushed = parse_int(params[ib].val);
	if (ia >= 0) arm = parse_int(params[ia].val);
	for (int i = 0; i < param_n; ++i) {
		int g = 0;
		if (params[i].meta) {
			if (params[i].meta->dis == DIS_BRUSHED && brushed) g = 1;
			if (params[i].meta->dis == DIS_ARM && arm) g = 1;
		}
		params[i].grey = g;
	}
}

static int clampi(int v, int lo, int hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static int esc_ok(const char *cmd)
{
	if (!esc_xfer) return 0;
	return esc_xfer(cmd, esc_resp, sizeof esc_resp) >= 0;
}

static int load_params(void)
{
	if (!esc_ok("show\n")) {
		param_n = 0;
		return 0;
	}
	param_n = 0;
	params_dirty = 0;
	char *p = esc_resp;
	while (*p && param_n < PARAM_MAX) {
		char *nl = strchr(p, '\n');
		if (nl) *nl = 0;
		char *col = strstr(p, ": ");
		if (col) {
			*col = 0;
			const param_meta_t *m = find_meta(p);
			if (m) {
				params[param_n].meta = m;
				snprintf(params[param_n].val, sizeof params[param_n].val, "%s", col + 2);
				snprintf(params[param_n].snap, sizeof params[param_n].snap, "%s", col + 2);
				++param_n;
			}
		}
		if (!nl) break;
		p = nl + 1;
	}
	refresh_grey();
	param_sel = 0;
	while (param_sel < param_n && params[param_sel].grey)
		++param_sel;
	if (param_sel >= param_n) param_sel = 0;
	return 1;
}

static int apply_set(int idx, const char *val)
{
	if (idx < 0 || idx >= param_n || !params[idx].meta) return 0;
	char cmd[80];
	snprintf(cmd, sizeof cmd, "set %s %s\n", params[idx].meta->key, val);
	if (!esc_ok(cmd)) return 0;
	snprintf(params[idx].val, sizeof params[idx].val, "%s", val);
	params_dirty = 0;
	for (int i = 0; i < param_n; ++i) {
		if (strcmp(params[i].val, params[i].snap)) {
			params_dirty = 1;
			break;
		}
	}
	refresh_grey();
	return 1;
}

static void revert_params(void)
{
	for (int i = 0; i < param_n; ++i) {
		if (!params[i].meta) continue;
		if (!strcmp(params[i].val, params[i].snap)) continue;
		apply_set(i, params[i].snap);
	}
	params_dirty = 0;
}

static void save_params(void)
{
	esc_ok("save\n");
	for (int i = 0; i < param_n; ++i)
		memcpy(params[i].snap, params[i].val, sizeof params[i].snap);
	params_dirty = 0;
}

static void list_layout(int sel, int count)
{
	int max_off = (count - WOUO_ROWS) * WOUO_LINE_H;
	if (max_off < 0) max_off = 0;
	int off = sel * WOUO_LINE_H - (WOUO_ROWS / 2) * WOUO_LINE_H;
	if (off < 0) off = 0;
	if (off > max_off) off = max_off;
	list_y_trg = (float)-off;
	box_y_trg = list_y_trg + sel * WOUO_LINE_H;
}

static int move_sel(int sel, int count, int dir, int (*grey_fn)(int))
{
	if (count <= 0) return 0;
	int i = sel;
	for (int n = 0; n < count; ++n) {
		i += dir;
		if (i < 0) i = count - 1;
		else if (i >= count) i = 0;
		if (!grey_fn || !grey_fn(i))
			return i;
	}
	return sel;
}

static int param_grey(int i)
{
	return i >= 0 && i < param_n && params[i].grey;
}

static const char *param_line(int i, char *buf, int buflen, int *grey, void *ctx)
{
	(void)ctx;
	*grey = params[i].grey;
	if (!params[i].meta) {
		snprintf(buf, buflen, "?");
		return buf;
	}
	char right[24];
	if (params[i].meta->kind == PK_ENUM)
		enum_nth(params[i].meta->opts, parse_int(params[i].val), right, sizeof right);
	else if (params[i].meta->kind == PK_STR)
		snprintf(right, sizeof right, "%.10s", params[i].val);
	else
		snprintf(right, sizeof right, "%.23s", params[i].val);

	snprintf(buf, buflen, "%-11s %s", params[i].meta->key, right);
	return buf;
}

static int line_width(const char *s)
{
	u8g2_SetFont(&u8g2, u8g2_font_6x12_tf);
	return u8g2_GetStrWidth(&u8g2, s) + 10;
}

static void enter_main(void)
{
	page = PG_MAIN;
	tile_hi = 0;
	tile_hi_trg = 1;
	encoder_ui_set_accel(0);
}

static void enter_status(void)
{
	page = PG_STATUS;
	info_ok = 0;
	info_text[0] = 0;
	status_line0[0] = 0;
	status_marquee_x = 0;
	encoder_ui_set_accel(0);
}

static void enter_params(void)
{
	page = PG_PARAMS;
	load_params();
	list_y = 0;
	box_y = 0;
	box_w = 0;
	list_layout(param_sel, param_n);
	char buf[32];
	int grey;
	box_w_trg = (float)line_width(param_line(param_sel, buf, sizeof buf, &grey, NULL));
	encoder_ui_set_accel(0);
}

static void enter_throt(void)
{
	page = PG_THROT;
	throt = 0;
	throt_bar = 0;
	encoder_ui_set_accel(1);
	esc_ok("throt 0\n");
}

static void leave_throt(void)
{
	esc_ok("throt 0\n");
	throt = 0;
}

static void enter_win(int idx)
{
	if (idx < 0 || idx >= param_n || !params[idx].meta) return;
	if (params[idx].meta->kind == PK_STR || params[idx].grey) return;
	win_idx = idx;
	win_val = parse_int(params[idx].val);
	win_y = 64;
	win_y_trg = 12;
	page = PG_WIN;
	encoder_ui_set_accel(1);
}

static void enter_save(void)
{
	page = PG_SAVE;
	save_yes = 1;
	win_y = 64;
	win_y_trg = 11;
	encoder_ui_set_accel(0);
}

static void poll_info(void)
{
	if (esc_ok("info\n")) {
		snprintf(info_text, sizeof info_text, "%.*s", (int)sizeof info_text - 1, esc_resp);
		info_ok = 1;
	} else {
		info_ok = 0;
	}
}

static void send_throt(void)
{
	char cmd[24];
	snprintf(cmd, sizeof cmd, "throt %d\n", throt);
	esc_ok(cmd);
}

static void draw_status(void)
{
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
	u8g2_SetFontPosTop(&u8g2);
	u8g2_SetDrawColor(&u8g2, 1);

	const char *p = info_ok ? info_text : "No ESC / timeout";
	const char *nl = strchr(p, '\n');
	size_t n0 = nl ? (size_t)(nl - p) : strlen(p);
	if (n0 >= sizeof status_line0)
		n0 = sizeof status_line0 - 1;
	if (n0 != strlen(status_line0) || memcmp(status_line0, p, n0)) {
		memcpy(status_line0, p, n0);
		status_line0[n0] = 0;
		status_marquee_x = 0;
	}

	int w0 = u8g2_GetStrWidth(&u8g2, status_line0);
	if (w0 <= WOUO_W) {
		u8g2_DrawStr(&u8g2, 0, 0, status_line0);
		status_marquee_x = 0;
	} else {
		int cycle = w0 + 24;
		int x = -(status_marquee_x % cycle);
		u8g2_SetClipWindow(&u8g2, 0, 0, WOUO_W, 10);
		u8g2_DrawStr(&u8g2, x, 0, status_line0);
		u8g2_DrawStr(&u8g2, x + cycle, 0, status_line0);
		u8g2_SetMaxClipWindow(&u8g2);
		status_marquee_x++;
	}

	p = nl ? nl + 1 : "";
	int y = 10;
	char line[22];
	while (*p && y < 64) {
		size_t n = 0;
		while (p[n] && p[n] != '\n' && n < sizeof line - 1) ++n;
		memcpy(line, p, n);
		line[n] = 0;
		u8g2_DrawStr(&u8g2, 0, y, line);
		y += 10;
		p += n;
		if (*p == '\n') ++p;
	}
}

static void draw_throt(void)
{
	u8g2_SetFont(&u8g2, u8g2_font_helvB12_tr);
	u8g2_SetFontPosBaseline(&u8g2);
	u8g2_SetDrawColor(&u8g2, 1);
	u8g2_DrawStr(&u8g2, 8, 18, "Throttle");
	char buf[16];
	snprintf(buf, sizeof buf, "%d", throt);
	int tw = u8g2_GetStrWidth(&u8g2, buf);
	u8g2_DrawStr(&u8g2, WOUO_W - 8 - tw, 18, buf);

	int x = 8, w = WOUO_W - 16, y = 32, h = 14;
	u8g2_DrawFrame(&u8g2, x, y, w, h);
	int fill = (int)(throt_bar * (w - 4) / THROT_MAX);
	if (fill < 0) fill = 0;
	if (fill > w - 4) fill = w - 4;
	u8g2_DrawBox(&u8g2, x + 2, y + 2, fill, h - 4);

	u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
	u8g2_SetFontPosTop(&u8g2);
	u8g2_DrawStr(&u8g2, 8, 52, "long press = back");
}

static void handle_event(const rotary_encoder_event_t *e)
{
	switch (page) {
	case PG_MAIN:
		if (e->type == RE_ET_CHANGED) {
			tile_sel += e->diff > 0 ? 1 : -1;
			if (tile_sel < 0) tile_sel = 2;
			if (tile_sel > 2) tile_sel = 0;
			tile_hi = 0;
			tile_hi_trg = 1;
		} else if (e->type == RE_ET_BTN_CLICKED) {
			if (tile_sel == 0) enter_status();
			else if (tile_sel == 1) enter_params();
			else enter_throt();
		}
		break;
	case PG_STATUS:
		if (e->type == RE_ET_BTN_LONG_PRESSED)
			enter_main();
		break;
	case PG_PARAMS:
		if (e->type == RE_ET_CHANGED && param_n) {
			param_sel = move_sel(param_sel, param_n, e->diff > 0 ? 1 : -1, param_grey);
			list_layout(param_sel, param_n);
			char buf[32];
			int grey;
			box_w_trg = (float)line_width(param_line(param_sel, buf, sizeof buf, &grey, NULL));
		} else if (e->type == RE_ET_BTN_CLICKED && param_n) {
			const param_meta_t *m = params[param_sel].meta;
			if (!m || params[param_sel].grey) break;
			if (m->kind == PK_BOOL) {
				int v = parse_int(params[param_sel].val) ? 0 : 1;
				char s[8];
				snprintf(s, sizeof s, "%d", v);
				apply_set(param_sel, s);
			} else if (m->kind != PK_STR) {
				enter_win(param_sel);
			}
		} else if (e->type == RE_ET_BTN_LONG_PRESSED) {
			if (params_dirty) enter_save();
			else enter_main();
		}
		break;
	case PG_WIN: {
		const param_meta_t *m = params[win_idx].meta;
		if (!m) break;
		int step = m->step > 0 ? m->step : 1;
		if (e->type == RE_ET_CHANGED) {
			win_val = clampi(win_val + (int)e->diff * step, m->minv, m->maxv);
		} else if (e->type == RE_ET_BTN_CLICKED) {
			char s[12];
			snprintf(s, sizeof s, "%d", win_val);
			apply_set(win_idx, s);
			page = PG_PARAMS;
			encoder_ui_set_accel(0);
			list_layout(param_sel, param_n);
		} else if (e->type == RE_ET_BTN_LONG_PRESSED) {
			page = PG_PARAMS;
			encoder_ui_set_accel(0);
		}
		break;
	}
	case PG_SAVE:
		if (e->type == RE_ET_CHANGED)
			save_yes = !save_yes;
		else if (e->type == RE_ET_BTN_CLICKED) {
			if (save_yes) save_params();
			else revert_params();
			enter_main();
		} else if (e->type == RE_ET_BTN_LONG_PRESSED) {
			revert_params();
			enter_main();
		}
		break;
	case PG_THROT:
		if (e->type == RE_ET_CHANGED) {
			throt = clampi(throt + (int)e->diff, 0, THROT_MAX);
			send_throt();
		} else if (e->type == RE_ET_BTN_LONG_PRESSED) {
			leave_throt();
			enter_main();
		}
		break;
	}
}

static void draw_lines(const char *text)
{
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
	u8g2_SetFontPosTop(&u8g2);

	int y = 0;
	const char *p = text ? text : "";
	char line[22];

	while (*p && y < 64) {
		size_t n = 0;
		while (p[n] && p[n] != '\n' && n < sizeof line - 1) ++n;
		memcpy(line, p, n);
		line[n] = 0;
		u8g2_DrawStr(&u8g2, 0, y, line);
		y += 8;
		p += n;
		if (*p == '\n') ++p;
	}

	if (y <= 56)
		u8g2_DrawStr(&u8g2, 0, 56, "PRESS BOOT 3s = WiFi");

	u8g2_SendBuffer(&u8g2);
}

void oled_ui_init(void)
{
	memset(&i2c_ctx, 0, sizeof i2c_ctx);
	i2c_ctx.cfg = (u8g2_esp32_i2c_config_t){
		.i2c_port = OLED_I2C_PORT,
		.sda_pin = OLED_SDA_PIN,
		.scl_pin = OLED_SCL_PIN,
		.clk_hz = OLED_I2C_HZ,
		.dev_addr_7bit = OLED_I2C_ADDR_7BIT,
		.timeout_ms = 1000,
		.reset_pin = U8G2_ESP32_PIN_UNUSED,
	};
	u8g2_esp32_i2c_set_default_context(&i2c_ctx);

	u8g2_Setup_ssd1306_i2c_128x64_noname_f(
		&u8g2,
		U8G2_R2,
		u8x8_byte_esp32_hw_i2c,
		u8x8_gpio_and_delay_esp32_i2c);
	u8x8_SetI2CAddress(u8g2_GetU8x8(&u8g2), OLED_I2C_ADDR_7BIT << 1);
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, 0);
	oled_ui_set_message("ESCape32 OLED");
}

void oled_ui_draw_info(const char *text)
{
	if (!text || !text[0]) {
		oled_ui_set_message("No ESC / timeout");
		return;
	}
	draw_lines(text);
}

void oled_ui_set_message(const char *msg)
{
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
	u8g2_SetFontPosTop(&u8g2);
	u8g2_DrawStr(&u8g2, 0, 0, msg ? msg : "");
	u8g2_DrawStr(&u8g2, 0, 56, "PRESS BOOT 3s = WiFi");
	u8g2_SendBuffer(&u8g2);
}

void oled_ui_power_off(void)
{
	u8g2_SetPowerSave(&u8g2, 1);
}

static char wifi_ssid[32];
static int wifi_marquee_x;

static void draw_wifi_screen(void)
{
	const char *line1 = "WIFI-Link:";
	const char *line2 = wifi_ssid[0] ? wifi_ssid : "";

	u8g2_SetPowerSave(&u8g2, 0);
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
	u8g2_SetFontPosTop(&u8g2);
	u8g2_SetDrawColor(&u8g2, 1);

	int w1 = u8g2_GetStrWidth(&u8g2, line1);
	u8g2_DrawStr(&u8g2, (128 - w1) / 2, 24, line1);

	int w2 = u8g2_GetStrWidth(&u8g2, line2);
	if (w2 <= WOUO_W) {
		u8g2_DrawStr(&u8g2, (128 - w2) / 2, 34, line2);
		wifi_marquee_x = 0;
	} else {
		int cycle = w2 + 24;
		int x = -(wifi_marquee_x % cycle);
		u8g2_SetClipWindow(&u8g2, 0, 34, WOUO_W, 44);
		u8g2_DrawStr(&u8g2, x, 34, line2);
		u8g2_DrawStr(&u8g2, x + cycle, 34, line2);
		u8g2_SetMaxClipWindow(&u8g2);
		wifi_marquee_x++;
	}
	u8g2_SendBuffer(&u8g2);
}

void oled_ui_show_wifi(const char *ssid)
{
	snprintf(wifi_ssid, sizeof wifi_ssid, "%s", ssid && ssid[0] ? ssid : "");
	wifi_marquee_x = 0;
	draw_wifi_screen();
}

void oled_ui_wifi_tick(void)
{
	draw_wifi_screen();
}

void oled_ui_loop(oled_esc_xfer_fn xfer)
{
	esc_xfer = xfer;
	enter_status();
	int pressed_ms = 0;
	int info_acc = 0;
	int boot_esc_wait = 1;
	TickType_t boot_t0 = xTaskGetTickCount();

	for (;;) {
		TickType_t t0 = xTaskGetTickCount();

		rotary_encoder_event_t ev;
		while (encoder_ui_poll(&ev))
			handle_event(&ev);

		if (page == PG_STATUS) {
			info_acc += UI_MS;
			if (info_acc >= OLED_REFRESH_MS) {
				info_acc = 0;
				poll_info();
			}
		} else {
			info_acc = OLED_REFRESH_MS;
		}

		if (boot_esc_wait) {
			if (page != PG_STATUS || info_ok)
				boot_esc_wait = 0;
			else if ((xTaskGetTickCount() - boot_t0) >= pdMS_TO_TICKS(BOOT_ESC_WAIT_MS)) {
				boot_esc_wait = 0;
				enter_main();
			}
		}

		wouo_anim(&tile_hi, tile_hi_trg, ANIM_K);
		wouo_anim(&list_y, list_y_trg, ANIM_K);
		wouo_anim(&box_y, box_y_trg, ANIM_K);
		wouo_anim(&box_w, box_w_trg, ANIM_K);
		wouo_anim(&win_y, win_y_trg, ANIM_K);
		wouo_anim(&throt_bar, (float)throt, ANIM_K);

		u8g2_ClearBuffer(&u8g2);
		u8g2_SetDrawColor(&u8g2, 1);
		switch (page) {
		case PG_MAIN:
			wouo_draw_tile(&u8g2, tile_sel, tile_hi, k_tiles, 3);
			break;
		case PG_STATUS:
			draw_status();
			break;
		case PG_PARAMS:
			wouo_draw_list(&u8g2, list_y, box_y, box_w, param_n, param_line, NULL);
			break;
		case PG_WIN: {
			wouo_draw_list(&u8g2, list_y, box_y, box_w, param_n, param_line, NULL);
			char val[24];
			const param_meta_t *m = params[win_idx].meta;
			if (m && m->kind == PK_ENUM)
				enum_nth(m->opts, win_val, val, sizeof val);
			else
				snprintf(val, sizeof val, "%d", win_val);
			int span = m ? (m->maxv - m->minv) : 1;
			int bar = m ? (win_val - m->minv) : 0;
			if (span < 1) span = 1;
			wouo_draw_window(&u8g2, win_y, m ? m->key : "", val, bar, span);
			break;
		}
		case PG_SAVE:
			wouo_draw_list(&u8g2, list_y, box_y, box_w, param_n, param_line, NULL);
			wouo_draw_confirm(&u8g2, win_y, "Save to flash?", save_yes);
			break;
		case PG_THROT:
			draw_throt();
			break;
		}
		u8g2_SendBuffer(&u8g2);

		if (!gpio_get_level(BOOT_PIN)) {
			pressed_ms += UI_MS;
			if (pressed_ms >= BOOT_HOLD_MS) {
				if (page == PG_THROT) leave_throt();
				break;
			}
		} else {
			pressed_ms = 0;
		}

		vTaskDelayUntil(&t0, pdMS_TO_TICKS(UI_MS));
	}
}
