/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's screen: RIVER. The river at night, carrying frasil.
 *   pitch is altitude above the waterline; a voice is a current carrying ice crystals, as long and as dense as it
 *   is loud; pressure warms its crystals to lamp light; FREEZE sets them into a lattice and stills the water; the
 *   reverb (SPACE) is the haze off the water; a glint is a glint in the sky, always at the same place for a note;
 *   the bass is the river's bed, a slow swell the river's width at its note's height.
 * The drone's three layers own the river's three things: CHOIR the current, PARTIALS the crystals, BREATH the
 * water's haze around it; their mix shows there, and exactly, as three strata under the waterline on the play page.
 * Three bands, each drawn on the canvas and blitted: the header (0 .. 31), the river (32 .. 175, 20 frames a
 * second), the footer (176 .. 239: the three hands, or the page's second row). The pages' parameters sit in a 2 x 2
 * grid across the river's lower half and the footer; on the play page (SOUND) and for the three hands, a turn shows
 * the value large over the river for a moment instead.
 * Part of the unity build (app.c includes it): gfx.c's canvas (cv_px, byte-swapped RGB565) is in scope. */

#define C_BG RGB(10, 13, 18)
#define C_STEEL RGB(38, 48, 60)          /* structure: gauge tracks */
#define C_SLATE RGB(74, 85, 99)          /* quiet marks, the waterline */
#define C_MIST RGB(139, 149, 164)        /* labels */
#define C_FOG RGB(220, 227, 234)         /* values, the voices */
#define C_LAMP RGB(242, 166, 90)         /* warm: the hand (pressure, what is being turned, what changed) */
#define C_ICE RGB(110, 208, 255)         /* cold: the ice (PARTIALS' crystals, GLINT, FREEZE, a choice made) */
#define C_HAZE RGB(51, 65, 79)
#define C_SINK RGB(10, 100, 176)         /* SINK's water, a clear blue over the river (C_BG's red) */

#define ML 14                            /* margins */
#define MR 226
#define R1 0                             /* the bands' first rows */
#define R2 32
#define R3 176
#define HZ 175                           /* the waterline */
#define HZ_HOME 116                      /* the play page's: the strata below it */
#define TOUCH_MS 900u                    /* a turned value stays large this long */

#include "frasil_logo.h"                 /* LOGO_W x LOGO_H, 4-bit (tools/gen_logo.py) */

static int32_t oy;                       /* the band being drawn: its first screen row */

static struct {
    uint32_t field_at, frames, fps_at, fps;
    uint32_t sig1, sig3;                 /* what the header and the footer showed last */
    uint32_t frz_at;                     /* when the freeze last changed (the crystals ease to it) */
    uint8_t frz;
    uint8_t full;
    uint8_t was_logo, was_map, was_pop;
    uint8_t acts;                        /* the words SCENES' box showed last: 4, 2 on NEW SHARD (the tests) */
    uint32_t help_sig;
    float stillt;                        /* the river's time when it froze */
    float lt, gph, wph;                  /* the logo's last frame's time; how far its granules and its water have gone */
    int32_t edge[240];                   /* the haze fades in at the sides: 0 .. 256 */
    float sn[256];                       /* a sine a turn */
} ui;

static uint32_t strlen_(const char *s)
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}

/* ------------------------------------------------------------ drawing --- */
static inline uint16_t mix565(uint16_t bg, uint16_t c, int32_t a)   /* a of 32, rounded (no green cast) */
{
    uint32_t r = c >> 11, g = (c >> 5) & 63u, b = c & 31u, br = bg >> 11, bgg = (bg >> 5) & 63u, bb = bg & 31u;
    uint32_t ia = 32u - (uint32_t)a;
    return (uint16_t)(((r * (uint32_t)a + br * ia + 16u) >> 5) << 11 | ((g * (uint32_t)a + bgg * ia + 16u) >> 5) << 5 |
                      ((b * (uint32_t)a + bb * ia + 16u) >> 5));
}

static inline void bl(int32_t x, int32_t y, uint16_t c, int32_t a)   /* screen coordinates, a of 32 */
{
    uint16_t *p;
    y -= oy;
    if (a <= 0 || (uint32_t)x >= cv_w || (uint32_t)y >= cv_h)
        return;
    p = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    *p = swap16(a >= 32 ? c : mix565(swap16(*p), c, a));
}

static void band(int32_t y0, int32_t h)
{
    oy = y0;
    cv_begin(240, (uint32_t)h, C_BG);
}

static void hline(int32_t x0, int32_t x1, int32_t y, uint16_t c) { cv_rect(x0, y - oy, x1 - x0 + 1, 1, c); }

static int32_t text_tw(const felucca_font_t *f, const char *s, int32_t tr)   /* width, tracked */
{
    int32_t n = (int32_t)strlen_(s);
    return text_w(f, s) + (n > 1 ? tr * (n - 1) : 0);
}

static int32_t text_t(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c, int32_t tr)
{
    char ch[2] = {0, 0};
    for (; *s; s++) {
        ch[0] = *s;
        x = cv_text(x, y - oy, f, ch, c) + tr;
    }
    return x - tr;
}

/* a page's title, top left: white, bold (drawn twice a pixel apart), tracked; returns its end */
static int32_t title(const char *s, int32_t tr)
{
    text_t(ML, 8, &FONT_B, s, C_WHITE, tr + 1);
    return text_t(ML + 1, 8, &FONT_B, s, C_WHITE, tr + 1) + 1;
}

static void text_r(int32_t xr, int32_t y, const felucca_font_t *f, const char *s, uint16_t c, int32_t tr)
{
    text_t(xr - text_tw(f, s, tr), y, f, s, c, tr);
}

static void text_c(int32_t xc, int32_t y, const felucca_font_t *f, const char *s, uint16_t c, int32_t tr)
{
    text_t(xc - text_tw(f, s, tr) / 2, y, f, s, c, tr);
}

/* a name in the room it has (w), as large as it fits: FONT_B tracked tr, then closer, then FONT_S, then FONT_XS; cut
 * last. bold: drawn twice, a pixel apart (a title's). Returns its end */
static int32_t fit_text(int32_t x, int32_t y, int32_t w, const char *s, uint16_t c, int bold, int32_t tr)
{
    static const felucca_font_t *const F[3] = {&FONT_B, &FONT_S, &FONT_XS};
    char t[24];
    int32_t k, r = 0, n;
    const felucca_font_t *f = 0;
    for (k = 0; k < 3 && !f; k++)
        for (r = k ? 1 : tr; r >= 0; r--)
            if (text_tw(F[k], s, r) + bold <= w) {
                f = F[k];
                break;
            }
    if (!f) {                                /* (still too wide: FONT_XS, as much as fits) */
        f = &FONT_XS;
        r = 0;
    }
    for (n = 0; s[n] && n < 23; n++)
        t[n] = s[n];
    t[n] = 0;
    while (n > 0 && text_tw(f, t, r) + bold > w)
        t[--n] = 0;
    y += f == &FONT_XS;
    if (bold)
        text_t(x, y, f, t, c, r);
    return text_t(x + bold, y, f, t, c, r) + bold;
}

static void gauge(int32_t x, int32_t y, int32_t w, float v, uint16_t fill)
{
    int32_t n = (int32_t)(v * (float)w + 0.5f);
    n = clamp(n, 1, w);
    hline(x, x + w - 1, y, C_STEEL);
    hline(x, x + n - 1, y, fill);
    cv_rect(x + n - 1, y - 2 - oy, 1, 5, C_FOG);
}

/* the control a value is under, before its name, a filled disc: K1 .. K4 ice with the number, the three left
 * encoders amber with a letter (A SELECT, B PRESETS, C ALGORITHM). Centred on (cx, cy); 14 px across (12 in the
 * strata) */
#define BADGE_W 19                       /* the badge and the gap after it */
static void badge_r(int32_t cx, int32_t cy, char ch, int knob, float r)
{
    int32_t x, y, w, n = (int32_t)r + 2;
    char t[2] = {ch, 0};
    for (y = -n; y <= n; y++)
        for (x = -n; x <= n; x++) {
            float c = r + 0.5f - fm_sqrtf((float)(x * x + y * y));
            c = c < 0.0f ? 0.0f : c > 1.0f ? 1.0f : c;
            bl(cx + x, cy + y, knob ? C_ICE : C_LAMP, (int32_t)(32.0f * c));
        }
    {                                    /* the character, drawn twice a pixel apart: bolder; a small disc's smaller,
                                          * inside it */
        const felucca_font_t *f = r < 6.0f ? &FONT_T : &FONT_XS;
        int32_t dy = r < 6.0f ? 6 : 8;
        w = text_w(f, t);
        cv_text(cx - w / 2, cy - dy - oy, f, t, C_BG);
        cv_text(cx - w / 2 + 1, cy - dy - oy, f, t, C_BG);
    }
}

static void badge(int32_t cx, int32_t cy, char ch, int knob) { badge_r(cx, cy, ch, knob, 6.5f); }

/* the badge of parameter p on this page (or of encoder enc, the one that turned it): its K slot, or its letter if it
 * is one of the three hands (SELECT A, PRESETS B, ALGORITHM C) and not on this page's K1 .. K4 (TEMPO on GLOBAL) */
static void badge_of(int32_t cx, int32_t cy, int p, int enc)
{
    static const char HAND[3] = {[EN_SELECT] = 'A', [EN_ALGO] = 'C', [EN_PRESET] = 'B'};
    uint32_t i;
    if (enc >= 0 && enc < EN_K1) {
        badge(cx, cy, HAND[enc], 0);
        return;
    }
    if (enc >= EN_K1) {
        badge(cx, cy, (char)('1' + enc - EN_K1), 1);
        return;
    }
    for (i = 0; i < 4u; i++)
        if (PAGES[st.page].p[i] == p) {
            badge(cx, cy, (char)('1' + i), 1);
            return;
        }
    for (i = 0; i < 3u; i++)
        if (MACRO_OF[i] == p) {
            badge(cx, cy, HAND[i], 0);
            return;
        }
}

/* a value as the screen writes it, the number large and its unit small; the large size steps down if too wide */
static void value_big(int32_t x, int32_t y, int p, int32_t maxw, const felucca_font_t *f, uint16_t c, int align)
{
    char t[24], *u = 0;
    int32_t w, uw = 0, i;
    const felucca_font_t *fv = f;
    value_text(p, st.v[p], t);
    for (i = 0; t[i]; i++)               /* a unit after the number: S, MS, HZ, CT */
        if (t[i] == ' ' && i > 0 && t[i - 1] >= '0' && t[i - 1] <= '9' && t[i + 1] && (t[i + 2] == 0 || t[i + 3] == 0 ||
                                                                                    t[i + 4] == 0))
            u = &t[i + 1];
    if (u)
        u[-1] = 0;
    w = text_w(fv, t);
    if (u)
        uw = 4 + text_w(&FONT_XS, u);
    if (w + uw > maxw) {
        fv = &FONT_B;
        w = text_w(fv, t);
    }
    if (align == 1)
        x -= (w + uw) / 2;
    else if (align == 2)
        x -= w + uw;
    x = cv_text(x, y - oy, fv, t, c);
    if (u)
        cv_text(x + 4, y + (int32_t)fv->h - (int32_t)FONT_XS.h - 1 - oy, &FONT_XS, u, C_MIST);
}

/* ------------------------------------------------------------ the river --- */
static uint32_t hsh(uint32_t i, uint32_t k)   /* a repeatable pseudo-random, 0 .. 65535 */
{
    uint32_t v = i * 374761393u + k * 668265263u;
    v = (v ^ (v >> 13)) * 1274126177u;
    return (v ^ (v >> 16)) & 0xFFFFu;
}

static inline float sn(float turns)       /* sin(2 pi turns), from the table */
{
    int32_t i = (int32_t)(turns * 256.0f);
    return ui.sn[(uint32_t)i & 255u];
}

static inline float br_clamp01(float t) { return t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t; }

static inline float smooth01(float t)
{
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    return t * t * (3.0f - 2.0f * t);
}

static float note_y(int n, int32_t top, int32_t bottom)    /* C1 on the waterline, D6 at the top */
{
    float y = (float)bottom - (float)(n - 24) * (float)(bottom - top) * (1.0f / 62.0f);
    return y < (float)top + 6.0f ? (float)top + 6.0f : y > (float)bottom - 4.0f ? (float)bottom - 4.0f : y;
}

static void haze(int32_t top, int32_t bottom, float dim)   /* SPACE: how high and how dense; stratus rows */
{
    float space = (float)st.v[P_ESPACE] / 127.0f, blur = ((float)st.v[P_GX_BLUR] + mod[P_GX_BLUR]) / 127.0f;
    int32_t hh, x, y;
    if (blur > 0.0f && space < 0.9f)     /* BLUR: the drone into the cloud, the haze as SPACE would have it */
        space += (0.9f - space) * (blur > 1.0f ? 1.0f : blur);
    hh = (int32_t)((24.0f + 46.0f * space) * (float)(bottom - top) / 143.0f);
    for (y = bottom - hh; y < bottom; y++) {
        float tr = (float)(y - (bottom - hh)) / (float)hh;
        int32_t a = (int32_t)(256.0f * 0.55f * 32.0f * space * dim * tr * tr * ((y & 1) ? 1.0f : 0.6f) *
                              (st.v[P_FREEZE] ? 1.8f : 1.0f));
        for (x = ML; x <= MR; x++) {
            if (st.v[P_FREEZE] && ((y % 3) || (x & 1)))
                continue;
            bl(x, y, C_HAZE, (a * ui.edge[x]) >> 16);
        }
    }
}

/* one crystal of frasil: a grain, a needle along the flow, a disc catching light; set ice is a small cross */
static void crystal(int32_t x, int32_t y, uint16_t col, float b, float kind, int set, int link)
{
    if (set) {
        bl(x, y, col, (int32_t)(32.0f * (b * 1.3f > 1.0f ? 1.0f : b * 1.3f)));
        bl(x + 1, y, col, (int32_t)(16.0f * b));
        bl(x - 1, y, col, (int32_t)(16.0f * b));
        bl(x, y + 1, col, (int32_t)(16.0f * b));
        bl(x, y - 1, col, (int32_t)(16.0f * b));
        if (link) {                      /* a hairline to the next */
            bl(x + 2, y, col, (int32_t)(8.0f * b));
            bl(x + 3, y, col, (int32_t)(8.0f * b));
            bl(x + 4, y, col, (int32_t)(8.0f * b));
        }
    } else if (kind < 0.08f) {
        bl(x, y, col, (int32_t)(32.0f * (b * 1.2f > 1.0f ? 1.0f : b * 1.2f)));
        bl(x + 1, y, col, (int32_t)(13.0f * b));
        bl(x - 1, y, col, (int32_t)(13.0f * b));
        bl(x, y + 1, col, (int32_t)(13.0f * b));
        bl(x, y - 1, col, (int32_t)(13.0f * b));
    } else if (kind < 0.40f) {
        bl(x, y, col, (int32_t)(32.0f * b));
        bl(x + 1, y, col, (int32_t)(22.0f * b));
        bl(x + 2, y, col, (int32_t)(10.0f * b));
    } else {
        bl(x, y, col, (int32_t)(29.0f * b));
    }
}

