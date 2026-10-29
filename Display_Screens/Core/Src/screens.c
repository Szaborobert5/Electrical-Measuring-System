#include "screens.h"
#include "font.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ════════════════════════════════════════════════════════════════════
   INTERNAL STATE
   ════════════════════════════════════════════════════════════════════ */
static ScreenState_t  _state      = SCREEN_VOLTAGE;
static uint8_t        _force_full = 1;   /* full redraw on next update */

/* One sparkline buffer per screen */
static SparkBuf_t _spark_v;   /* voltage  history */
static SparkBuf_t _spark_i;   /* current  history */
static SparkBuf_t _spark_p;   /* power    history */
static SparkBuf_t _spark_e;   /* energy   history */

/* ════════════════════════════════════════════════════════════════════
   SHARED DRAWING PRIMITIVES
   ════════════════════════════════════════════════════════════════════ */

/* Draw the top status bar:
   [ICON  LABEL]  ● ● ● ●   (dots = current page indicator)        */
static void Draw_TopBar(const char *label, uint16_t accent) {
    /* Background strip */
    ST7789_FillRect(0, 0, 240, 34, PANEL);

    /* Separator line */
    ST7789_DrawHLine(0, 34, 240, accent);

    /* Label */
    ST7789_WriteString(10, 11, label, Font_11x18, accent, PANEL);

    /* State indicator dots */
    for (int i = 0; i < 4; i++) {
        uint16_t col = (i == (int)_state) ? accent : DIM;
        /* Draw a filled 5×5 square as "dot" */
        ST7789_FillRect(200 + i * 10, 15, 5, 5, col);
    }
}

/* Draw the bottom info bar with 3 labeled values                    */
static void Draw_BottomBar(
    const char *l0, const char *v0,
    const char *l1, const char *v1,
    const char *l2, const char *v2,
    uint16_t accent)
{
    /* Background */
    ST7789_FillRect(0, 206, 240, 34, PANEL);
    ST7789_DrawHLine(0, 206, 240, accent);

    /* Three columns — each 80px wide */
    uint16_t cols[3] = {4, 84, 164};
    const char *labels[3] = {l0, l1, l2};
    const char *values[3] = {v0, v1, v2};

    for (int i = 0; i < 3; i++) {
        ST7789_WriteString(cols[i], 210, labels[i], Font_7x10, DIM, PANEL);
        ST7789_WriteString(cols[i], 222, values[i], Font_7x10, accent, PANEL);
    }
}

/* Draw an arc gauge centred at (cx,cy), radius r.
   Range: 135°–405° (270° sweep, starting bottom-left).
   pct: 0.0 – 1.0                                                    */
static void Draw_ArcGauge(uint16_t cx, uint16_t cy, uint16_t r, float pct, uint16_t color, uint16_t track_color, uint8_t thick) {
    float start = 135.0f;
    float sweep = 270.0f;
    float end_track = start + sweep;
    float end_value = start + sweep * pct;
    if (end_value > end_track) end_value = end_track;

    /* Track arc (dimmed) */
    ST7789_DrawArc(cx, cy, r, start, end_track,
                   track_color, thick);
    /* Value arc */
    if (pct > 0.01f)
        ST7789_DrawArc(cx, cy, r, start, end_value,
                       color, thick);
}