/* a voice: the water around it, the current, its crystals */
static void current(const frasil_view_t *w, int32_t top, int32_t bottom, float dim, float tt, float fz)
{
    float motion = (float)st.v[P_MOUVEMENT] / 127.0f, wob = 1.0f - fz;
    float lvl = w->level, prs = w->pressure > 1.0f ? 1.0f : w->pressure, yc = note_y(w->note, top, bottom);
    float half = 30.0f + lvl * 76.0f, th = 1.4f + lvl * 2.0f + prs * 2.6f, ph = (float)w->note * 0.37f;
    float mc = (float)st.v[P_CHOEUR] / 127.0f, mp = (float)st.v[P_PARTIELS] / 127.0f, mb = (float)st.v[P_SOUFFLE] / 127.0f;
    float drift = tt * (0.004f + 0.03f * motion);
    int32_t xc = 130, x0 = (int32_t)((float)xc - half), x1 = (int32_t)((float)xc + half);
    int32_t rr = (int32_t)(th * 2.4f) + 1, prof[40], nc, k, x;
    uint16_t col = mix565(C_FOG, C_LAMP, (int32_t)(prs * 32.0f));     /* the water and the current */
    uint16_t ice = mix565(fz > 0.5f ? C_ICE : mix565(C_ICE, C_FOG, 10), C_LAMP, (int32_t)(prs * 32.0f));   /* crystals */
    uint32_t seed = (uint32_t)w->note * 31u + 7u;
    if (rr > 19)
        rr = 19;
    for (k = -rr; k <= rr; k++) {        /* the halo's profile across the current (x 256 x 32) */
        float d = (float)k / th;
        prof[k + 20] = (int32_t)(lvl * fm_expf(-d * d) * dim * (0.10f + 0.14f * prs) * (0.15f + 1.1f * mb) * 32.0f * 256.0f);
    }
    if (x0 < 34)
        x0 = 34;
    if (x1 > MR)
        x1 = MR;
    for (x = x0; x <= x1; x++) {
        float e = 1.0f - smooth01(((float)(x > xc ? x - xc : xc - x) - (half - 34.0f)) * (1.0f / 34.0f));
        float yy = yc + wob * (sn((float)x * 0.0064f + ph + drift) * 1.1f +
                               sn((float)x * 0.0154f + 2.0f * ph + drift * 1.7f) * 0.45f);
        int32_t ei = (int32_t)(e * 256.0f), yi = (int32_t)yy, a;
        float f = yy - (float)yi, ca;
        if (ei <= 0)
            continue;
        for (k = -rr; k <= rr; k++)      /* the water around the current */
            bl(x, yi + k, col, (prof[k + 20] * ei) >> 16);
        ca = (0.22f + 0.25f * lvl) * e * dim * (1.0f + 1.2f * fz) * (0.25f + 0.95f * mc);   /* the current: thin, solid
                                                                                             * when set */
        a = (int32_t)(32.0f * (ca > 1.0f ? 1.0f : ca));
        bl(x, yi, col, (int32_t)((float)a * (1.0f - f)));
        bl(x, yi + 1, col, (int32_t)((float)a * f));
    }
    /* the frasil, downstream, faster with MOTION; FREEZE eases it into a lattice */
    nc = (int32_t)((18.0f + 56.0f * lvl + 30.0f * prs) * (dim > 0.5f ? 1.0f : 0.45f) * mp);
    for (k = 0; k < nc; k++) {
        float u = (float)hsh(seed, (uint32_t)k * 3u) * (1.0f / 65536.0f);
        float spread = 0.55f + 0.45f * (float)hsh(seed, (uint32_t)k * 3u + 7u) * (1.0f / 65536.0f);
        float speed = 0.6f + 0.8f * (float)hsh(seed, (uint32_t)k * 5u + 3u) * (1.0f / 65536.0f);
        float p = u + tt * (0.01f + 0.05f * motion) * speed, g, xf, yf, b;
        int32_t cx, cy;
        p -= (float)(int32_t)p;
        xf = (float)xc + (p - 0.5f) * 2.0f * (half - 6.0f) * spread;
        g = ((float)hsh(seed, (uint32_t)k * 3u + 1u) + (float)hsh(seed, (uint32_t)k * 3u + 2u)) * (1.0f / 65536.0f) -
            1.0f;
        yf = yc + g * th * 2.2f + wob * sn(xf * 0.0064f + ph + drift) * 1.1f;
        if (fz > 0.0f) {                 /* the lattice: every 6 px, on the current and 3 px either side */
            float lx = 36.0f + (float)(((int32_t)xf - 36) / 6 * 6) + (k % 3 == 1 ? 3.0f : 0.0f);
            float ly = yc + (float)(k % 3 == 0 ? 0 : k % 3 == 1 ? -3 : 3);
            xf += (lx - xf) * fz;
            yf += (ly - yf) * fz;
        }
        cx = (int32_t)(xf + 0.5f);
        cy = (int32_t)(yf + 0.5f);
        if (cx < 36 || cx > MR)
            continue;
        b = (1.0f - smooth01(((float)(cx > xc ? cx - xc : xc - cx) - (half - 34.0f)) * (1.0f / 34.0f))) *
            (0.45f + 0.55f * (float)hsh(seed, (uint32_t)k * 5u + 11u) * (1.0f / 65536.0f)) * dim;
        if (b > 0.05f)
            crystal(cx, cy, ice, b, (float)hsh(seed, (uint32_t)k * 7u + 5u) * (1.0f / 65536.0f), fz > 0.5f,
                    k % 3 == 0);
    }
}

static void glints(int32_t top, int32_t bottom, float dim)   /* a glint in the sky, rising a little as it fades */
{
    glint_view_t sp[GLINT_NV];
    uint32_t i;
    int32_t j;
    frasil_glint_view(sp);
    for (i = 0; i < GLINT_NV; i++) {
        float l = sp[i].level, rise = (float)sp[i].age * 0.08f;
        int32_t gx, gy, arm, a;
        float lv;
        if (sp[i].note < 0 || l < 0.004f)
            continue;
        lv = br_clamp01(1.0f + fm_log2f(l) * (1.0f / 8.0f));   /* as heard: 48 dB from full to gone */
        gx = 44 + (int32_t)((uint32_t)(sp[i].note * 37) % 23u) * 8 +   /* a note's own place; VIBRATO sways it */
             (int32_t)(sp[i].cents * 0.25f);
        {                                /* in the sky above the river: higher notes higher */
            float u = br_clamp01(((float)sp[i].note - 60.0f) * (1.0f / 48.0f));
            gy = top + 10 + (int32_t)((1.0f - u) * (float)(bottom - top) * 0.32f) - (int32_t)(rise > 8.0f ? 8.0f : rise);
        }
        if (gy < top + 4)
            gy = top + 4;
        a = (int32_t)(32.0f * lv);
        arm = 1 + (int32_t)(lv * 8.0f);
        for (j = 1; j <= arm; j++) {     /* four arms of ice, fading out, around a white core */
            int32_t aj = a * (arm + 1 - j) / (arm + 1);
            bl(gx + j, gy, C_ICE, aj);
            bl(gx - j, gy, C_ICE, aj);
            bl(gx, gy + j, C_ICE, aj * 3 / 4);
            bl(gx, gy - j, C_ICE, aj * 3 / 4);
        }
        bl(gx, gy, C_WHITE, a);
        for (j = 0; j < 6; j++) {        /* a few motes around it */
            uint32_t h = hsh((uint32_t)sp[i].note, (uint32_t)j);
            bl(gx + (int32_t)(h & 15u) - 8, gy + (int32_t)((h >> 4) & 15u) - 8, C_ICE, (a * (int32_t)(h >> 12)) >> 5);
        }
    }
}

/* the bass: a slow swell across the river at its note's height, the water under it a little darker down to the
 * bed; pressure warms it; FREEZE stills it */
static void bass_bed(int32_t top, int32_t bottom, float dim, int labels, float tt, float fz)
{
    bass_view_t b;
    float lvl, prs, yc, amp, a0;
    int32_t x, y, depth;
    uint16_t col;
    frasil_bass_view(&b);
    if (b.note < 0 || b.level < 0.01f)
        return;
    lvl = b.level;
    prs = b.pressure > 1.0f ? 1.0f : b.pressure;
    yc = note_y(b.note, top, bottom);
    amp = (1.5f + 2.5f * lvl) * (1.0f - fz);
    a0 = (0.40f + 0.50f * lvl) * dim;
    col = mix565(mix565(C_SLATE, C_FOG, 18), C_LAMP, (int32_t)(prs * 32.0f));
    depth = (int32_t)(bottom - yc);
    for (x = ML; x <= MR; x++) {
        float e = (float)ui.edge[x] * (1.0f / 256.0f);
        float yy = yc + amp * (sn((float)x * 0.0042f + tt * 0.021f) * 0.8f + sn((float)x * 0.0097f - tt * 0.013f) * 0.2f);
        int32_t yi = (int32_t)yy, a = (int32_t)(32.0f * a0 * e);
        float f = yy - (float)yi;
        bl(x, yi, col, (int32_t)((float)a * (1.0f - f)));
        bl(x, yi + 1, col, a);
        bl(x, yi + 2, col, (int32_t)((float)a * f));
        for (y = yi + 3; y < bottom; y++) {   /* the water under it, down to the bed */
            int32_t d = y - yi - 3, af = (int32_t)(8.0f * a0 * e * (1.0f - (float)d / (float)(depth + 4)));
            if (af > 0 && ((x + y) & 1))
                bl(x, y, C_HAZE, af);
        }
    }
    if (labels) {                        /* its note, on the right: the voices' are on the left */
        static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        char n[8];
        n[0] = 0;
        text_cat(n, NN[b.note % 12]);
        int_text(b.note / 12 - 1, n + strlen_(n));
        text_r(MR, (int32_t)yc - 13, &FONT_XS, n, mix565(C_BG, prs > 0.1f ? C_LAMP : b.gate ? C_MIST : C_SLATE,
                                                        (int32_t)(32.0f * dim)), 0);
    }
}

/* a pixel toward c by a (0 .. 1), each channel rounded on a fixed 4 x 4 pattern: a gradient as smooth as 565 can make
 * it, without bands, and still where nothing changes (screen coordinates) */
static void blf(int32_t x, int32_t y, uint16_t c, float a)
{
    static const uint8_t B4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    float th = ((float)B4[(y & 3) * 4 + (x & 3)] + 0.5f) * (1.0f / 16.0f);
    uint32_t q;
    uint16_t *p;
    y -= oy;
    if (a <= 0.0f || (uint32_t)x >= cv_w || (uint32_t)y >= cv_h)
        return;
    p = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    q = swap16(*p);
    *p = swap16((uint16_t)((uint32_t)((float)(q >> 11) + ((float)(c >> 11) - (float)(q >> 11)) * a + th) << 11 |
                           (uint32_t)((float)(q >> 5 & 63u) + ((float)(c >> 5 & 63u) - (float)(q >> 5 & 63u)) * a + th) << 5 |
                           (uint32_t)((float)(q & 31u) + ((float)(c & 31u) - (float)(q & 31u)) * a + th)));
}

/* SINK: the river under water as it is turned, a clear blue veil from the top down, densest at the top, fading out
 * where it reaches and at the sides as the river does; as faint as the river is (a page's strip, the value over it).
 * Its red the background's: over the dark it moves green and blue alone (565's coarse red would put mauve in it) */
static void sink(int32_t top, int32_t bottom, float dim)
{
    float s = ((float)st.v[P_GX_SINK] + mod[P_GX_SINK]) / 127.0f, line, fade;
    int32_t h = bottom - top, y, x;
    if (s <= 0.01f)
        return;
    s = s > 1.0f ? 1.0f : s;
    line = (float)top + s * 1.25f * (float)h;
    fade = 0.6f * (float)h;
    for (y = top; y < bottom && (float)y < line; y++) {
        float a = 0.34f * dim * smooth01((line - (float)y) / fade) * smooth01((float)(y - top + 1) * (1.0f / 6.0f));
        for (x = ML; x <= MR; x++)
            blf(x, y, C_SINK, a * (float)ui.edge[x] * (1.0f / 256.0f));
    }
}

static void river(int32_t top, int32_t bottom, float dim, int labels, float t)
{
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    frasil_view_t v[FR_NV];
    float fz, tt;
    int32_t order[FR_NV], nv = 0, i, j, last_label = -100;
    if (ui.frz != st.v[P_FREEZE]) {
        ui.frz = st.v[P_FREEZE];
        ui.frz_at = plat_ms();
        ui.stillt = t;
    }
    fz = smooth01((float)(plat_ms() - ui.frz_at) * (1.0f / 300.0f));   /* 1: set (eased over 300 ms) */
    if (!st.v[P_FREEZE])
        fz = 1.0f - fz;
    tt = st.v[P_FREEZE] ? ui.stillt : t;      /* the river stops when it is set */
    if (st.v[P_PUMP_RATE])               /* PUMP: the river breathes with the drones' duck */
        dim *= 0.45f + 0.55f * frasil_pump_gain();
    haze(top, bottom, dim);
    bass_bed(top, bottom, dim, labels, tt, fz);
    frasil_view(v);                      /* the voices, lowest first: their labels must not collide */
    for (i = 0; i < FR_NV; i++)
        if (v[i].note >= 0 && v[i].level > 0.01f)
            order[nv++] = i;
    for (i = 1; i < nv; i++)
        for (j = i; j > 0 && v[order[j]].note < v[order[j - 1]].note; j--) {
            int32_t k = order[j];
            order[j] = order[j - 1];
            order[j - 1] = k;
        }
    for (i = 0; i < nv; i++) {
        const frasil_view_t *w = &v[order[i]];
        current(w, top, bottom, dim, tt, fz);
        if (labels) {
            int32_t yc = (int32_t)note_y(w->note, top, bottom);
            if (last_label - yc >= 11 || last_label < 0) {
                char n[8];
                uint16_t lc = w->pressure > 0.1f ? C_LAMP : w->gate ? C_MIST : C_SLATE;
                n[0] = 0;
                text_cat(n, NN[w->note % 12]);
                int_text(w->note / 12 - 1, n + strlen_(n));
                cv_text(ML, yc - 7 - oy, &FONT_XS, n, mix565(C_BG, lc, (int32_t)(32.0f * dim)));
                last_label = yc;
            }
        }
    }
    glints(top, bottom, dim);
    sink(top, bottom, dim);
    hline(ML, MR, bottom, mix565(C_BG, st.v[P_FREEZE] ? C_ICE : C_SLATE, (int32_t)(32.0f * dim)));   /* the waterline */
}

/* the play page's corner, top right of the river: what goes on unseen, only what is on (GLINT's sequence or its
 * arpeggio, the bass's sequence, the LFOs running, SWELL) */
static void river_status(void)
{
    int32_t x = MR, y = R2 + 4, i;
    if (st.v[P_SWELL] || st.swell_btn > 0.0f) {
        text_r(x, y, &FONT_XS, "SWELL", C_LAMP, 1);
        x -= text_tw(&FONT_XS, "SWELL", 1) + 9;
    }
    if (lfo_any) {                       /* LFO, a dot for each: lit, the ones running */
        for (i = NLFO - 1; i >= 0; i--) {
            if (lfo[i].on)
                cv_rect(x - 3, y + 5 - oy, 3, 3, C_ICE);
            else
                cv_rect(x - 2, y + 6 - oy, 1, 1, C_SLATE);
            x -= 6;
        }
        text_r(x - 1, y, &FONT_XS, "LFO", C_MIST, 1);
        x -= text_tw(&FONT_XS, "LFO", 1) + 10;
    }
    if (st.v[P_BQ_ON]) {                 /* the bass's sequence, in lamp (slate, stopped) */
        const char *w = st.v[P_BQ_REC] ? "BASS REC" : "BASS SEQ";
        text_r(x, y, &FONT_XS, w, st.v[P_BQ_REC] || bq.run ? C_LAMP : C_SLATE, 1);
        x -= text_tw(&FONT_XS, w, 1) + 10;
    }
    if (st.v[P_SQ_TYPE] == SQT_SEQ) {
        const char *w = st.v[P_SQ_REC] ? "REC" : "SEQ";
        text_r(x, y, &FONT_XS, w, st.v[P_SQ_REC] ? C_LAMP : sq.run ? C_ICE : C_SLATE, 1);
    } else if (st.v[P_SQ_TYPE] == SQT_ARP && arp.n) {
        text_r(x, y, &FONT_XS, arp.latch ? "ARP LATCH" : "ARP", arp.paused ? C_SLATE : C_ICE, 1);
    }
}

/* ------------------------------------------------------------ the bands --- */
static int touched_now(void) { return st.touched_at && plat_ms() - st.touched_at < TOUCH_MS && st.touched < NPARAM; }

static int is_macro(int p) { return p == P_KB || p == P_DR_PRESET || p == P_DR_FULL; }   /* the three hands' */

static int is_layer(int p) { return p == P_CHOEUR || p == P_PARTIELS || p == P_SOUFFLE || p == P_CHALEUR; }

/* the large view of one value: on the play page (but its strata show the three layers), and for the three hands
 * anywhere */
static int overlay_on(void)
{
    return touched_now() && ((st.page == PG_SON && !is_layer(st.touched)) ||
                             (is_macro(st.touched) && st.touched_enc < EN_K1));   /* (BODY on DRONE PARAMS' K4:
                                                                                 * its cell) */
}

/* the play page's mix: under the waterline, each layer a stratum drawn as its thing in the river (the current, the
 * crystals, the haze), as long as it is loud; the one being turned lit */
static void strata(float dim)
{
    static const uint8_t L[4] = {P_CHOEUR, P_PARTIELS, P_SOUFFLE, P_CHALEUR};
    const int32_t x0 = 80, len = 124;
    uint32_t i;
    int32_t x, k;
    for (i = 0; i < 4u; i++) {
        int p = L[i], hot = touched_now() && st.touched == p;
        int32_t y = HZ_HOME + 10 + (int32_t)i * 14, n = (int32_t)st.v[p] * len / 127;   /* (the badges apart) */
        uint16_t c = mix565(C_BG, i == 1u ? C_ICE : hot ? C_FOG : C_MIST, (int32_t)((hot || i == 1u ? 32.0f : 28.0f) * dim));
        uint16_t lc = mix565(C_BG, hot ? C_LAMP : C_MIST, (int32_t)(24.0f * dim));
        char t[8];
        badge_r(ML + 6, y, (char)('1' + i), 1, 5.0f);
        text_t(ML + BADGE_W - 2, y - 6, &FONT_T, PD[p].name, lc, 1);
        for (x = x0; x < x0 + len; x += 3)   /* the full length, dotted */
            bl(x, y, C_STEEL, 32);
        if (i == 0) {                    /* CHOIR: the current, a solid line */
            for (x = x0; x < x0 + n; x++) {
                bl(x, y, c, 32);
                bl(x, y - 1, c, 12);
            }
        } else if (i == 1) {             /* PARTIALS: crystals, needles along the flow */
            for (x = x0; x + 2 < x0 + n; x += 4) {
                int32_t jy = (int32_t)(hsh((uint32_t)x, 3u) % 3u) - 1;
                crystal(x, y + jy, c, 0.95f, hsh((uint32_t)x, 9u) < 5000u ? 0.0f : 0.3f, 0, 0);
            }
        } else if (i == 2) {             /* BREATH: haze, stratus rows */
            static const int32_t PROF[5] = {6, 14, 22, 14, 6};
            for (x = x0; x < x0 + n; x++) {
                int32_t e = x > x0 + n - 10 ? (x0 + n - x) * 26 : 256;
                for (k = -2; k <= 2; k++)
                    bl(x, y + k, c, (PROF[k + 2] * e) >> 8);
            }
        } else {                         /* WARMTH: embers, a low glow with sparks along it */
            uint16_t wc = mix565(C_BG, hot ? C_LAMP : RGB(214, 150, 96), (int32_t)(28.0f * dim));
            for (x = x0; x < x0 + n; x++) {
                bl(x, y, wc, 8 + (int32_t)(hsh((uint32_t)x, 5u) & 7u));
                if (hsh((uint32_t)x, 11u) % 6u == 0) {
                    bl(x, y, wc, 32);
                    bl(x, y - 1, wc, 12);
                }
            }
        }
        int_text((int32_t)st.v[p] * 100 / 127, t);
        text_r(MR, y - 6, &FONT_T, t, lc, 0);
    }
}

/* the keys' badge, top right: a small keyboard, what the keys play and the octave, in the mode's colour (DRONE white,
 * GLINT ice, BASS lamp), in a rounded frame: solid while the notes latch (the drones and the bass on GLOBAL's KEYS
 * LATCH; GLINT's arpeggio on and latched), dotted while they sound only as long as they are held */
/* what the keys do, for the badge: DRONE, BASS, or GLINT's: GLINT (each key its glint, GLINT SEQ's pages' RECORD OFF
 * too), ARP (into the arpeggio), SEQ (they transpose the sequence), REC (they write into it, in lamp); BASS's sequence
 * on, SEQ and REC the same */
static const char *keys_word(uint16_t *col)
{
    *col = st.glint ? C_ICE : st.bass ? C_LAMP : C_FOG;
    if (st.bass && st.v[P_BQ_ON])
        return st.v[P_BQ_REC] ? "REC" : "SEQ";
    if (!st.glint)
        return st.bass ? "BASS" : "DRONE";
    if (seq_keys()) {
        if (st.v[P_SQ_REC])
            *col = C_LAMP;
        return st.v[P_SQ_REC] ? "REC" : "SEQ";
    }
    return st.v[P_SQ_TYPE] == SQT_ARP ? "ARP" : "GLINT";
}

static int32_t keys_badge_w(char *oct)       /* its width; the octave as it writes it */
{
    uint16_t col;
    const char *word = keys_word(&col);
    oct[0] = 0;
    if (st.octave > 0)
        text_cat(oct, "+");
    int_text(st.octave, oct + strlen_(oct));
    return 4 + 17 + 4 + text_tw(&FONT_XS, word, 1) + 8 + text_tw(&FONT_XS, oct, 0) + 5;
}

static void keys_badge(int32_t x0, int32_t w, const char *oct)
{
    uint16_t col;
    const char *word = keys_word(&col);
    int latched = st.glint ? (st.v[P_SQ_TYPE] == SQT_ARP && arp.latch) : !st.v[P_KEYS] && !(st.bass && st.v[P_BQ_ON]);
    const int32_t y0 = 6, y1 = 24, r = 4, x1 = x0 + w - 1;
    int32_t x, y, k, xw;
    for (y = y0 + 1; y < y1; y++)            /* a breath of its colour inside */
        for (x = x0 + 1; x < x1; x++)
            bl(x, y, col, 2);
    for (k = 0, x = x0 + r; x <= x1 - r; x++, k++)   /* the frame: straight runs, dotted two on two off */
        if (latched || !(k & 2)) {
            bl(x, y0, col, 24);
            bl(x, y1, col, 24);
        }
    for (k = 0, y = y0 + r; y <= y1 - r; y++, k++)
        if (latched || !(k & 2)) {
            bl(x0, y, col, 24);
            bl(x1, y, col, 24);
        }
    for (y = 0; y < r; y++)                  /* its corners, quarter circles */
        for (x = 0; x < r; x++) {
            float d = fm_sqrtf((float)((r - x) * (r - x) + (r - y) * (r - y))) - (float)r, a = 1.0f - fm_fabsf(d);
            int32_t ai;
            if (a <= 0.0f || (!latched && (x + y) & 1))
                continue;
            ai = (int32_t)(24.0f * a);
            bl(x0 + x, y0 + y, col, ai);
            bl(x1 - x, y0 + y, col, ai);
            bl(x0 + x, y1 - y, col, ai);
            bl(x1 - x, y1 - y, col, ai);
        }
    {                                        /* the keyboard: four white keys, three pixels each, two black */
        int32_t kx = x0 + 4, ky = 11;
        for (x = 0; x <= 16; x++) {
            bl(kx + x, ky, col, 28);
            bl(kx + x, ky + 8, col, 28);
        }
        for (k = 0; k <= 16; k += 4)
            for (y = 1; y < 8; y++)
                bl(kx + k, ky + y, col, k == 0 || k == 16 ? 28 : 16);
        for (k = 4; k <= 8; k += 4)
            for (y = 1; y < 6; y++) {
                bl(kx + k - 1, ky + y, col, 32);
                bl(kx + k, ky + y, col, 32);
                bl(kx + k + 1, ky + y, col, 32);
            }
    }
    xw = text_t(x0 + 4 + 17 + 4, 9, &FONT_XS, word, col, 1);
    for (y = y0 + 5; y <= y1 - 5; y++)       /* a hairline, then the octave */
        bl(xw + 4, y, col, 10);
    text_t(xw + 8, 9, &FONT_XS, oct, mix565(C_BG, col, 22), 0);
}

/* where the page is among its button's: a dot each under the title, the page's lit (HOME: SOUND, MIXER, the logo) */
static void page_dots(void)
{
    uint32_t p, n = 0, i = 0, k;
    for (p = 0; p < NPAGES; p++)
        if (PAGES[p].button == PAGES[st.page].button && !page_hidden(p)) {   /* (GLINT's by its SEQ) */
            n++;
            if (p <= st.page)
                i = n;
        }
    if (PAGES[st.page].button == B_HOME) {   /* (its third, the logo) */
        n = 3;
        i = st.page == PG_MIXER ? 2u : 1u;
    }
    if (n < 2u)
        return;
    for (k = 1; k <= n; k++) {
        int32_t x = ML + 1 + (int32_t)(k - 1u) * 6;
        if (k == i)
            cv_rect(x, 26 - oy, 3, 3, C_ICE);
        else
            cv_rect(x + 1, 27 - oy, 1, 1, C_SLATE);
    }
}

/* a line of progress under the title (a MORPH, PLAY held), in lamp */
static void head_progress(float k)
{
    int32_t w = MR - ML, n = (int32_t)((k < 0.0f ? 0.0f : k > 1.0f ? 1.0f : k) * (float)w);
    hline(ML, MR, 27, C_STEEL);
    if (n > 0)
        hline(ML, ML + n - 1, 27, C_LAMP);
}

static int32_t title_at(int32_t x, const char *s, uint16_t c, int32_t tr)   /* title()'s, from x, in c */
{
    text_t(x, 8, &FONT_B, s, c, tr + 1);
    return text_t(x + 1, 8, &FONT_B, s, c, tr + 1) + 1;
}

static void header(void)
{
    char t[40], oct[8];
    uint32_t now = plat_ms();
    int32_t xt, bw = keys_badge_w(oct), bx = MR - bw + 1, xr = bx - 6;
    float ph = play_holding();
    band(R1, 32);
    if (frasil_shed_level() > 0) {       /* the processor lightening the sound: an amber dot by the badge */
        cv_rect(bx - 7, 13 - oy, 3, 3, C_LAMP);
        xr -= 8;
    }
    if (ph > 0.0f) {                     /* PLAY held: what it is about to do, counting down */
        title_at(ML, holding_any() || !rst_valid ? "RELEASE ALL" : "RESTORE", C_LAMP, 0);
        head_progress(ph);
        keys_badge(bx, bw, oct);
        cv_blit(0, R1);
        return;
    }
    if (now < say.msg_until) {           /* a message for a moment (the badge stays, if there is room) */
        int32_t x = text_t(ML, 8, &FONT_B, say.msg[0], C_FOG, 1);
        if (say.msg[1][0])
            x = text_t(x + 7, 9, &FONT_XS, say.msg[1], C_MIST, 1);
        if (x + 8 <= bx)
            keys_badge(bx, bw, oct);
        cv_blit(0, R1);
        return;
    }
    if (mo.on && (st.page == PG_SON || st.page == PG_SCENES)) {   /* a MORPH: an arrow, the scene it goes to, how
                                                                    * far (elsewhere: the progress under the page) */
        int32_t i;
        for (i = 0; i < 9; i++)
            bl(ML + i, 16, C_LAMP, 32);
        for (i = 1; i <= 3; i++) {
            bl(ML + 8 - i, 16 - i, C_LAMP, 32);
            bl(ML + 8 - i, 16 + i, C_LAMP, 32);
        }
        title_at(ML + 15, mo.name, C_WHITE, 1);
        head_progress((float)(now - mo.t0) / (float)mo.dur);
        keys_badge(bx, bw, oct);
        cv_blit(0, R1);
        return;
    }
    if (st.compare) {                    /* EDIT held: the scene as it was saved */
        int32_t x = title_at(ML, "COMPARE", C_LAMP, 0);
        text_t(x + 8, 9, &FONT_XS, "AS SAVED", C_MIST, 1);
        keys_badge(bx, bw, oct);
        cv_blit(0, R1);
        return;
    }
    /* the title first, the keys' badge at the right; between them, by priority while they fit: on GLOBAL the load,
     * FREEZE; last, beside the title, the key; under the title, where the page is among its button's */
    if (st.page == PG_SON) {             /* the scene: its name, as large as it fits (sixteen characters), a lamp dot
                                          * while the sound differs; FREEZE's tag if there is room left (else a flake) */
        scene_t sc;
        char nt[SC_NAME + 1];
        int32_t room = xr - ML - (st.dirty ? 8 : 0), wf = text_tw(&FONT_XS, "FREEZE", 1) + 8 + 6;
        scene_get(st.scene, &sc);
        name_text(&sc, nt);
        if (st.v[P_FREEZE])
            room -= text_tw(&FONT_B, nt, 2) + 1 + wf <= room ? wf : 13;
        xt = fit_text(ML, 8, room, nt, C_WHITE, 1, 2);
        if (st.dirty) {
            cv_rect(xt + 4, 12 - oy, 3, 3, C_LAMP);
            xt += 8;
        }
    } else if (st.page == PG_SCENES) {
        xt = title("SCENES", 2);
        if (st.dirty && xt + 8 + text_tw(&FONT_XS, "CHANGED", 1) <= xr)
            xt = text_t(xt + 8, 9, &FONT_XS, "CHANGED", C_LAMP, 1);
    } else {
        xt = title(PAGES[st.page].name, 0);
    }
    page_dots();
    if (mo.on) {
        int32_t w = (int32_t)((float)(now - mo.t0) / (float)mo.dur * (float)(MR - ML));
        hline(ML, MR, 30, C_STEEL);
        if (w > 0)
            hline(ML, ML + (w > MR - ML ? MR - ML : w), 30, C_LAMP);
    }
    keys_badge(bx, bw, oct);
    t[0] = 0;
    if (st.page == PG_GLOBAL) {          /* the load, for the first tests on a device */
        uint32_t n;
        text_cat(t, "CPU ");
        int_text((int32_t)plat_cpu_pct(), t + strlen_(t));
        text_cat(t, "%");
        n = strlen_(t);
        text_cat(t, "  ");
        int_text((int32_t)ui.fps, t + strlen_(t));
        text_cat(t, " FPS");
        if (xr - text_tw(&FONT_XS, t, 0) < xt + 30)
            t[n] = 0;                    /* short of room: the load alone */
        if (xr - text_tw(&FONT_XS, t, 0) >= xt + 8) {
            text_r(xr, 9, &FONT_XS, t, C_MIST, 0);
            xr -= text_tw(&FONT_XS, t, 0) + 6;
        }
    }
    if (st.v[P_FREEZE]) {                /* FREEZE: an ice tag */
        int32_t wf = text_tw(&FONT_XS, "FREEZE", 1) + 8;
        if (xr - wf + 1 >= xt + 6) {
            cv_rect(xr - wf + 1, 7, wf, 16, C_ICE);
            text_t(xr - wf + 5, 9, &FONT_XS, "FREEZE", C_BG, 1);
            xr -= wf + 7;
        } else if (st.page == PG_SON) {  /* (a long name: a flake of ice) */
            int32_t k, cx = xr - 4;
            for (k = 1; k <= 4; k++) {
                int32_t dy = (k * 4 + 3) / 7, a = 32 - k * 4;
                bl(cx, 15 - k, C_ICE, a);
                bl(cx, 15 + k, C_ICE, a);
                if (k < 4) {
                    bl(cx + k, 15 - dy, C_ICE, a);
                    bl(cx - k, 15 + dy, C_ICE, a);
                    bl(cx + k, 15 + dy, C_ICE, a);
                    bl(cx - k, 15 - dy, C_ICE, a);
                }
            }
            bl(cx, 15, C_WHITE, 32);
            xr -= 13;
        }
    }
    t[0] = 0;
    if (st.page == PG_SON) {
        if (st.v[P_SCALE] || st.v[P_CHORD]) {   /* the harmony, quietly */
            text_cat(t, NOTE_NAME[st.v[P_ROOT] % 12u]);
            text_cat(t, " ");
            text_cat(t, SCALE_NAME[st.v[P_SCALE] % NSCALES]);
            if (xt + 12 + text_tw(&FONT_XS, t, 1) <= xr)
                text_t(xt + 12, 9, &FONT_XS, t, C_SLATE, 1);
        }
    }
    cv_blit(0, R1);
}