/* Draw sparkline in a bounding box (x,y,w,h)                        */
static void Draw_Sparkline(SparkBuf_t *s, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (s->count < 2) return;

    /* Find min/max over buffer */
    float mn = s->buf[0], mx = s->buf[0];
    for (int i = 1; i < s->count; i++) {
        if (s->buf[i] < mn) mn = s->buf[i];
        if (s->buf[i] > mx) mx = s->buf[i];
    }
    float range = mx - mn;
    if (range < 0.001f) range = 1.0f;

    /* Clear the sparkline area */
    uint16_t bg = BLACK;
    /* Determine bg from context — just use black, each screen
       will have already painted its gradient behind this */
    ST7789_FillRect(x, y, w, h, bg);

    /* Draw line segment by segment */
    for (int i = 1; i < s->count; i++) {
        float frac_prev = (s->buf[i-1] - mn) / range;
        float frac_curr = (s->buf[i]   - mn) / range;
        uint16_t x0 = x + (uint16_t)((i-1) * w / (SPARK_LEN-1));
        uint16_t x1 = x + (uint16_t)(i     * w / (SPARK_LEN-1));
        uint16_t y0 = y + h - 1 - (uint16_t)(frac_prev * (h-1));
        uint16_t y1 = y + h - 1 - (uint16_t)(frac_curr * (h-1));

        /* Bresenham line between (x0,y0) and (x1,y1) */
        int dx = (int)x1 - (int)x0;
        int dy = (int)y1 - (int)y0;
        int steps = (ABS(dx) > ABS(dy)) ? ABS(dx) : ABS(dy);
        if (steps == 0) steps = 1;
        float sx = (float)dx / steps;
        float sy = (float)dy / steps;
        float px = x0, py = y0;
        for (int s2 = 0; s2 <= steps; s2++) {
            ST7789_DrawPixel((uint16_t)px, (uint16_t)py, color);
            px += sx; py += sy;
        }
    }
    /* Last point dot */
    if (s->count > 0) {
        float frac = (s->buf[s->count-1] - mn) / range;
        uint16_t lx = x + w - 1;
        uint16_t ly = y + h - 1 - (uint16_t)(frac * (h-1));
        ST7789_FillRect(lx-1, ly-1, 3, 3, color);
    }
}

/* Draw a horizontal progress bar                                    */
static void Draw_ProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, float pct, uint16_t fg, uint16_t bg_bar) {
    uint16_t fill = (uint16_t)(pct * w);
    if (fill > w) fill = w;
    ST7789_FillRect(x,        y, w,    h, bg_bar);
    ST7789_FillRect(x,        y, fill, h, fg);
    /* End cap highlight */
    if (fill > 2)
        ST7789_FillRect(x + fill - 2, y, 2, h,
                        WHITE);
}

/* Draw a small label+value pair in a rounded-ish rectangle          */
static void Draw_InfoBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *label, const char *value, uint16_t accent) {
    ST7789_FillRect(x, y, w, h,  PANEL);
    /* Top micro-bar */
    ST7789_DrawHLine(x, y, w, accent);
    uint16_t tx = x + 4;
    ST7789_WriteString(tx, y + 4, label, Font_7x10, DIM, PANEL);
    ST7789_WriteString(tx, y + 14, value, Font_7x10, accent, PANEL);
}

/* Sparkline push helper */
void Spark_Push(SparkBuf_t *s, float val) {
    s->buf[s->head] = val;
    s->head = (s->head + 1) % SPARK_LEN;
    if (s->count < SPARK_LEN) s->count++;
    /* Reorder into linear array for drawing */
    /* buf is a ring; Draw_Sparkline reads buf[0..count-1].
       Re-linearise into place: rotate so oldest is at index 0. */
    if (s->count == SPARK_LEN) {
        /* Rotate in-place */
        float tmp[SPARK_LEN];
        for (int i = 0; i < SPARK_LEN; i++)
            tmp[i] = s->buf[(s->head + i) % SPARK_LEN];
        memcpy(s->buf, tmp, sizeof(tmp));
        s->head = 0;
    }
}

/* ════════════════════════════════════════════════════════════════════
   SCREEN 1 — VOLTAGE
   ────────────────────────────────────────────────────────────────────
   Layout:
     [0..33]   Top bar: "VOLTAGE"  + dots
     [35..38]  1px accent line (already in top bar)
     [40..170] Arc gauge 240° sweep, value centred
     [172..192] Sparkline history
     [194..206] Frequency badge
     [206..240] Bottom bar: MIN / MAX / AVG
   ════════════════════════════════════════════════════════════════════ */