/* a choice among a few: a dot for each, the chosen one lit */
static void steps(int32_t x, int32_t y, int32_t w, uint32_t n, uint32_t i)
{
    uint32_t k;
    for (k = 0; k < n; k++) {
        int32_t px = x + (int32_t)(k * (uint32_t)(w - 2) / (n - 1u));
        if (k == i)
            cv_rect(px - 1, y - 1 - oy, 3, 3, C_ICE);
        else
            cv_rect(px, y - oy, 1, 1, C_SLATE);
    }
}

/* an LFO TARGET's colour: its track's, as the keys' badge has them (DRONE white, GLINT ice, BASS lamp), OFF slate */
static uint16_t tgt_col(uint32_t t)
{
    uint32_t p = LFO_TGT[t % NLFO_TGT];
    if (p >= NPARAM)
        return C_MIST;
    if (p == P_GL_LEVEL || p == P_GL_BRIGHT || p == P_GL_PING || p == P_GL_SPACE || p == P_MX_GLINT || p == P_ARP_CHANCE ||
        p == P_SQ_LEN || p == P_GX_SPARK)
        return C_ICE;
    if (p == P_BS_FILTER || p == P_BS_RESO || p == P_BS_DRIVE || p == P_MX_BASS)
        return C_LAMP;
    return C_FOG;
}

static void cell_w(int32_t x, int32_t y, int p, int32_t w);
static void cell(int32_t x, int32_t y, int p) { cell_w(x, y, p, 96); }

static void cell_w(int32_t x, int32_t y, int p, int32_t w)   /* a parameter: its name, its value, a gauge or its
                                                              * choices, w wide */
{
    int hot, f;
    if (p >= NPARAM)                     /* an empty slot (GLOBAL II) */
        return;
    hot = touched_now() && st.touched == p;
    f = PD[p].fmt;
    badge_of(x + 7, y + 7, p, -1);
    text_t(x + BADGE_W, y, &FONT_XS, PD[p].name, hot ? C_LAMP : C_MIST, 1);
    value_big(x, y + 15, p, w + 4, &FONT_M, f == F_LTGT ? tgt_col(st.v[p]) : C_FOG, 0);
    if (f == F_LTGT)                     /* (a TARGET: a name, in its track's colour, no scale) */
        return;
    if (PD[p].max <= 16u && f != F_PCT)
        steps(x + 1, y + 41, w, PD[p].max + 1u, st.v[p]);
    else
        gauge(x, y + 41, w, (float)st.v[p] / (float)PD[p].max, hot ? C_LAMP : C_MIST);
    if (p < NSOUND && mod[p] != 0.0f && PD[p].max > 16u) {   /* an LFO on it: where it is now, in ice */
        float u = br_clamp01(((float)st.v[p] + mod[p]) / (float)PD[p].max);
        int32_t n = clamp((int32_t)(u * (float)w + 0.5f), 1, w);
        cv_rect(x + n - 1, y + 41 - 3 - oy, 1, 7, C_ICE);
    }
}

/* a sequencer recording (STEP or LIVE), its strip ruled, a quiet help far under the notes. Across, x0 .. x1, a
 * hairline each whole tone of the strip's range, a note on one or between two (closer than 4 px, thinned to thirds,
 * tritones, octaves: each divides the octave, the Cs kept, a touch brighter) */
static void pitch_lines(int32_t x0, int32_t x1, int32_t top, int32_t bot, int32_t lo, int32_t hi)
{
    static const uint8_t K[4] = {2, 4, 6, 12};
    int32_t k = 0, n, x, y;
    while (k < 3 && K[k] * (bot - top) < 4 * (hi - lo))
        k++;
    for (n = lo; n <= hi; n++)
        if (n % K[k] == 0)
            for (y = bot - (n - lo) * (bot - top) / (hi - lo), x = x0; x <= x1; x++)
                bl(x, y, C_SLATE, n % 12 ? 4 : 7);
}

/* down, a step's edge, y0 .. y1 */
static void step_line(int32_t x, int32_t y0, int32_t y1)
{
    for (; y0 <= y1; y0++)
        bl(x, y0, C_SLATE, 4);
}

/* the step recorded into (STEP: K1's, where the next key writes; LIVE: the one playing), xa .. xb: a soft band of the
 * track's colour, a shaft of light fading out up and down, its edges a little brighter */
static void rec_band(int32_t xa, int32_t xb, int32_t y0, int32_t y1, uint16_t c)
{
    int32_t x, y, h = (y1 - y0) / 2;
    for (y = y0; y <= y1; y++) {
        int32_t d = y - y0 < y1 - y ? y - y0 : y1 - y, a2 = 2 + 10 * (d > h ? h : d) * (2 * h - d) / (h * h);
        for (x = xa; x <= xb; x++) {     /* (in half steps, the odd ones dithered: no bands) */
            int32_t a = (a2 >> 1) + ((a2 & 1) & (x ^ y));
            bl(x, y, c, x == xa || x == xb ? a + 3 : a);
        }
    }
}

/* GLINT's sequence, a constellation across the strip (16 steps wide while SEQ LENGTH is 16 or less, else 32): a star
 * a step, as high as its note (the steps' lowest to highest, at least an octave), larger with ACCENT; a tied step, a
 * line on from the last; a rest, a dot; past SEQ LENGTH, slate. The step playing, white, a line of light down through
 * it; the step on K1, an amber tick under it; RECORD on LIVE, an amber light in the corner. Recording, the strip
 * ruled, each step a column, the one recorded into lit in ice */