static void Screen_Voltage_Update(const EnergyData_t *d) {
    /* Only repaint the value zone and sparkline to avoid flicker */
    ST7789_FillRect(30, 85, 180, 40,  DARKCYAN);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", d->voltage);
    ST7789_WriteStringCentered(88, buf, Font_16x26, CYAN, DARKCYAN);

    /* Frequency badge */
    snprintf(buf, sizeof(buf), "%.2fHz", d->frequency);
    ST7789_FillRect(72, 138, 96, 18,  PANEL);
    ST7789_WriteStringCentered(142, buf, Font_7x10, CYAN,  PANEL);

    /* Redraw arc */
    float pct = d->voltage / 260.0f;
    Draw_ArcGauge(120, 108, 62, pct,  CYAN, 0x014B, 8);

    /* Sparkline */
    Draw_Sparkline(&_spark_v, 5, 179, 230, 22,  CYAN);

    /* Bottom bar */
    char vs[8]; snprintf(vs, sizeof(vs), "%.1fV", d->v_min);
    char vx[8]; snprintf(vx, sizeof(vx), "%.1fV", d->v_max);
    Draw_BottomBar("MIN", vs, "NOW", buf + 4, "MAX", vx,  CYAN);
}
static void Screen_Voltage_Full(const EnergyData_t *d) {
    /* Dark blue-teal background */
    ST7789_FillColor(DARKCYAN);

    Draw_TopBar("VOLTAGE", CYAN);

    /* Outer decorative ring */
    ST7789_DrawCircle(120, 108, 75, DIM);
    ST7789_DrawCircle(120, 108, 46,  DIM);
    Screen_Voltage_Update(d);
}


/* ════════════════════════════════════════════════════════════════════
   SCREEN 2 — CURRENT
   ────────────────────────────────────────────────────────────────────
   Layout:
     [0..33]   Top bar: "CURRENT" + dots
     [40..170] Double arc gauge (outer = range, inner = decorative)
     [172..192] Sparkline
     [194..206] RMS badge
     [206..240] Bottom bar: MIN / MAX / RMS
   ════════════════════════════════════════════════════════════════════ */

static void Screen_Current_Update(const EnergyData_t *d) {
    ST7789_FillRect(30, 85, 180, 40, DARKGREEN);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.3f", d->current);
    ST7789_WriteStringCentered(88, buf, Font_16x26, GREEN, DARKGREEN);

    float pct = d->current / 16.0f;
    Draw_ArcGauge(120, 108, 62, pct,  GREEN, 0x0144 , 8);
    Draw_ArcGauge(120, 108, 50, pct * 0.75f, 0x03E6, 0x00C1, 4);

    char rbuf[20];
    snprintf(rbuf, sizeof(rbuf), "RMS %.3fA", d->current);
    ST7789_FillRect(44, 138, 152, 18,  PANEL);
    ST7789_WriteStringCentered(142, rbuf, Font_7x10, GREEN,  PANEL);

    Draw_Sparkline(&_spark_i, 5, 179, 230, 22,  GREEN);

    char imin[8]; snprintf(imin, sizeof(imin), "%.2fA", d->i_min);
    char imax[8]; snprintf(imax, sizeof(imax), "%.2fA", d->i_max);
    Draw_BottomBar("MIN", imin, "RMS", rbuf + 4, "MAX", imax,  GREEN);
}
static void Screen_Current_Full(const EnergyData_t *d) {
    ST7789_FillColor(DARKGREEN);
    Draw_TopBar("CURRENT",  GREEN);

    ST7789_DrawCircle(120, 108, 75,  DIM);
    ST7789_DrawCircle(120, 108, 36,  DIM);
    /* Outer arc: 0..16A range, thick=8 */
    Screen_Current_Update(d);
}