static void seq_scope(void)
{
    uint32_t len = seq_len(), i, cur = st.v[P_SQ_STEP] % SC_SEQ, ns = st.v[P_SQ_LEN] < 16u ? 16u : SC_SEQ;   /* (the
                                                                       * width from SEQ LENGTH as set; SEQ LENGTH as
                                                                       * it plays) */
    int32_t top = 40, bot = 80, lo = 127, hi = 0, py = -1, px = 0;
    int on = st.v[P_SQ_TYPE] == SQT_SEQ;
    for (i = 0; i < SC_SEQ; i++) {
        int32_t n = seq[i] & 127;
        if (n != (int32_t)SQ_REST && n != (int32_t)SQ_TIE) {
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    }
    if (hi < lo) {
        lo = 60;
        hi = 72;
    }
    if (hi - lo < 12) {                  /* at least an octave tall */
        int32_t m = (hi + lo) / 2;
        lo = m - 6;
        hi = m + 6;
    }
    if (on && st.v[P_SQ_REC]) {          /* recording: a column a step, the one recorded into lit */
        int32_t e[SC_SEQ + 1], r = st.v[P_SQ_REC] == 1u ? (int32_t)(cur % (st.v[P_SQ_LEN] + 1u)) : sq.run ? sq.pos : -1;
        for (i = 0; i <= ns; i++)        /* (its edges: halfway between two steps, the strip's ends) */
            e[i] = i == 0 ? ML : i == ns ? MR
                               : ML + 4 + (int32_t)(2u * i - 1u) * (MR - ML - 8) / (2 * (int32_t)(ns - 1u));
        if (r >= 0 && r < (int32_t)ns)
            rec_band(e[r], e[r + 1], top - 4, bot + 4, C_ICE);
        for (i = 0; i <= ns; i++)
            step_line(e[i], top - 4, bot + 4);
        pitch_lines(ML, MR, top, bot, lo, hi);
    }
    hline(ML, MR, bot + 5, C_STEEL);
    for (i = 0; i < ns; i++) {
        int32_t x = ML + 4 + (int32_t)i * (MR - ML - 8) / (int32_t)(ns - 1u), n = seq[i] & 127, y, j;
        int in = i < len, play = on && sq.run && sq.pos == (int32_t)i && sq.sounding >= 0;
        uint16_t col = in ? C_ICE : C_SLATE;
        if (on && sq.run && sq.pos == (int32_t)i)   /* the playhead */
            cv_rect(x, top - 4 - oy, 1, bot - top + 9, mix565(C_BG, C_ICE, 7));
        if (i == cur && st.page == PG_SEQ)          /* the step on K1 */
            cv_rect(x - 1, bot + 7 - oy, 3, 2, C_LAMP);
        if (n == (int32_t)SQ_REST) {
            bl(x, bot + 5, col, in ? 24 : 10);
            py = -1;
            continue;
        }
        if (n == (int32_t)SQ_TIE) {      /* on from the last note */
            if (py >= 0)
                for (j = px; j <= x; j++)
                    bl(j, py, col, in ? 18 : 8);
            px = x;
            continue;
        }
        y = bot - (n - lo) * (bot - top) / (hi - lo);
        {
            int32_t arm = seq[i] & SQ_ACC ? 5 : 3, a = play ? 32 : in ? 26 : 10;
            for (j = 1; j <= arm; j++) {
                int32_t aj = a * (arm + 1 - j) / (arm + 1);
                bl(x + j, y, col, aj);
                bl(x - j, y, col, aj);
                bl(x, y + j, col, aj * 3 / 4);
                bl(x, y - j, col, aj * 3 / 4);
            }
            bl(x, y, play ? C_WHITE : in ? C_FOG : C_SLATE, play ? 32 : a);
        }
        py = y;
        px = x;
    }
    if (st.v[P_SQ_REC] == 2u)            /* LIVE */
        for (i = 0; i < 9u; i++)
            bl(MR - 3 + (int32_t)(i % 3u) - 1, top - 3 + (int32_t)(i / 3u) - 1, C_LAMP, i == 4u ? 32 : 16);
}

/* the bass's sequence, a line of time across the strip: each step as wide as it is long (the STEPS it plays, and the
 * one on K1 past them, after a dotted end). A NOTE is a line at its note's height (the lowest to highest, at least an
 * octave), a mark where it starts, on through its HOLDs, the water under it down to the bed, as the bass's in the
 * river (bass_bed); a CUT, nothing; HOLDs at its start, the note its last steps leave sounding, round. In lamp, the
 * one playing bright; the playhead going along; the step on K1, a lamp line under it; RECORD on LIVE, a lamp light in
 * the corner, and the take's note down drawn bright as it is held, from where it went down to the playhead; SEQUENCE
 * off, the steps slate. Recording, the strip ruled, each step a column as wide, the one recorded into lit in lamp */
static void bseq_scope(void)
{
    uint32_t steps = st.v[P_BQ_STEPS] + 1u, cur = st.v[P_BQ_STEP] % SC_BSEQ, n = cur >= steps ? cur + 1u : steps;
    uint32_t i, total = 0, at = 0, loop = 0;
    int32_t top = 44, bot = 76, bed = 86, lo = 127, hi = 0, y = -1, play = 0, x0, x1, x, j;
    int on = st.v[P_BQ_ON], held = on && bq.tn >= 0 && bseq_playing();
    for (i = 0; i < n; i++) {
        int32_t b = bnote[i] & 127;
        total += bseq_ticks((int32_t)i);
        loop += i < steps ? bseq_ticks((int32_t)i) : 0u;
        if (b != (int32_t)SQ_REST && b != (int32_t)SQ_TIE) {
            lo = b < lo ? b : lo;
            hi = b > hi ? b : hi;
        }
    }
    if (held) {                          /* (the note held among them) */
        lo = bq.tn < lo ? bq.tn : lo;
        hi = bq.tn > hi ? bq.tn : hi;
    }
    if (hi < lo) {
        lo = 36;
        hi = 48;
    }
    if (hi - lo < 12) {                  /* at least an octave tall */
        int32_t m = (hi + lo) / 2;
        lo = m - 6;
        hi = m + 6;
    }
    if (st.v[P_BQ_REC]) {                /* recording: a column a step, the one recorded into lit */
        int32_t r = st.v[P_BQ_REC] == 1u ? (int32_t)(cur % steps) : bq.run ? bq.pos : -1;
        for (i = 0; i < n; i++) {
            x0 = ML + (int32_t)(at * (uint32_t)(MR - ML) / total);
            at += bseq_ticks((int32_t)i);
            x1 = ML + (int32_t)(at * (uint32_t)(MR - ML) / total);
            if (r == (int32_t)i)
                rec_band(x0, x1, top - 6, bed - 1, C_LAMP);
            step_line(x0, top - 6, bed - 1);
        }
        step_line(MR, top - 6, bed - 1);
        pitch_lines(ML, MR, top, bot, lo, hi);
        at = 0;
    }
    hline(ML, MR, bed, C_STEEL);         /* the bed */
    if ((bnote[0] & 127u) == SQ_TIE) {   /* HOLDs first: the note its last steps leave sounding, round its end */
        for (i = steps; i > 1u && (bnote[i - 1u] & 127u) == SQ_TIE; i--)
            ;
        if (i > 1u && (bnote[i - 1u] & 127u) != SQ_REST) {
            uint32_t e;
            for (e = 1; e < steps && (bnote[e] & 127u) == SQ_TIE; e++)
                ;
            y = bot - ((int32_t)(bnote[i - 1u] & 127u) - lo) * (bot - top) / (hi - lo);
            play = on && bq.run && bq.pos >= 0 && (uint32_t)bq.pos < e;
        }
    }
    for (i = 0; i < n; i++) {
        int32_t b = bnote[i] & 127, in = on && i < steps;
        uint16_t col;
        x0 = ML + (int32_t)(at * (uint32_t)(MR - ML) / total);
        at += bseq_ticks((int32_t)i);
        x1 = ML + (int32_t)(at * (uint32_t)(MR - ML) / total);
        if (i == steps && n > steps)     /* the end of what plays, dotted */
            for (j = top - 4; j <= bed; j += 2)
                bl(x0, j, C_SLATE, 24);
        bl(x0, bed - 1, C_SLATE, 32);   /* each step's start, a tick across the bed */
        bl(x0, bed + 1, C_SLATE, 32);
        if (b == (int32_t)SQ_REST) {
            y = -1;
            continue;
        }
        if (b != (int32_t)SQ_TIE) {      /* a NOTE: a line from here (HOLD carries it on), bright while it plays */
            uint32_t e;
            y = bot - (b - lo) * (bot - top) / (hi - lo);
            for (e = i + 1u; e < n && (bnote[e] & 127u) == SQ_TIE; e++)
                ;
            play = on && bq.run && bq.pos >= 0 && (uint32_t)bq.pos >= i && (uint32_t)bq.pos < e;
        }
        if (y < 0)                       /* (a HOLD after nothing: nothing) */
            continue;
        col = in ? C_LAMP : C_SLATE;
        for (x = x0 + (b != (int32_t)SQ_TIE); x < x1; x++) {   /* (a pixel apart from the one before) */
            int32_t a = play ? 32 : in ? 22 : 12, yy;
            bl(x, y, col, a);
            bl(x, y + 1, col, a * 2 / 5);
            for (yy = y + 2; yy < bed; yy++) {   /* the water under it, fading down to the bed */
                int32_t d = (bed - yy) * 16 / (bed - y);
                if ((x + yy) & 1)
                    bl(x, yy, play ? mix565(C_HAZE, C_LAMP, 8) : C_HAZE, (play ? 18 : 12) * d * d / 256);
            }
        }
        if (b != (int32_t)SQ_TIE)       /* where it starts */
            for (j = y - 2; j <= y + 3; j++)
                bl(x0 + 1, j, play ? C_WHITE : col, j == y ? 32 : play ? 20 : in ? 14 : 8);
    }
    if (held) {                          /* the take's note down, bright: from where it went down to the playhead
                                          * (round the loop's end; held a whole turn, all of it) */
        float a = bq.tp > 0.0f ? bq.tp : 0.0f, z = bq.tp + bq.te, w = (float)loop, s[4] = {a, z, 0.0f, 0.0f};
        uint32_t k;
        if (z - a >= w) {
            s[0] = 0.0f;
            s[1] = w;
        } else if (z > w) {
            s[1] = w;
            s[3] = z - w;
        }
        y = bot - (bq.tn - lo) * (bot - top) / (hi - lo);
        for (k = 0; k < 4u; k += 2u)
            for (x = ML + (int32_t)(s[k] * (float)(MR - ML) / (float)total);
                 x < ML + (int32_t)(s[k + 1u] * (float)(MR - ML) / (float)total); x++) {
                int32_t yy;
                bl(x, y, C_LAMP, 32);
                bl(x, y + 1, C_LAMP, 13);
                for (yy = y + 2; yy < bed; yy++) {
                    int32_t d = (bed - yy) * 16 / (bed - y);
                    if ((x + yy) & 1)
                        bl(x, yy, mix565(C_HAZE, C_LAMP, 8), 18 * d * d / 256);
                }
            }
        if (bq.tp >= 0.0f && z - a < w)  /* where it went down */
            for (j = y - 2; j <= y + 3; j++)
                bl(ML + (int32_t)(a * (float)(MR - ML) / (float)total) + 1, j, C_WHITE, j == y ? 32 : 20);
    }
    at = 0;
    for (i = 0; i < n; i++) {            /* the playhead; the step on K1 */
        uint32_t w = bseq_ticks((int32_t)i);
        x0 = ML + (int32_t)(at * (uint32_t)(MR - ML) / total);
        x1 = ML + (int32_t)((at + w) * (uint32_t)(MR - ML) / total);
        if (on && bq.run && bq.pos == (int32_t)i) {
            float f = bq.t / (float)w;
            x = x0 + (int32_t)((f > 1.0f ? 1.0f : f) * (float)(x1 - x0));
            cv_rect(x, top - 6 - oy, 1, bed - top + 6, mix565(C_BG, C_LAMP, 9));
        }
        if (i == cur && st.page == PG_BSEQ)
            cv_rect(x0 + 1, bed + 4 - oy, x1 - x0 > 2 ? x1 - x0 - 1 : 2, 2, C_LAMP);
        at += w;
    }
    if (st.v[P_BQ_REC] == 2u)            /* LIVE */
        for (i = 0; i < 9u; i++)
            bl(MR - 3 + (int32_t)(i % 3u) - 1, top - 5 + (int32_t)(i / 3u) - 1, C_LAMP, i == 4u ? 32 : 16);
}

/* BASS ADSR's and GLINT ADSR's strip: the envelope as one draws it, up to the top over ATTACK, down to SUSTAIN's level
 * over DECAY, held there while the key is (SUSTAIN), down to nothing over RELEASE once let go. Each part as wide as its
 * knob is turned (SUSTAIN's what is left), its time across it, curved as a charge and a discharge are; the part being
 * turned lit. Each note sounding (the bass's one, GLINT's up to four) a glint riding it: how far along from the
 * engine's times, let go from where it was then */
#define ADSR_TOP 42
#define ADSR_BED 82
#define ADSR_HOLD 1.5f                   /* seconds held: a note's glint across SUSTAIN's plateau, then waiting there */
typedef struct {
    float ta, td, tr, sus;               /* the times (s, the engine's: glint.c, bass.c); SUSTAIN's level */
    int32_t x[5];                        /* where each part begins, and the end */
} adsr_t;

static float adsr_fall(float u) { return (fm_expf(-3.0f * u) - 0.049787068f) * (1.0f / 0.950212932f); }   /* 1 .. 0 */

static float adsr_level(const adsr_t *A, int part, float u)   /* the level u (0..1) across a part */
{
    u = br_clamp01(u);
    return part == 0 ? 1.0f - adsr_fall(u) * adsr_fall(u)   /* (up quickly, easing into the top) */
         : part == 1 ? A->sus + (1.0f - A->sus) * adsr_fall(u)
         : part == 2 ? A->sus : A->sus * adsr_fall(u);
}

static int32_t adsr_y(float g) { return ADSR_BED - (int32_t)(br_clamp01(g) * (float)(ADSR_BED - ADSR_TOP) + 0.5f); }

static int32_t adsr_at(const adsr_t *A, int part, float u)   /* x, u (0..1) across a part */
{
    return A->x[part] + (int32_t)(br_clamp01(u) * (float)(A->x[part + 1] - A->x[part]) + 0.5f);
}

/* a held note's place: ta seconds into its ATTACK, then td into DECAY (and on, SUSTAIN's) -> its part and how far */
static int adsr_held(const adsr_t *A, int attacking, float t, float *u)
{
    if (attacking) {
        *u = t / A->ta;
        return 0;
    }
    if (t < A->td) {
        *u = t / A->td;
        return 1;
    }
    *u = (t - A->td) / ADSR_HOLD;
    return 2;
}

static void adsr_dot(int32_t x, float g, uint16_t col)   /* a note on it: a glow, its arms, a white core */
{
    int32_t y = adsr_y(g), dx, dy, j;
    for (dy = -3; dy <= 3; dy++)
        for (dx = -3; dx <= 3; dx++) {
            int32_t r2 = dx * dx + dy * dy;
            if (r2 <= 10)
                bl(x + dx, y + dy, col, 16 * (11 - r2) / 11);
        }
    for (j = 2; j <= 6; j++) {
        int32_t a = 24 * (7 - j) / 6;
        bl(x + j, y, col, a);
        bl(x - j, y, col, a);
        bl(x, y + j, col, a * 3 / 4);
        bl(x, y - j, col, a * 3 / 4);
    }
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            bl(x + dx, y + dy, C_WHITE, dx || dy ? 20 : 32);
}

/* a note's glint: held (attacking: in its ATTACK; t seconds in it, else from DECAY's start), or let go toff seconds
 * ago from there */
static void adsr_note(const adsr_t *A, int held, int attacking, float t, float toff, uint16_t col)
{
    float u, g;
    int part = adsr_held(A, attacking, t, &u);
    g = adsr_level(A, part, u);
    if (held)
        adsr_dot(adsr_at(A, part, u), g, col);
    else
        adsr_dot(adsr_at(A, 3, toff / A->tr), g * adsr_fall(toff / A->tr), col);
}

static void adsr_scope(int glint)
{
    static const char *const PART[4] = {"A", "D", "S", "R"};
    const uint8_t *P = PAGES[st.page].p;
    uint16_t col = glint ? C_ICE : C_LAMP;
    adsr_t A;
    int32_t x, py = 0, k, lit = -1, w[4];
    float a = (float)st.v[P[0]] / 127.0f, d = (float)st.v[P[1]] / 127.0f, r = (float)st.v[P[3]] / 127.0f;
    A.sus = (float)st.v[P[2]] / 127.0f;
    A.ta = glint ? 0.002f * fm_powf(750.0f, a) : 0.005f * fm_powf(1000.0f, a);
    A.td = glint ? 0.08f * fm_powf(75.0f, d) : 0.01f * fm_powf(1000.0f, d);
    A.tr = glint ? 0.02f * fm_powf(300.0f, r) : 0.03f * fm_powf(500.0f, r);
    w[0] = 10 + (int32_t)(44.0f * a + 0.5f);
    w[1] = 10 + (int32_t)(44.0f * d + 0.5f);
    w[3] = 10 + (int32_t)(44.0f * r + 0.5f);
    w[2] = MR - ML - w[0] - w[1] - w[3];
    A.x[0] = ML;
    for (k = 0; k < 4; k++)
        A.x[k + 1] = A.x[k] + w[k];
    if (touched_now())
        for (k = 0; k < 4; k++)
            if (P[k] == st.touched)
                lit = k;
    hline(ML, MR, ADSR_BED, C_STEEL);    /* the bed: silence */
    for (k = 0; k < 4; k++) {            /* each part: its curve, the water under it, its letter */
        for (x = A.x[k]; x < A.x[k + 1] + (k == 3); x++) {
            int32_t y = adsr_y(adsr_level(&A, k, (float)(x - A.x[k]) / (float)(A.x[k + 1] - A.x[k]))), yy;
            int32_t y0 = x > ML && py < y ? py : y, y1 = x > ML && py > y ? py : y;
            for (yy = y0; yy <= y1; yy++) {   /* (joined to the last), a soft row under it */
                bl(x, yy, col, k == lit ? 32 : 22);
                bl(x, yy + 1, col, k == lit ? 12 : 7);
            }
            for (yy = y + 2; yy < ADSR_BED; yy++) {
                int32_t f = (ADSR_BED - yy) * 16 / (ADSR_BED - y);
                if ((x + yy) & 1)
                    bl(x, yy, k == lit ? mix565(C_HAZE, col, 10) : C_HAZE, (k == lit ? 20 : 12) * f * f / 256);
            }
            py = y;
        }
        text_c((A.x[k] + A.x[k + 1]) / 2, ADSR_BED + 4, &FONT_T, PART[k], k == lit ? col : C_SLATE, 0);
    }
    for (k = 1; k < 4; k++) {            /* where one part gives way to the next, a tick across the bed */
        bl(A.x[k], ADSR_BED - 1, C_SLATE, 32);
        bl(A.x[k], ADSR_BED + 1, C_SLATE, 32);
    }
    for (x = adsr_y(A.sus) + 3; x < ADSR_BED - 1; x += 2)   /* let go here: dotted down from the plateau's end */
        bl(A.x[3], x, C_SLATE, 20);
    if (glint) {                         /* each glint sounding (its onset and its DECAY from the strike: DECAY's
                                          * part once ATTACK's time is gone by) */
        glint_view_t v[GLINT_NV];
        uint32_t i;
        frasil_glint_view(v);
        for (i = 0; i < GLINT_NV; i++) {
            float t = v[i].gate ? v[i].t : v[i].t - v[i].toff;
            if (v[i].note >= 0 && v[i].env >= 0.01f)
                adsr_note(&A, v[i].gate, t < A.ta, t < A.ta ? t : t - A.ta, v[i].toff, col);
        }
    } else {                             /* the bass's note */
        bass_view_t b;
        frasil_bass_view(&b);
        if (b.note >= 0 && b.level * b.level >= 0.005f)
            adsr_note(&A, b.stage < 2u, b.stage < 2u ? b.stage == 0u : b.let_stage == 0u, b.stage < 2u ? b.t : b.let_t,
                      b.t, col);
    }
}

/* an LFO's pages: its shape over a turn (four for RANDOM and DRIFT: the three before and this one) across the strip,
 * as deep as DEPTH (a little even at 0, to be seen), from where POLARITY puts it (the middle, the bottom, the top);
 * where it is now, a glint; slate while it is off */
static void lfo_scope(uint32_t i)
{
    const uint8_t *L = &st.v[LFO_BASE[i]];
    uint32_t s = L[0] % 7u, pol = L[3] % 3u;
    int rnd = s >= 5u, on = lfo[i].on;
    float d = 0.2f + 0.8f * (float)L[2] / 127.0f, turns = rnd ? 4.0f : 1.0f, now;
    int32_t top = 40, bot = 80, x, py = 0, w = MR - ML;
    uint16_t col = on ? C_ICE : C_SLATE;
    int32_t base = pol == 1u ? bot : pol == 2u ? top : (top + bot) / 2;
    hline(ML, MR, base, C_STEEL);
    for (x = 0; x <= w; x++) {
        float u = (float)x / (float)w * turns, sv, y;
        uint32_t c = rnd ? lfo[i].cyc + (uint32_t)u - 3u : 0u;
        int32_t yi, y0, y1, k;
        sv = lfo_shape(s, rnd ? u - (float)(int32_t)u : (u >= 1.0f ? 0.9999f : u), i, c);
        y = pol == 1u ? (float)bot - 0.5f * (sv + 1.0f) * (float)(bot - top) * d
          : pol == 2u ? (float)top + 0.5f * (sv + 1.0f) * (float)(bot - top) * d
                      : (float)base - sv * 0.5f * (float)(bot - top) * d;
        yi = (int32_t)(y + 0.5f);
        y0 = x ? (py < yi ? py : yi) : yi;
        y1 = x ? (py > yi ? py : yi) : yi;
        for (k = y0; k <= y1; k++) {     /* (joined to the last: a square's edges), a soft second row under it */
            bl(ML + x, k, col, on ? 24 : 12);
            bl(ML + x, k + 1, col, on ? 7 : 4);
        }
        py = yi;
    }
    now = rnd ? (3.0f + lfo[i].ph) / 4.0f : lfo[i].ph;
    {
        int32_t gx = ML + (int32_t)(now * (float)w + 0.5f), gy, j;
        float sv = lfo[i].out;
        gy = (int32_t)((pol == 1u ? (float)bot - 0.5f * (sv + 1.0f) * (float)(bot - top) * d
                      : pol == 2u ? (float)top + 0.5f * (sv + 1.0f) * (float)(bot - top) * d
                                  : (float)base - sv * 0.5f * (float)(bot - top) * d) + 0.5f);
        cv_rect(gx, top - 4 - oy, 1, bot - top + 8, mix565(C_BG, col, 6));
        int32_t dx, dy;
        for (dy = -3; dy <= 3; dy++)     /* a glow, */
            for (dx = -3; dx <= 3; dx++) {
                int32_t r2 = dx * dx + dy * dy;
                if (r2 <= 10)
                    bl(gx + dx, gy + dy, col, (on ? 16 : 8) * (11 - r2) / 11);
            }
        for (j = 2; j <= 7; j++) {       /* four arms, */
            int32_t a = (on ? 26 : 12) * (8 - j) / 7;
            bl(gx + j, gy, col, a);
            bl(gx - j, gy, col, a);
            bl(gx, gy + j, col, a * 3 / 4);
            bl(gx, gy - j, col, a * 3 / 4);
        }
        for (dy = -1; dy <= 1; dy++)     /* a white core */
            for (dx = -1; dx <= 1; dx++)
                bl(gx + dx, gy + dy, on ? C_WHITE : C_MIST, dx || dy ? 20 : 32);
    }
}

/* MIXER: a fader for each track, its level (100: as it is, a mark; 127 at the top), and beside it the track's meter,
 * live (48 dB). Each in its colour: DRONE white, BASS lamp, GLINT ice, CLOUD the haze lit */
#define MIX_TOP 44
#define MIX_BOT 166
static int32_t mix_x(uint32_t i) { return ML + 26 + (int32_t)i * 53; }
static uint16_t mix_col(uint32_t i)
{
    return i == 0 ? C_FOG : i == 1u ? C_LAMP : i == 2u ? C_ICE : mix565(C_MIST, C_ICE, 12);
}

static void mixer(void)
{
    float m[4];
    uint32_t i;
    frasil_meters(m);
    for (i = 0; i < 4u; i++) {
        int p = PAGES[PG_MIXER].p[i], hot = touched_now() && st.touched == p;
        int32_t x = mix_x(i), h = MIX_BOT - MIX_TOP, y, dx, yl = MIX_BOT - (int32_t)st.v[p] * h / 127;
        int32_t yu = MIX_BOT - 100 * h / 127, ym;
        uint16_t c = mix_col(i);
        float db = 6.0206f * fm_log2f(m[i] + 1e-6f), f = br_clamp01((db + 48.0f) * (1.0f / 48.0f));
        for (y = MIX_TOP; y <= MIX_BOT; y += 2)   /* the track */
            bl(x, y, C_STEEL, 32);
        for (dx = -7; dx <= 7; dx++)              /* the unity mark */
            bl(x + dx, yu, C_SLATE, dx & 1 ? 0 : 28);
        for (y = yl + 2; y <= MIX_BOT; y++)       /* the level, a column of its colour */
            for (dx = -5; dx <= 5; dx++)
                bl(x + dx, y, c, hot ? 11 : 6);
        cv_rect(x - 11, yl - 1 - oy, 23, 3, hot ? C_LAMP : c);   /* the fader's cap */
        ym = MIX_BOT - (int32_t)(f * (float)h);   /* the meter */
        for (y = ym; y <= MIX_BOT; y++) {
            bl(x + 15, y, c, 30);
            bl(x + 16, y, c, 30);
        }
    }
}

static void mixer_footer(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int p = PAGES[PG_MIXER].p[i], hot = touched_now() && st.touched == p;
        int32_t x = mix_x(i), w = 15 + text_tw(&FONT_XS, PD[p].name, 0), x0 = x - w / 2;
        badge(x0 + 6, 189, (char)('1' + i), 1);
        text_t(x0 + 15, 182, &FONT_XS, PD[p].name, hot ? C_LAMP : C_MIST, 0);
        value_big(x, 200, p, 50, &FONT_M, hot ? C_LAMP : mix_col(i), 1);
    }
}

/* MASTERING: the compressor's gain reduction, live, a bar from the right (20 dB across) */
static void gr_meter(void)
{
    float gr = st.v[P_MS_RATIO] ? -frasil_comp_gr() : 0.0f;
    int32_t w = MR - ML + 1, n = (int32_t)(br_clamp01(gr * (1.0f / 20.0f)) * (float)w), k;
    char s[16];
    text_t(ML, 42, &FONT_XS, "REDUCTION", C_MIST, 1);
    s[0] = 0;
    if (!st.v[P_MS_RATIO]) {
        text_cat(s, "OFF");
    } else {
        int32_t d = (int32_t)(gr * 10.0f + 0.5f);
        text_cat(s, d ? "-" : "");
        int_text(d / 10, s + strlen_(s));
        text_cat(s, ".");
        int_text(d % 10, s + strlen_(s));
        text_cat(s, " DB");
    }
    text_r(MR, 42, &FONT_XS, s, gr > 0.5f ? C_LAMP : C_MIST, 0);
    hline(ML, MR, 70, C_STEEL);
    for (k = 0; k <= 4; k++)                 /* a tick every 5 dB */
        cv_rect(MR - k * (w - 1) / 4, 66 - oy, 1, 9, C_SLATE);
    if (n > 0)
        cv_rect(MR - n + 1, 64 - oy, n, 13, C_LAMP);
}

/* SCENES: five lines around the one K1 points at: NEW SHARD first, then the sixteen slots, each its number, its name
 * (as large as it fits), what it holds (what it plays, FACTORY, EMPTY: left out for a long name); the one K1 points at
 * framed in ice, the one loaded marked NOW. NEW SHARD is no slot, a sound to start from: a plus for its number, START
 * FRESH for what it holds, framed in lamp. Under them, apart in a box of their own, the page's two actions: PLAY loads,
 * SAVE and REC store (on NEW SHARD, PLAY LOAD alone) */
#define SC_ROW 22
#define SC_BOX 150                        /* the actions' box: its top */
static void scenes_list(void)
{
    int32_t first = (st.pick >= NSCENES ? 0 : (int32_t)st.pick + 1) - 2, i;   /* (the lines: NEW SHARD's 0, a slot's
                                                                               * its number) */
    first = first < 0 ? 0 : first > (int32_t)NSCENES - 4 ? (int32_t)NSCENES - 4 : first;
    for (i = 0; i < 5; i++) {
        uint32_t s = first + i ? (uint32_t)(first + i - 1) : SC_NEW, used = s < NSCENES && (scn.used >> s & 1u);
        int32_t y = 36 + i * SC_ROW, pick = s == st.pick, x, wn, xn = ML + 27;
        uint16_t fc = s == SC_NEW ? C_LAMP : C_ICE;
        scene_t sc;
        char n[4], t[24], nt[SC_NAME + 1];
        scene_get(s, &sc);
        name_text(&sc, nt);
        if (pick) {                      /* framed, lit a little */
            cv_rect(ML - 4, y - oy, MR - ML + 9, SC_ROW - 2, mix565(C_BG, fc, 3));
            hline(ML - 3, MR + 3, y, fc);
            hline(ML - 3, MR + 3, y + SC_ROW - 3, fc);
            cv_rect(ML - 4, y + 1 - oy, 1, SC_ROW - 4, fc);
            cv_rect(MR + 4, y + 1 - oy, 1, SC_ROW - 4, fc);
        }
        if (s < NSCENES) {
            n[0] = (char)('0' + (s + 1u) / 10u);
            n[1] = (char)('0' + (s + 1u) % 10u);
            n[2] = 0;
            text_t(ML + 1, y + 2, &FONT_B, n, pick ? C_ICE : C_SLATE, 1);
        }
        t[0] = 0;
        if (s == SC_NEW) {               /* its mark: a plus, two pixels wide */
            cv_rect(ML + 3, y + 8 - oy, 11, 2, pick ? C_LAMP : C_SLATE);
            cv_rect(ML + 7, y + 4 - oy, 2, 10, pick ? C_LAMP : C_SLATE);
            text_cat(t, "START FRESH");
        } else if (!used) {
            text_cat(t, s < NFACTORY ? "FACTORY" : "EMPTY");
        } else if (sc.nch || sc.narp || sc.bass >= 0) {   /* what it plays */
            if (sc.nch) {
                int_text(sc.nch, t);
                text_cat(t, " DR");
            }
            if (sc.narp)
                text_cat(t, t[0] ? " ARP" : "ARP");
            if (sc.bass >= 0)
                text_cat(t, t[0] ? " BS" : "BASS");
        } else {
            text_cat(t, "SOUND");
        }
        x = MR + 1;                      /* from the right: NOW, then what it holds, if the name leaves room */
        if (s == st.scene)
            x -= text_tw(&FONT_XS, "NOW", 1) + 8;
        wn = text_tw(&FONT_B, nt, 1);
        if (xn + wn + 8 <= x - text_tw(&FONT_XS, t, 1) - 8) {
            x -= text_tw(&FONT_XS, t, 1) + 8;
            text_t(x + 8, y + 4, &FONT_XS, t, used && t[0] != 'S' ? C_ICE : pick && s == SC_NEW ? C_LAMP : C_SLATE, 1);
        }
        if (s == st.scene)               /* the one loaded: NOW */
            text_t(MR + 1 - text_tw(&FONT_XS, "NOW", 1), y + 4, &FONT_XS, "NOW", C_LAMP, 1);
        fit_text(xn, y + 2, x - 8 - xn, nt, pick ? C_WHITE : used || s < NFACTORY || s == SC_NEW ? C_FOG : C_SLATE, 0, 1);
    }
    {                                    /* the two actions, in their box: PLAY LOAD, SAVE + REC STORE */
        static const char *const W[4] = {"PLAY", "LOAD", "SAVE + REC", "STORE"};
        int32_t w = 0, x, k, y = SC_BOX, h = 22, nw = st.pick < NSCENES ? 4 : 2;   /* (NEW SHARD: nothing to store) */
        uint16_t fill = mix565(C_BG, C_ICE, 6);
        ui.acts = (uint8_t)nw;
        for (k = 0; k < nw; k++)
            w += text_tw(&FONT_XS, W[k], 1) + 1;
        w += nw == 4 ? 6 + 23 + 6 : 6;   /* (a key and its word 6 apart; the two 23 apart, a hairline between) */
        cv_rect(ML - 4, y + 1 - oy, MR - ML + 9, h - 2, fill);   /* the box: filled, its corners rounded */
        hline(ML - 3, MR + 3, y, fill);
        hline(ML - 3, MR + 3, y + h - 1, fill);
        x = 120 - w / 2;
        for (k = 0; k < nw; k++) {
            uint16_t c = k & 1 ? C_ICE : C_WHITE;
            text_t(x, y + 4, &FONT_XS, W[k], c, 1);
            x = text_t(x + 1, y + 4, &FONT_XS, W[k], c, 1) + 1;   /* (bold) */
            if (k == 1 && nw == 4) {
                cv_rect(x + 11, y + 5 - oy, 1, h - 10, mix565(fill, C_ICE, 12));
                x += 23;
            } else {
                x += 6;
            }
        }
    }
}

static void middle(float t)
{
    band(R2, 144);
    if (overlay_on()) {                  /* the value large, over a dimmed river */
        int p = st.touched;
        river(R2, st.page == PG_SON ? HZ_HOME : HZ, 0.35f, 0, t);
        if (st.page == PG_SON)
            strata(0.5f);
        {
            int32_t w = BADGE_W + text_tw(&FONT_XS, PD[p].name, 3), x0 = 120 - w / 2;
            badge_of(x0 + 7, 43, p, st.touched_enc);
            text_t(x0 + BADGE_W, 36, &FONT_XS, PD[p].name, C_MIST, 3);
        }
        value_big(120, 52, p, 200, &FONT_L, C_FOG, 1);
        gauge(60, 92, 120, (float)st.v[p] / (float)PD[p].max, C_LAMP);
    } else if (st.page == PG_SON) {
        river(R2, HZ_HOME, 1.0f, 1, t);
        river_status();
        strata(1.0f);
    } else if (st.page == PG_MIXER) {
        mixer();
    } else {                             /* an edit page: the river as a strip above the grid */
        if (st.page == PG_MASTER)
            gr_meter();
        else if (st.page >= PG_LFO1 && st.page <= PG_LFO3T)
            lfo_scope(st.page >= PG_LFO3 ? 2u : st.page >= PG_LFO2 ? 1u : 0u);
        else if (st.page >= PG_SEQ && st.page <= PG_SEQ3)
            seq_scope();
        else if (st.page == PG_BSEQ || st.page == PG_BSEQ2)
            bseq_scope();
        else if (st.page == PG_BASS3 || st.page == PG_GLINT3)
            adsr_scope(st.page == PG_GLINT3);
        else if (st.page != PG_SCENES)
            river(R2, 86, 0.45f, 0, t);
        if (st.page == PG_SCENES) {
            scenes_list();
        } else {
            cell(ML, 100, PAGES[st.page].p[0]);
            cell(130, 100, PAGES[st.page].p[1]);
        }
    }
    cv_blit(0, R2);
}

static void footer(void)
{
    band(R3, 64);
    if (overlay_on() || st.page == PG_SON) {   /* the three hands, as the three encoders on the left sit: KEYS, the
                                              * drone's PRESET (its number among them for its label, CUSTOM's PRESET),
                                              * BODY */
        static const uint8_t M[3] = {P_KB, P_DR_PRESET, P_DR_FULL};   /* A B C, as the badges read */
        char nb[8] = {0};
        const char *lb[3];
        int32_t w[3], tr = 1, bx;
        uint32_t i;
        for (i = 0; i < 3u; i++)
            lb[i] = PD[M[i]].name;
        if (st.v[P_DR_PRESET]) {         /* B's label: its number, "07/24" */
            nb[0] = (char)('0' + st.v[P_DR_PRESET] / 10u);
            nb[1] = (char)('0' + st.v[P_DR_PRESET] % 10u);
            text_cat(nb, "/");
            int_text((int32_t)NDPRESET, nb + 3);
            lb[1] = nb;
        }
        for (;;) {                       /* the labels as they fit, closer if they must */
            for (i = 0; i < 3u; i++)
                w[i] = BADGE_W + text_tw(&FONT_XS, lb[i], tr);
            if (w[0] + w[1] + w[2] + 12 <= MR - ML || tr == 0)
                break;
            tr = 0;
        }
        bx = ML + w[0] + (MR - w[2] - (ML + w[0]) - w[1]) / 2;   /* B: in the middle of what A and C leave */
        for (i = 0; i < 3u; i++) {
            int on = !overlay_on() || st.touched == M[i] || !is_macro(st.touched);
            uint16_t lc = mix565(C_BG, C_MIST, on ? 32 : 16), vc = mix565(C_BG, C_FOG, on ? 32 : 13);
            int32_t x0 = i == 0 ? ML : i == 1u ? bx : MR - w[2];
            badge_of(x0 + 7, 199, M[i], -1);
            text_t(x0 + BADGE_W, 192, &FONT_XS, lb[i], lc, tr);
            if (i == 0)
                value_big(ML, 207, M[i], 44, &FONT_M, vc, 0);
            else if (i == 1)
                value_big(120, 207, M[i], 92, &FONT_M, vc, 1);
            else
                value_big(MR, 207, M[i], 60, &FONT_M, vc, 2);
        }
    } else if (st.page == PG_MIXER) {
        mixer_footer();
    } else if (st.page == PG_SCENES) {   /* MORPH, LOAD and K4's place, empty: three, narrower */
        cell_w(ML, 180, PAGES[PG_SCENES].p[1], 60);
        cell_w(ML + 75, 180, PAGES[PG_SCENES].p[2], 60);
        cell_w(ML + 150, 180, PAGES[PG_SCENES].p[3], 60);
    } else {
        cell(ML, 180, PAGES[st.page].p[2]);
        cell(130, 180, PAGES[st.page].p[3]);
    }
    cv_blit(0, R3);
}

/* ---------------------------------------------------------- the logo --- */
#define LOGO_X ((240 - (int32_t)LOGO_W) / 2)
#define LOGO_Y 126                        /* its waterline at 146: some 60 % sky, the rest water */
#define C_SEA RGB(52, 128, 236)          /* the logo's lower half, deep in the water */

/* what is playing, for the logo's screen: how much sounds, how hard it is pressed, how loud it comes out */
typedef struct {
    float energy;                        /* the voices' levels together, 0 .. 1 */
    float press;                         /* the hardest pressed, 0 .. 1 */
    float out;                           /* the output's peak, 0 .. 1 */
    float speed;                         /* the granules' pace (MOTION, and more as more sounds) */
    int frozen;
} live_t;

static live_t live_now(int still)
{
    live_t L = {0.0f, 0.0f, 0.0f, 1.0f, 0};
    frasil_view_t v[FR_NV];
    uint32_t i;
    if (still)                           /* the splash, before the engine: quiet */
        return L;
    frasil_view(v);
    for (i = 0; i < FR_NV; i++)
        if (v[i].note >= 0) {
            L.energy += v[i].level * 0.4f;
            if (v[i].pressure > L.press)
                L.press = v[i].pressure;
        }
    {                                    /* and the bass */
        bass_view_t b;
        frasil_bass_view(&b);
        if (b.note >= 0) {
            L.energy += b.level * 0.4f;
            if (b.pressure > L.press)
                L.press = b.pressure;
        }
    }
    L.energy = br_clamp01(L.energy);
    L.press = br_clamp01(L.press);
    L.out = br_clamp01(frasil_out_level() * 1.6f);
    L.speed = (0.3f + 0.7f * (float)st.v[P_MOUVEMENT] / 127.0f) * (0.6f + 0.8f * L.energy);
    L.frozen = st.v[P_FREEZE];
    return L;
}

/* frasil drifting by in three depths (the far ones small and slow), more of them and faster as more sounds, the
 * near ones ice, warmed to amber by pressure; a haze along the logo's waterline breathing with the output */
static void granules(float t, const live_t *L)
{
    int32_t k, x, y, n = 100 + (int32_t)(110.0f * L->energy);
    int32_t cut = LOGO_Y + (int32_t)LOGO_CUT;
    int32_t ha = 6 + (int32_t)(18.0f * L->out);
    uint16_t hz = mix565(C_HAZE, C_ICE, (int32_t)(8.0f + 14.0f * L->out));
    for (y = cut - 9; y <= cut + 9; y++) {           /* the haze */
        int32_t d = y - cut, a = ha - (d < 0 ? -d : d) * ha / 10;
        if (a <= 0 || y < oy || y >= oy + (int32_t)cv_h)
            continue;
        for (x = 0; x < 240; x += 1 + (y & 1))
            bl(x, y, hz, a * ui.edge[x] >> 8);
    }
    n = n * (cut - 6) / 240;             /* the sky alone: as dense as they were over the whole screen */
    for (k = 0; k < n; k++) {
        uint32_t h = hsh(0xF5A51u, (uint32_t)k), h2 = hsh(0x1CEu, (uint32_t)k);
        int32_t depth = k % 3, a;
        float speed = (depth == 0 ? 5.0f : depth == 1 ? 11.0f : 21.0f) * (0.7f + 0.6f * (float)(h2 & 255u) / 255.0f);
        float fx = (float)(h & 1023u) * (280.0f / 1024.0f) + ui.gph * speed;
        float fy = (float)(h >> 6 & 1023u) * ((float)(cut - 10) / 1024.0f);   /* over the waterline */
        uint16_t c;
        y = (int32_t)(fy + sn(t * 0.05f + (float)(h2 >> 8 & 255u) * (1.0f / 256.0f)) * 3.0f);
        if (y < oy - 2 || y >= oy + (int32_t)cv_h + 2)   /* (another band's) */
            continue;
        fx -= 280.0f * (float)(int32_t)(fx * (1.0f / 280.0f));
        x = (int32_t)fx - 20;
        a = depth == 0 ? 6 : depth == 1 ? 12 + (int32_t)(h2 >> 4 & 3u) : 20 + (int32_t)(h2 >> 4 & 7u);
        a = (int32_t)((float)a * (0.7f + 0.6f * L->out));
        a = a * (x < 20 ? (x < 0 ? 0 : x) : x > 220 ? 240 - x : 20) / 20;   /* they appear and go at the sides */
        if (L->frozen)
            c = C_ICE;
        else if (depth == 2 && (h2 >> 20 & 255u) < (uint32_t)(L->press * 200.0f))
            c = C_LAMP;                  /* pressed: some of the near ones warm */
        else if (depth == 2)
            c = k % 2 ? C_ICE : C_FOG;
        else if (depth == 1 && (k & 7) == 1)
            c = C_ICE;
        else
            c = C_MIST;
        crystal(x, y, c, br_clamp01((float)a * (1.0f / 32.0f)),
                depth == 0 ? 0.9f : (float)(h2 >> 12 & 15u) * (1.0f / 16.0f), L->frozen, 0);
    }
}