/* ════════════════════════════════════════════════════════════════════
   SCREEN 3 — POWER
   ────────────────────────────────────────────────────────────────────
   Layout:
     [0..33]   Top bar: "POWER" + dots
     [40..165] Arc gauge for active power (0..5000W)
               Inner arc shows reactive portion (Q/S)
     [148..165] Power factor badge
     [166..188] Sparkline
     [188..206] P / Q / S info boxes
     [206..240] Bottom bar: ACTIVE / REACT / APPAR
   ════════════════════════════════════════════════════════════════════ */
static void Screen_Power_Update(const EnergyData_t *d) {
    /* Repaint value zone only */
    ST7789_FillRect(30, 80, 180, 40, DARKAMBER);

    /* Main value */
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", d->active_power);
    ST7789_WriteStringCentered(83, buf, Font_16x26, AMBER, DARKAMBER);

    /* Redraw both arcs */
    float pct_p = d->active_power / 5000.0f;
    Draw_ArcGauge(120, 108, 62, pct_p, AMBER,  0x2100, 8);
    Draw_ArcGauge(120, 108, 50, pct_p, ORANGE, 0x0C10, 4);

    /* Sparkline */
    Draw_Sparkline(&_spark_p, 5, 179, 230, 20, AMBER);

    /* Info box + bottom bar */
    char pmin[12], pmax[12];
    snprintf(pmin, sizeof(pmin), "%.0fW", d->p_min);
    snprintf(pmax, sizeof(pmax), "%.0fW", d->p_max);
    //Draw_InfoBoxPower(2, 178, 236, 26, "ACTIVE", pb, AMBER);
    Draw_BottomBar("MIN", pmin, "NOW", buf + 4, "MAX", pmax, AMBER);
}

static void Screen_Power_Full(const EnergyData_t *d) {
    ST7789_FillColor(DARKAMBER);
    Draw_TopBar("POWER", AMBER);

    ST7789_DrawCircle(120, 108, 75,  DIM);
    ST7789_DrawCircle(120, 108, 36,  DIM);
    Screen_Power_Update(d);
}

/* ════════════════════════════════════════════════════════════════════
   SCREEN 4 — ENERGY
   ────────────────────────────────────────────────────────────────────
   Layout:
     [0..33]   Top bar: "ENERGY" + dots
     [38..100] kWh value — large, centred in pill box
     [104..120] Wh sub-value
     [124..136] Horizontal progress bar (0..10 kWh)
     [138..146] Bar scale labels
     [150..176] Session time + current rate info boxes
     [178..206] Sparkline (energy growth curve)
     [206..240] Bottom: TODAY / MONTH / AVG POWER
   ════════════════════════════════════════════════════════════════════ */
static void Screen_Energy_Update(const EnergyData_t *d) {
    /* Repaint pill + value only */
    ST7789_FillRect(10, 38, 220, 68,  PANEL);
    ST7789_DrawHLine(10, 38,  220, PURPLE);
    ST7789_DrawHLine(10, 105, 220, PURPLE);

    float kwh = d->energy_wh / 1000.0f;
    char kbuf[16];
    snprintf(kbuf, sizeof(kbuf), "%.3f", kwh);
    ST7789_WriteStringCentered(48, kbuf, Font_16x26, PURPLE, PANEL);
    ST7789_WriteStringCentered(82, "kWh", Font_11x18, DIM,  PANEL);

    char whbuf[20];
    snprintf(whbuf, sizeof(whbuf), "= %.1f Wh", d->energy_wh);
    ST7789_FillRect(0, 106, 240, 12,  DARKPURPLE);
    ST7789_WriteStringCentered(108, whbuf, Font_7x10, DIM,  DARKPURPLE);

    /* Progress bar */
    float bar_pct = kwh / 10.0f;
    if (bar_pct > 1.0f) bar_pct = 1.0f;
    ST7789_FillRect(10, 120, 220, 10,  PANEL);
    Draw_ProgressBar(10, 120, 220, 10, bar_pct, PURPLE, 0x0925);

    /* Large Wh box + RATE box */
    char rbuf[12];  snprintf(rbuf,  sizeof(rbuf),  "%.0f W",  d->active_power);
    Draw_InfoBox(  5, 146, 112, 30, "CONSUMED", whbuf + 8, PURPLE);
    Draw_InfoBox(123, 146, 112, 30, "RATE",     rbuf,  ORANGE);

    Draw_Sparkline(&_spark_e, 5, 179, 230, 24, PURPLE);

    /* Bottom bar */
    char eb[10]; snprintf(eb, sizeof(eb), "%.2f kWh", kwh);
    char pb[10]; snprintf(pb, sizeof(pb), "%.0f W",   d->active_power);
    Draw_BottomBar("SESSION", eb, "RATE", pb, "Wh", whbuf + 10, PURPLE);
}
static void Screen_Energy_Full(const EnergyData_t *d) {
    ST7789_FillColor(DARKPURPLE);
    Draw_TopBar("ENERGY", PURPLE);

    Screen_Energy_Update(d);
    /* Scale labels */
    ST7789_WriteString(10, 132, "0", Font_7x10, DIM,  DARKPURPLE);
    ST7789_WriteString(193, 132, "10kWh", Font_7x10, DIM,  DARKPURPLE);
}

/* ════════════════════════════════════════════════════════════════════
   PUBLIC API
   ════════════════════════════════════════════════════════════════════ */
void Screens_Init(void) {
    memset(&_spark_v, 0, sizeof(_spark_v));
    memset(&_spark_i, 0, sizeof(_spark_i));
    memset(&_spark_p, 0, sizeof(_spark_p));
    memset(&_spark_e, 0, sizeof(_spark_e));
    _state      = SCREEN_VOLTAGE;
    _force_full = 1;
}

void Screens_NextState(void) {
    _state = (ScreenState_t)(((int)_state + 1) % SCREEN_COUNT);
    _force_full = 1;   /* force full redraw on transition */
}

ScreenState_t Screens_GetState(void) {
    return _state;
}

void Screens_UpdateStats(EnergyData_t *d) {
    if (d->voltage  < d->v_min) d->v_min = d->voltage;
    if (d->voltage  > d->v_max) d->v_max = d->voltage;
    if (d->current  < d->i_min) d->i_min = d->current;
    if (d->current  > d->i_max) d->i_max = d->current;
    if (d->active_power < d->p_min) d->p_min = d->active_power;
    if (d->active_power > d->p_max) d->p_max = d->active_power;
}

void Screens_Update(const EnergyData_t *d) {
    /* Push into the relevant sparkline regardless of current screen */
    Spark_Push(&_spark_v, d->voltage);
    Spark_Push(&_spark_i, d->current);
    Spark_Push(&_spark_p, d->active_power);
    Spark_Push(&_spark_e, d->energy_wh);

    if (_force_full) {
        /* Full repaint */
        switch (_state) {
            case SCREEN_VOLTAGE: Screen_Voltage_Full(d); break;
            case SCREEN_CURRENT: Screen_Current_Full(d); break;
            case SCREEN_POWER:   Screen_Power_Full(d);   break;
            case SCREEN_ENERGY:  Screen_Energy_Full(d);  break;
            default: break;
        }
        _force_full = 0;
    } else {
        /* Partial update — only dynamic zones */
        switch (_state) {
            case SCREEN_VOLTAGE: Screen_Voltage_Update(d); break;
            case SCREEN_CURRENT: Screen_Current_Update(d); break;
            case SCREEN_POWER:   Screen_Power_Update(d);   break;
            case SCREEN_ENERGY:  Screen_Energy_Update(d);  break;
            default: break;
        }
    }
}