/* the water under the logo's waterline, the screen's lower third: a body of night blue, lighter by the horizon, with
 * long slow swells of light coming through it toward us (closer together far off), a colour a row; on it the sky's
 * light, short strokes in perspective (fine by the horizon, longer near), each coming and going slowly, more of
 * them under the logo. Livelier as more comes out; FREEZE stills it, the strokes in ice. The logo's lower half is
 * drawn over it, in the water */
static void water_body(const live_t *L)   /* (first: written over the bare band, a colour a row) */
{
    int32_t cut = LOGO_Y + (int32_t)LOGO_CUT, x, y;
    int32_t y0 = cut + 1 > oy ? cut + 1 : oy, y1 = oy + (int32_t)cv_h < 240 ? oy + (int32_t)cv_h : 240;
    float sw = 1.0f + 0.5f * L->out;
    float g0 = (float)(C_BG >> 5 & 63u), gd = (float)(C_ICE >> 5 & 63u) - g0;
    float b0 = (float)(C_BG & 31u), bd = (float)(C_ICE & 31u) - b0;
    for (y = y0; y < y1; y++) {
        float z = (float)(y - cut) * (1.0f / (float)(240 - cut));
        float lv = 0.6f + 3.2f * (1.0f - z) * (1.0f - z) +
                   1.1f * sw * (1.0f - 0.5f * z) * sn(2.2f * fm_sqrtf(z) - ui.wph * 0.08f);
        uint32_t g, b;
        uint16_t c, *row = &cv_px[(uint32_t)(y - oy) * cv_w];
        if (lv < 0.0f)
            lv = 0.0f;
        /* from the background toward ice, lv of 32: green and blue each rounded on its own, red the background's
         * (a grain of two levels flickered as the swells went through, and red's coarse steps put mauve in it) */
        g = (uint32_t)(g0 + gd * lv * (1.0f / 32.0f) + 0.5f);
        b = (uint32_t)(b0 + bd * lv * (1.0f / 32.0f) + 0.5f);
        c = swap16((uint16_t)((C_BG & 0xF800u) | (g > 63u ? 63u : g) << 5 | (b > 31u ? 31u : b)));
        for (x = 0; x < 240; x++)
            row[x] = c;
    }
}

static void water(const live_t *L)
{
    int32_t cut = LOGO_Y + (int32_t)LOGO_CUT, x, r, j, i;
    int32_t y1 = oy + (int32_t)cv_h < 240 ? oy + (int32_t)cv_h : 240;
    for (r = 0; r < 14; r++) {           /* the strokes of light, a row at a time down to the near water */
        float z = ((float)r + 0.5f) * (1.0f / 14.0f);
        int32_t yr = cut + 3 + (int32_t)(75.0f * z * fm_sqrtf(z));
        float sway = 1.5f * z * sn(ui.wph * 0.11f + (float)yr * 0.031f);
        if (yr < oy || yr >= y1)
            continue;
        for (j = 0; j < 9; j++) {
            uint32_t h = hsh((uint32_t)(r * 31 + j), 0x5EAu), h2 = hsh((uint32_t)(r * 31 + j), 0x7EDu);
            int32_t len = 1 + (int32_t)(z * 7.0f) + (int32_t)(h & 1u);
            float vx = ((float)(h2 & 255u) * (1.0f / 255.0f) - 0.5f) * 2.0f;   /* drifting, a pixel or so a second */
            float fx = (float)(h >> 4 & 1023u) * (280.0f / 1024.0f) + ui.wph * vx, life, a, dx;
            uint16_t c = L->frozen || (h2 >> 16 & 3u) ? C_ICE : C_FOG;
            fx -= 280.0f * fm_floorf(fx * (1.0f / 280.0f));
            fx += sway - 20.0f;
            life = sn(ui.wph * (0.11f + 0.08f * (float)(h2 >> 8 & 255u) * (1.0f / 255.0f)) +
                      (float)(h >> 8 & 255u) * (1.0f / 256.0f));
            if (life <= 0.0f)            /* each is there half the time, fading in and out */
                continue;
            dx = (fx - 120.0f) * (1.0f / 110.0f);
            a = (8.0f + 10.0f * L->out + 4.0f * (1.0f - z)) * life * life * (0.3f + 0.7f * br_clamp01(1.0f - dx * dx));
            for (i = 0; i < len; i++) {  /* its ends softer */
                float e = len > 1 ? fm_fabsf(2.0f * (float)i / (float)(len - 1) - 1.0f) : 0.0f;
                x = (int32_t)fx + i;
                if (x >= 0 && x < 240)
                    bl(x, yr, c, (int32_t)(a * (1.0f - 0.6f * e) * (float)ui.edge[x] * (1.0f / 256.0f) + 0.5f));
            }
        }
    }
}

/* the logo, its lower half (under the water) tinted ice as the output rises */
static uint32_t logo_px(int32_t x, uint32_t y)   /* the logo's intensity at (x, y), 0..15 (0 outside) */
{
    uint32_t b;
    if (x < 0 || x >= (int32_t)LOGO_W)
        return 0;
    b = LOGO_PX[(y * LOGO_W + (uint32_t)x) >> 1];
    return (x & 1) ? (b & 15u) : (b >> 4);
}

/* the logo: over the waterline as it is; under it, in the water: blue, ice by the surface, deep sea blue at the bottom
 * (a little lighter as the output rises), and swaying slowly, the deeper the more (a pixel and a half at the bottom),
 * as a thing seen through moving water */
static void logo_draw(float out, float t)
{
    int32_t x, y, y0 = oy - LOGO_Y > 0 ? oy - LOGO_Y : 0;
    int32_t y1 = oy + (int32_t)cv_h - LOGO_Y < (int32_t)LOGO_H ? oy + (int32_t)cv_h - LOGO_Y : (int32_t)LOGO_H;
    for (y = y0; y < y1; y++) {          /* (this band's rows) */
        if (y <= (int32_t)LOGO_CUT) {
            for (x = 0; x < (int32_t)LOGO_W; x++) {
                uint32_t i = logo_px(x, (uint32_t)y);
                if (i)
                    bl(LOGO_X + x, LOGO_Y + y, C_FOG, (int32_t)(i * 32u / 15u));
            }
        } else {
            float d = (float)(y - (int32_t)LOGO_CUT) / (float)(LOGO_H - LOGO_CUT);
            float dx = (0.35f + 1.15f * d) * sn(t * 0.23f + (float)y * 0.045f), f, g;
            uint16_t under = mix565(C_ICE, C_SEA, (int32_t)((10.0f + 22.0f * d) * (1.0f - 0.35f * out)));
            uint8_t px[LOGO_W + 4u];     /* the row, unpacked, two bare pixels each side */
            float tt = t * 0.31f + (float)y * 0.07f;
            uint32_t ph;                 /* the light: turns, 16.16 */
            int32_t k = (int32_t)(-dx + 64.0f) - 64;      /* the row's sway: x - dx = x + k + f */
            f = -dx - (float)k;
            g = (1.0f - f) * (32.0f / 15.0f);
            ph = (uint32_t)((tt - (float)(int32_t)tt) * 65536.0f);
            f *= 32.0f / 15.0f;
            px[0] = px[1] = px[LOGO_W + 2u] = px[LOGO_W + 3u] = 0;
            for (x = 0; x < (int32_t)LOGO_W; x++)
                px[x + 2] = (uint8_t)logo_px(x, (uint32_t)y);
            for (x = 0; x < (int32_t)LOGO_W; x++) {
                const uint8_t *q = &px[x + k + 2];
                float a = (float)q[0] * g + (float)q[1] * f;
                if (a > 0.5f) {          /* light moving under */
                    a *= 0.86f + 0.1f * ui.sn[(ph + (uint32_t)x * 852u) >> 8 & 255u];
                    bl(LOGO_X + x, LOGO_Y + y, under, (int32_t)a);
                }
            }
        }
    }
}

/* the whole screen in two bands: the sky's granules and the water behind, the glints around, the logo, the tagline
 * under it, the author at the bottom. still: the splash, before anything plays */
static void logo_scene(float t, int still)
{
    live_t L = live_now(still);
    float dt = t - ui.lt;
    int32_t b;
    ui.lt = t;
    if (dt < 0.0f || dt > 0.25f)         /* (its first frame, or back to it after a while) */
        dt = 0.05f;
    if (L.frozen)
        t = ui.stillt;                   /* set: the granules stop where they are */
    else {                               /* their paces added up a frame at a time: t times a pace the sound changes
                                          * would leap ahead as a pad comes in, and race back as it fades */
        ui.gph += dt * L.speed;
        ui.wph += dt * (0.7f + 0.3f * L.speed);
    }
    for (b = 0; b < 2; b++) {
        band(b ? 144 : 0, b ? 96 : 144);
        water_body(&L);
        granules(t, &L);
        water(&L);
        if (!still)
            glints(0, 150, 1.0f);
        logo_draw(L.out, t);
        text_c(120, 176, &FONT_T, APP_TAGLINE, mix565(C_MIST, C_ICE, (int32_t)(6.0f + 12.0f * L.out)), 2);
        text_c(120, 218, &FONT_S, APP_AUTHOR, C_WHITE, 1);
        {                                /* the version, top right ("DEV" but for a release) */
            char v[16] = {0};
            if (APP_VERSION[0] >= '0' && APP_VERSION[0] <= '9')
                text_cat(v, "v");
            text_cat(v, APP_VERSION);
            text_r(MR, 10, &FONT_XS, v, C_MIST, 1);
        }
        cv_blit(0, (uint32_t)oy);
    }
}

/* power-on, before the engine: the same, still */
void app_splash(void)
{
    uint32_t i;
    for (i = 0; i < 240u; i++) {
        float e = smooth01((float)((int32_t)i - ML) / 40.0f) * smooth01((float)(MR - (int32_t)i) / 40.0f);
        ui.edge[i] = (int32_t)(e * 256.0f);
    }
    logo_scene(0.0f, 1);
}

/* --------------------------------------------------------------- the help --- */
/* HOME held: what each control does, four to a page, in the panel's order; SELECT turns the pages, HOME closes it.
 * Each line: what it does, large; its control on the right (a key cap, or an encoder's badge); what it does held,
 * small, in amber, or the track it plays on, in the track's colour. The page's button ringed in ice, the modes that
 * are on (the keys not on drones, the latch) lit */
enum { H_BTN, H_ENC, H_GEST };
typedef struct {
    uint8_t kind, b;                     /* H_BTN: button b; H_ENC: badge b ('A', 'B', 'C', or 0: OCT); H_GEST */
    const char *does, *control, *held;
} help_t;
static const char *const HELP_TITLE[HELP_PAGES] = {"BUTTONS", "BUTTONS", "BUTTONS", "ENCODERS", "KEYS"};
static const help_t HELP[HELP_PAGES][4] = {
    {{H_BTN, B_FX, "GENERATIVE FX", "FX", "AGAIN: CLOUD, PUMP / HOLD: FREEZE"}, {H_BTN, B_SEL, "HARMONY", "SEL", "HOLD + KEY: SINGLE NOTE"},
     {H_BTN, B_ENV, "DRONE SHAPE", "ENV", "HOLD: SWELL"}, {H_BTN, B_LFO, "DRONE MOTION", "LFO", "AGAIN: LFO 1-3 / HOLD+K: ROUTE"}},
    {{H_BTN, B_EDIT, "DRONE, LAYERS", "EDIT", "AGAIN: LAYERS / HOLD: COMPARE"}, {H_BTN, B_GLO, "GLOBAL", "GLO", "AGAIN: MIDI / HOLD + CC: LEARN"},
     {H_BTN, B_HOME, "PLAY / MIXER", "HOME", "AGAIN: MIXER, LOGO / HOLD: HELP"}, {H_BTN, B_SAVE, "SCENES", "SAVE", "HOLD + KEY: LOAD / + REC: STORE"}},
    {{H_BTN, B_ARP, "GLINT PAGES", "ARP", "HOLD: ARP LATCH"}, {H_BTN, B_SEQ, "BASS PAGES", "SEQ", ""},
     {H_BTN, B_PLAY, "START, STOP", "PLAY", "HOLD: RELEASE ALL / SCENES: LOAD"}, {H_BTN, B_REC, "RECORD LIVE", "REC", "AGAIN: OFF / HOLD: METRONOME"}},
    {{H_ENC, 'A', "KEYS", "SELECT", "HOLD A PAGE BUTTON: ITS PAGES"}, {H_ENC, 'B', "PRESET", "PRESETS", ""},
     {H_ENC, 'C', "BODY", "ALGORITHM", ""}, {H_ENC, 0, "OCTAVE", "OCT- OCT+", "BOTH: RESET"}},
    {{H_GEST, 0, "LIGHT A DRONE", "TAP", "A CHORD WITH HARMONY"}, {H_GEST, 0, "LET IT GO", "TAP AGAIN", ""},
     {H_GEST, 0, "SWELL IT", "PRESS IN", "HOLD THE KEY, PRESS HARDER"}, {H_GEST, 0, "ONE NOTE", "SEL + KEY", "OVER A CHORD"}},
};
static const char *const HELP_TRACK[HELP_PAGES][4] = {   /* the track a line's control plays on, if only one */
    [3] = {0, "DRONE", "DRONE", 0},
};

static void keycap(int32_t xr, int32_t y, const char *s, int on, int here)   /* right edge xr */
{
    int32_t w = text_tw(&FONT_B, s, 1) + 16, x = xr - w + 1;
    uint16_t edge = here || on ? C_ICE : C_SLATE;
    if (on)
        cv_rect(x, y - oy, w, 22, C_ICE);
    hline(x + 1, x + w - 2, y, edge);
    hline(x + 1, x + w - 2, y + 21, edge);
    cv_rect(x, y + 1 - oy, 1, 20, edge);
    cv_rect(x + w - 1, y + 1 - oy, 1, 20, edge);
    text_t(x + 8, y + 4, &FONT_B, s, on ? C_BG : here ? C_ICE : C_FOG, 1);
}

static void help_screen(void)
{
    int32_t b, i;
    uint32_t pg = st.help_page % HELP_PAGES;
    for (b = 0; b < 2; b++) {
        band(b ? 144 : 0, b ? 96 : 144);
        {                                /* the header: HELP, the page's name, its dots, SELECT's badge */
            int32_t x = title("HELP", 0), k;
            text_t(x + 8, 9, &FONT_XS, HELP_TITLE[pg], C_MIST, 1);
            for (k = 0; k < HELP_PAGES; k++)
                cv_rect(MR - (HELP_PAGES - 1 - k) * 8 - 3, 13 - oy, k == (int32_t)pg ? 5 : 3, k == (int32_t)pg ? 5 : 3,
                        k == (int32_t)pg ? C_ICE : C_SLATE);
            badge(MR - HELP_PAGES * 8 - 10, 15, 'A', 0);
            hline(ML, MR, 30, C_STEEL);
        }
        for (i = 0; i < 4; i++) {
            const help_t *h = &HELP[pg][i];
            int32_t y = 36 + i * 50;
            text_t(ML, y + 3, &FONT_M, h->does, C_WHITE, 0);
            if (h->held[0])
                text_t(ML, y + 30, &FONT_XS, h->held, C_LAMP, 1);
            else if (HELP_TRACK[pg][i])  /* (the keys' badge's colour for the drones) */
                text_t(ML, y + 30, &FONT_XS, HELP_TRACK[pg][i], C_FOG, 1);
            if (h->kind == H_BTN) {
                int on = (h->b == B_REC && (st.v[P_SQ_REC] || st.v[P_BQ_REC])) || (h->b == B_ARP && arp.latch) ||
                         (h->b == B_FX && st.v[P_FREEZE]);
                keycap(MR, y + 4, h->control, on, PAGES[st.page].button == h->b);
            } else if (h->kind == H_ENC && h->b) {
                badge_r(MR - 10, y + 15, (char)h->b, 0, 9.0f);
                text_r(MR - 26, y + 9, &FONT_XS, h->control, C_MIST, 1);
            } else {
                keycap(MR, y + 4, h->control, 0, 0);
            }
            if (i < 3)
                hline(ML, MR, y + 47, C_STEEL);
        }
        cv_blit(0, (uint32_t)oy);
    }
}

/* ------------------------------------------------------------ a pop-up --- */
/* app.c's popup: where it comes from, small, by a light in its colour; the title large, in lamp (a warning) or ice
 * (done); the waterline, brighter in the middle; what to do; and its one way out, a button framed in its colour, in
 * the middle: OK, SEL's key in it */
static void popup_screen(void)
{
    int32_t b, i, x, k;
    uint16_t c = pop.kind == POP_DONE ? C_ICE : C_LAMP;
    int32_t wk = text_tw(&FONT_B, "SEL", 1) + 16, w = 20 + text_tw(&FONT_B, "OK", 2) + 14 + wk + 8, h = 38;
    int32_t x0 = 120 - w / 2, y0 = 190;
    for (b = 0; b < 2; b++) {
        band(b ? 144 : 0, b ? 96 : 144);
        for (k = 0; k < 49; k++) {       /* the light: a round dot and its glow */
            int32_t dx = k % 7 - 3, dy = k / 7 - 3, d = dx * dx + dy * dy;
            bl(ML + 3 + dx, 46 + dy, c, d <= 2 ? 32 : d <= 5 ? 12 : d <= 9 ? 4 : 0);
        }
        text_t(ML + 13, 40, &FONT_XS, pop.kicker, C_MIST, 1);
        text_t(ML, 58, &FONT_L, pop.title, c, 1);
        for (x = ML; x <= MR; x++) {     /* the waterline */
            int32_t d = x - 120 < 0 ? 120 - x : x - 120;
            bl(x, 102, C_FOG, 22 - d * 18 / (MR - 120));
        }
        for (i = 0; i < 3; i++)
            if (pop.line[i][0])
                text_t(ML, 116 + i * 21, &FONT_S, pop.line[i], C_FOG, 0);
        cv_rect(x0 + 1, y0 + 1 - oy, w - 2, h - 2, mix565(C_BG, c, 3));   /* the button: lit a little, framed */
        for (k = 0; k < 2; k++) {
            hline(x0 + 1 + k, x0 + w - 2 - k, y0 + k, c);
            hline(x0 + 1 + k, x0 + w - 2 - k, y0 + h - 1 - k, c);
            cv_rect(x0 + k, y0 + 1 + k - oy, 1, h - 2 - 2 * k, c);
            cv_rect(x0 + w - 1 - k, y0 + 1 + k - oy, 1, h - 2 - 2 * k, c);
        }
        text_t(x0 + 20, y0 + 11, &FONT_B, "OK", C_WHITE, 2);
        keycap(x0 + w - 9, y0 + 8, "SEL", 0, 0);
        cv_blit(0, (uint32_t)oy);
    }
}

/* fm1-base's restore question (app.h APP_BACKUP_ASK), as a pop-up: RESTORE FROM USB by a lamp light, RESTORE? large,
 * what it does; its two answers side by side as on the panel: NO, FX's key (at the left, the panel's first), YES, SEL's,
 * framed in ice */
static void ask_button(int32_t x0, int32_t y0, int32_t w, const char *word, const char *key, uint16_t c)
{
    int32_t k, h = 38;
    cv_rect(x0 + 1, y0 + 1 - oy, w - 2, h - 2, mix565(C_BG, c, 3));
    for (k = 0; k < 2; k++) {
        hline(x0 + 1 + k, x0 + w - 2 - k, y0 + k, c);
        hline(x0 + 1 + k, x0 + w - 2 - k, y0 + h - 1 - k, c);
        cv_rect(x0 + k, y0 + 1 + k - oy, 1, h - 2 - 2 * k, c);
        cv_rect(x0 + w - 1 - k, y0 + 1 + k - oy, 1, h - 2 - 2 * k, c);
    }
    text_t(x0 + 13, y0 + 11, &FONT_B, word, C_WHITE, 2);
    keycap(x0 + w - 8, y0 + 8, key, 0, 0);
}

void app_backup_ask_screen(void)
{
    int32_t b, x, k;
    for (b = 0; b < 2; b++) {
        band(b ? 144 : 0, b ? 96 : 144);
        for (k = 0; k < 49; k++) {       /* the light */
            int32_t dx = k % 7 - 3, dy = k / 7 - 3, d = dx * dx + dy * dy;
            bl(ML + 3 + dx, 46 + dy, C_LAMP, d <= 2 ? 32 : d <= 5 ? 12 : d <= 9 ? 4 : 0);
        }
        text_t(ML + 13, 40, &FONT_XS, "RESTORE FROM USB", C_MIST, 1);
        text_t(ML, 58, &FONT_L, "RESTORE?", C_LAMP, 1);
        for (x = ML; x <= MR; x++) {     /* the waterline */
            int32_t d = x - 120 < 0 ? 120 - x : x - 120;
            bl(x, 102, C_FOG, 22 - d * 18 / (MR - 120));
        }
        text_t(ML, 116, &FONT_S, "IT REPLACES ALL SCENES", C_FOG, 0);
        text_t(ML, 137, &FONT_S, "AND THE SETTINGS.", C_FOG, 0);
        ask_button(ML - 2, 190, 96, "NO", "FX", C_SLATE);
        ask_button(MR - 108, 190, 112, "YES", "SEL", C_ICE);
        cv_blit(0, (uint32_t)oy);
    }
}

/* app.c's name: STORED and the slot, small, by an ice light; the sixteen places of the name, the cursor's framed in
 * ice; how to write it (OCT- OCT+ the cursor, K4 the letters, K3 the digits); FX to leave it as stored, and the one to
 * store it, framed in ice, SEL (OK, as everywhere; FX no, as everywhere) */
static void name_screen(void)
{
    int32_t b, i, k, x;
    char t[24], ch[2] = {0, 0};
    int32_t wk = text_tw(&FONT_B, "SEL", 1) + 16, w = 20 + text_tw(&FONT_B, "STORE", 2) + 14 + wk + 8, h = 38;
    int32_t x0 = 120 - w / 2, y0 = 192;
    t[0] = 0;
    text_cat(t, "STORED IN ");
    t[10] = (char)('0' + (nm.slot + 1u) / 10u);
    t[11] = (char)('0' + (nm.slot + 1u) % 10u);
    t[12] = 0;
    for (b = 0; b < 2; b++) {
        band(b ? 144 : 0, b ? 96 : 144);
        for (k = 0; k < 49; k++) {       /* the light */
            int32_t dx = k % 7 - 3, dy = k / 7 - 3, d = dx * dx + dy * dy;
            bl(ML + 3 + dx, 18 + dy, C_ICE, d <= 2 ? 32 : d <= 5 ? 12 : d <= 9 ? 4 : 0);
        }
        text_t(ML + 13, 12, &FONT_XS, t, C_MIST, 1);
        text_t(ML, 28, &FONT_M, "NAME IT", C_WHITE, 2);
        for (i = 0; i < (int32_t)SC_NAME; i++) {   /* the places, 13 px each: a line under each, the cursor's framed */
            int32_t cx = 16 + i * 13, cur = i == (int32_t)nm.cur, cw;
            ch[0] = nm.name[i];
            if (cur) {
                cv_rect(cx, 63 - oy, 13, 32, mix565(C_BG, C_ICE, 4));
                hline(cx, cx + 12, 62, C_ICE);
                hline(cx, cx + 12, 95, C_ICE);
                cv_rect(cx - 1, 63 - oy, 1, 32, C_ICE);
                cv_rect(cx + 13, 63 - oy, 1, 32, C_ICE);
            } else {
                hline(cx + 2, cx + 10, 94, C_STEEL);
            }
            if (ch[0] != ' ') {          /* (bold: drawn twice) */
                cw = text_w(&FONT_B, ch) + 1;
                text_t(cx + 7 - cw / 2, 70, &FONT_B, ch, cur ? C_ICE : C_WHITE, 0);
                text_t(cx + 8 - cw / 2, 70, &FONT_B, ch, cur ? C_ICE : C_WHITE, 0);
            }
        }
        x = ML + text_tw(&FONT_B, "OCT-", 1) + 15;   /* how: OCT- OCT+ the cursor, K4 letters, K3 digits, FX cancels */
        keycap(x, 110, "OCT-", 0, 0);
        x += 6 + text_tw(&FONT_B, "OCT+", 1) + 16;
        keycap(x, 110, "OCT+", 0, 0);
        text_t(x + 10, 115, &FONT_XS, "MOVE", C_MIST, 1);
        badge(ML + 7, 147, '4', 1);
        x = text_t(ML + 19, 141, &FONT_XS, "LETTERS", C_MIST, 1);
        badge(x + 18, 147, '3', 1);
        text_t(x + 30, 141, &FONT_XS, "DIGITS", C_MIST, 1);
        x = ML + text_tw(&FONT_B, "FX", 1) + 15;
        keycap(x, 160, "FX", 0, 0);
        text_t(x + 10, 165, &FONT_XS, "CANCEL", C_MIST, 1);
        cv_rect(x0 + 1, y0 + 1 - oy, w - 2, h - 2, mix565(C_BG, C_ICE, 3));   /* STORE, SEL's key in it, framed */
        for (k = 0; k < 2; k++) {
            hline(x0 + 1 + k, x0 + w - 2 - k, y0 + k, C_ICE);
            hline(x0 + 1 + k, x0 + w - 2 - k, y0 + h - 1 - k, C_ICE);
            cv_rect(x0 + k, y0 + 1 + k - oy, 1, h - 2 - 2 * k, C_ICE);
            cv_rect(x0 + w - 1 - k, y0 + 1 + k - oy, 1, h - 2 - 2 * k, C_ICE);
        }
        text_t(x0 + 20, y0 + 11, &FONT_B, "STORE", C_WHITE, 2);
        keycap(x0 + w - 9, y0 + 8, "SEL", 0, 0);
        cv_blit(0, (uint32_t)oy);
    }
}

/* -------------------------------------------------------------- frames --- */
static void ui_init(void)
{
    uint32_t i;
    for (i = 0; i < 256u; i++)
        ui.sn[i] = fm_sinf(6.2831853f * (float)i / 256.0f);
    for (i = 0; i < 240u; i++) {
        float e = smooth01((float)((int32_t)i - ML) / 40.0f) * smooth01((float)(MR - (int32_t)i) / 40.0f);
        ui.edge[i] = (int32_t)(e * 256.0f);
    }
    lcd_fill(0, 0, 240, 240, C_BG);
    ui.full = 1;
}

/* what a band shows, as a number: it is drawn again when that changes */
static uint32_t sig_add(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

static void ui_frame(void)
{
    uint32_t now = plat_ms(), s1 = 2166136261u, s3 = 2166136261u, i;
    int ov = overlay_on();
    if (pop.on) {                        /* a pop-up: over everything, drawn once */
        if (!pop.drawn || !ui.was_pop)
            popup_screen();
        pop.drawn = 1;
        ui.was_pop = 1;
        return;
    }
    if (nm.on) {                         /* a name being written: over everything, drawn as it changes */
        if (!nm.drawn || !ui.was_pop)
            name_screen();
        nm.drawn = 1;
        ui.was_pop = 1;
        return;
    }
    if (ui.was_pop) {                    /* closed: back where we were, all of it (the help too) */
        ui.was_pop = 0;
        ui.full = 1;
        ui.was_logo = 0;
        ui.help_sig = ~0u;
    }
    if (st.help) {                       /* the help: drawn again when its page or a mode changes */
        uint32_t hs = st.help_page | (uint32_t)st.bass << 4 | (uint32_t)st.glint << 5 | (uint32_t)arp.latch << 6 |
                      (uint32_t)(st.v[P_SQ_REC] || st.v[P_BQ_REC]) << 7 | (uint32_t)st.page << 8 |
                      (uint32_t)st.v[P_FREEZE] << 16;
        if (!ui.was_map || hs != ui.help_sig)
            help_screen();
        ui.help_sig = hs;
        ui.was_map = 1;
        return;
    }
    if (ui.was_map) {                    /* let go: back where we were, all of it */
        ui.was_map = 0;
        ui.full = 1;
        ui.was_logo = 0;
    }
    if (st.logo && st.logo_until && now >= st.logo_until)
        st.logo = 0;
    if (st.logo) {                       /* the logo's screen, alive: 20 frames a second, 12 while the sound takes
                                          * most of the processor (it moves slowly; the keys stay quick) */
        if (!ui.was_logo || now - ui.field_at >= (plat_cpu_pct() > 75u ? 80u : 50u)) {
            ui.field_at = now;
            logo_scene((float)now * 0.001f, 0);
            ui.frames++;
        }
        ui.was_logo = 1;
        return;
    }
    if (ui.was_logo) {                   /* back from it: everything again */
        ui.was_logo = 0;
        ui.full = 1;
    }
    /* the header: the page, the freeze, the keys' mode, the octave, a message, the harmony, the load */
    s1 = sig_add(s1, st.page | (uint32_t)st.v[P_FREEZE] << 8 | (uint32_t)st.glint << 9 | (uint32_t)(now < say.msg_until) << 10 |
                         (uint32_t)arp.latch << 11 | (uint32_t)(st.v[P_SQ_TYPE] != 0) << 12 | (uint32_t)st.bass << 13 |
                         (uint32_t)st.v[P_KEYS] << 14 | (uint32_t)st.v[P_SQ_TYPE] << 15 | (uint32_t)st.v[P_SQ_REC] << 17 |
                         (uint32_t)st.dirty << 19 | (uint32_t)st.compare << 20 | (uint32_t)(frasil_shed_level() > 0) << 21 |
                         (uint32_t)mo.on << 22 | (uint32_t)st.scene << 23 | (uint32_t)st.v[P_BQ_ON] << 28 |
                         (uint32_t)st.v[P_BQ_REC] << 29);
    s1 = sig_add(s1, (uint32_t)(play_holding() * 32.0f) | (mo.on ? (now - mo.t0) * 64u / (mo.dur ? mo.dur : 1u) : 0u) << 8);
    s1 = sig_add(s1, (uint32_t)(st.octave + 8));
    s1 = sig_add(s1, say.msg_until);
    s1 = sig_add(s1, st.v[P_ROOT] | st.v[P_SCALE] << 4 | (uint32_t)st.v[P_CHORD] << 8);
    for (i = 0; i < SC_NAME && st.scene < NSCENES; i++)   /* (the scene's name, as it may change: NAME IT, a
                                                           * restore; NEW SHARD's never does) */
        s1 = sig_add(s1, (uint8_t)scn.s[st.scene].name[i] | (scn.used >> st.scene & 1u) << 8);
    if (st.page == PG_GLOBAL)
        s1 = sig_add(s1, plat_cpu_pct() << 8 | ui.fps);
    /* the footer: the page, the overlay, the values */
    s3 = sig_add(s3, st.page | (uint32_t)ov << 8 | (uint32_t)st.dirty << 9 | (uint32_t)st.pick << 10);
    s3 = sig_add(s3, touched_now() ? st.touched : 255u);
    for (i = 0; i < 4u; i++) {           /* (and where an LFO has them) */
        uint8_t p = PAGES[st.page].p[i];
        s3 = sig_add(s3, p < NPARAM ? st.v[p] : 0u);
        s3 = sig_add(s3, p < NSOUND ? (uint32_t)(int32_t)(mod[p] * 2.0f) : 0u);
    }
    s3 = sig_add(s3, st.v[P_KB] | st.v[P_DR_PRESET] << 8 | (uint32_t)st.v[P_DR_FULL] << 16);   /* (the three hands) */
    if (ui.full || s1 != ui.sig1) {
        ui.sig1 = s1;
        header();
    }
    if (ui.full || now - ui.field_at >= 50u) {   /* the river moves: 20 frames a second */
        ui.field_at = now;
        middle((float)now * 0.001f);
        ui.frames++;
    }
    if (ui.full || s3 != ui.sig3) {
        ui.sig3 = s3;
        footer();
    }
    if (now - ui.fps_at >= 1000u) {
        ui.fps = ui.frames;
        ui.frames = 0;
        ui.fps_at = now;
    }
    ui.full = 0;
}
