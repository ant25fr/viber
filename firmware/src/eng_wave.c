/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* WAVE: a wavetable oscillator. Felucca's own design and tables (tools/gen_wavetables.py,
 * build/gen/felucca_wave.h): 8 tables of 16 single-cycle frames, 64 samples, 8-bit (PPG-style:
 * deliberately small and a little lo-fi), stored in 4 KB: see wv_frame.
 *
 * TABLE and WAVE pick a point in a continuous (table, frame) grid: the four frames around it
 * (TABLE's floor/ceil x WAVE's floor/ceil) blend bilinearly (Q8) into a 64-sample working frame
 * once a control tick (wv_blend), then the audible phase just reads that frame with linear
 * interpolation (wv_sample), as cheap as the sine table every other engine uses. ENV > WAVE adds
 * the voice's own envelope (m->envq15, already computed) to WAVE before the blend: the classic
 * wavetable move of a sound's own envelope sweeping its own timbre, free (no extra state).
 *
 * SUB (an octave down) and DETUNE (a second voice, slightly sharp) read the same blended frame on
 * their own phases for thickness. WARP folds the mixed signal (a sine driven past its table, PPG's
 * "wave" turned into a fold). DRIFT is a slow bounded random walk on the main pitch (v->s[0], a
 * leaky integrator fed by v->s[1]'s noise) for an analog-ish wobble; NOISE mixes in filtered hiss
 * of its own (v->s[2]) under the tone.
 *
 * Cost: one 64-sample bilinear blend a control tick (cheap: ~4 ops x 64, once every CTL samples),
 * then up to three table reads and a multiply-add a sample (main, SUB, DETUNE: as WHEEL's partials),
 * plus WARP's bounded fold loop only when it is turned up. voice_t.s[0..2] are this engine's; no
 * per-part memory (engines.c eng_mem_t) is needed. */
#include "felucca_wave.h"

/* frame[i] at phase ph (ph's top 6 bits: 0..63; the rest, a linear fraction to the next sample),
 * scaled to the Q15 the other oscillators use (SINE etc): an 8-bit table loses nothing by it */
static inline int32_t wv_sample(const int8_t *frame, uint32_t ph)
{
    uint32_t i = ph >> 26, frac = (ph & 0x3FFFFFFu) >> 10;
    int32_t a = frame[i], b = frame[(i + 1u) & 63u];
    return (a + (((b - a) * (int32_t)frac) >> 16)) << 8;
}

/* frame fr of table tab as 64 samples: a pointer into the stored table (WK_FULL: nothing copied) or buf, which it fills
 * (the tables are stored compactly, tools/gen_wavetables.py: an odd-symmetric one as its first half, a pulse as its
 * length, the crushed saw not at all; each expands to exactly the samples the generator made) */
static const int8_t *wv_frame(int8_t *buf, uint32_t tab, uint32_t fr)
{
    uint32_t i, k = WAVE_SLOT[tab];
    switch (WAVE_KIND[tab]) {
    case WK_FULL:
        return WAVE_FULL[k][fr];
    case WK_ODD: {
        const int8_t *h = WAVE_ODD[k][fr];
        buf[0] = buf[32] = 0;
        for (i = 1; i < 32u; i++) {
            buf[i] = h[i - 1u];
            buf[64u - i] = (int8_t)-h[i - 1u];
        }
        break;
    }
    case WK_SQUARE: {
        uint32_t on = WAVE_SQON[fr];
        for (i = 0; i < WAVE_FLEN; i++)
            buf[i] = i < on ? 127 : -127;
        break;
    }
    default: {                                   /* WK_CHIP: a saw in 32 - 2 fr levels (2 at least), rounded half to even */
        int32_t lv = (int32_t)(fr < 15u ? 31u - 2u * fr : 1u);
        for (i = 0; i < WAVE_FLEN; i++) {
            int32_t n = (int32_t)i * lv, q = (n + 32) >> 6, v;
            if ((n & 63) == 32 && (q & 1))
                q--;
            v = (2 * q - lv) * 127;
            buf[i] = (int8_t)(v >= 0 ? (v + lv / 2) / lv : -((-v + lv / 2) / lv));
        }
        break;
    }
    }
    return buf;
}

/* the four frames around a continuous (table 0..127, wave 0..127) position, bilinear (Q8) into out[64] */
static void wv_blend(int8_t *out, int32_t table, int32_t wave)
{
    int32_t pt = clamp(table, 0, 127) * (WAVE_NTAB - 1u) * 256 / 127, pw = clamp(wave, 0, 127) * (WAVE_NFRAME - 1u) * 256 / 127;
    uint32_t ti = (uint32_t)(pt >> 8), wi = (uint32_t)(pw >> 8), ft = (uint32_t)pt & 255u, fw = (uint32_t)pw & 255u, i;
    int8_t b0[WAVE_FLEN], b1[WAVE_FLEN];
    const int8_t *f0, *f1;
    if (ti > WAVE_NTAB - 2u) { ti = WAVE_NTAB - 2u; ft = 255u; }
    if (wi > WAVE_NFRAME - 2u) { wi = WAVE_NFRAME - 2u; fw = 255u; }
    f0 = wv_frame(b0, ti, wi); f1 = wv_frame(b1, ti + 1u, wi);               /* WAVE's floor frame across TABLE ... */
    for (i = 0; i < WAVE_FLEN; i++)
        out[i] = (int8_t)(f0[i] + (((f1[i] - f0[i]) * (int32_t)ft) >> 8));
    f0 = wv_frame(b0, ti, wi + 1u); f1 = wv_frame(b1, ti + 1u, wi + 1u);     /* ... and its ceil frame, then across WAVE */
    for (i = 0; i < WAVE_FLEN; i++) {
        int32_t s0 = out[i], s1 = f0[i] + (((f1[i] - f0[i]) * (int32_t)ft) >> 8);
        out[i] = (int8_t)(s0 + (((s1 - s0) * (int32_t)fw) >> 8));
    }
}

/* a sine-driven fold (PPG's "wave" past its table): x past +-32767 reflects back in, amt 0..127 */
static inline int32_t wv_fold(int32_t x, int32_t amt)
{
    int32_t g = 256 + amt * 20, guard = 6;
    x = (x * g) >> 8;
    while (guard-- && (x > 32767 || x < -32767))
        x = x > 32767 ? 65534 - x : -65534 - x;
    return x;
}

static void wave_note_on(track_t *t, voice_t *v)
{
    uint32_t vi = (uint32_t)(v - t->v) % NVOICE;
    v->ph[0] = v->ph[1] = v->ph[2] = 0;
    v->s[0] = 0;                                 /* DRIFT's wobble */
    v->s[1] = (int32_t)(0x6D2B79F5u ^ vi * 0x9E3779B1u);   /* DRIFT's noise seed */
    v->s[2] = (int32_t)(0x85EBCA6Bu ^ vi * 0xC2B2AE35u);   /* NOISE's seed */
}

static void wave_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int8_t frame[WAVE_FLEN];
    int32_t sub_lv = p[P_E2] * 258, det_lv = p[P_E3] * 180, noise_lv = p[P_E6] * 180, warp = p[P_E4];
    uint32_t inc = m->inc, inc_sub = m->inc >> 1, inc_det, i;
    int32_t wave = p[P_E1] + ((m->envq15 >> 8) * p[P_E7] >> 7);
    if (n > CTL)
        n = CTL;
    wv_blend(frame, p[P_E0], wave);
    inc_det = inc + (uint32_t)mulq16((int32_t)inc, (uint32_t)p[P_E3] * 8u);
    v->s[0] = clamp(v->s[0] + (((int32_t)(noise32(&v->s[1]) >> 28) - 8) - (v->s[0] >> 6)), -4000, 4000);
    if (p[P_E5]) {                               /* DRIFT: a leaky random walk nudges the pitch, +- */
        int32_t d = (v->s[0] * p[P_E5]) >> 7, neg = d < 0, delta;
        delta = mulq16((int32_t)inc, (uint32_t)(neg ? -d : d));
        inc = (uint32_t)((int32_t)inc + (neg ? -delta : delta));
    }
    for (i = 0; i < n; i++) {
        int32_t s = wv_sample(frame, v->ph[0]);
        v->ph[0] += inc;
        if (sub_lv) {
            s += mulq15(wv_sample(frame, v->ph[1]), sub_lv);
            v->ph[1] += inc_sub;
        }
        if (det_lv) {
            s += mulq15(wv_sample(frame, v->ph[2]), det_lv);
            v->ph[2] += inc_det;
        }
        if (noise_lv)
            s += mulq15((int32_t)(noise32(&v->s[2]) >> 17) - 16384, noise_lv);
        if (warp)
            s = wv_fold(s, warp);
        out[i] += voice_amp2(s >> 1, m, i);
    }
}

static const preset_t WAVE_PRESETS[] = {
    /* name, {TABLE WAVE SUB DETUNE WARP DRIFT NOISE ENVWAVE}, {A D S R}, fenv, mono */
    {"DIGI PAD", {36, 70, 30, 25, 0, 15, 8, 40}, {0, 50, 100, 60}, 0, 0, FX(0, 20, 20, 60), TAG(PT_PAD)},
    {"CHIP LEAD", {109, 40, 0, 10, 15, 0, 0, 70}, {0, 20, 100, 20}, 0, 1, FX(10, 0, 10, 15), TAG(PT_LEAD)},
    {"VOX MORPH", {36, 10, 0, 20, 0, 10, 5, 90}, {10, 40, 110, 50}, 0, 0, FX(0, 15, 15, 45), TAG(PT_PAD)},
    {"BELL PLUCK", {54, 30, 0, 0, 10, 0, 0, 100}, {0, 70, 0, 30}, 0, 0, FX(0, 10, 25, 35), TAG(PT_PLUCK)},
    {"FOLD BASS", {72, 20, 80, 15, 60, 5, 0, 20}, {0, 30, 110, 20}, 0, 1, FX(15, 0, 0, 10), TAG(PT_BASS)},
    {"NOISE DRONE", {91, 50, 20, 40, 0, 50, 60, 0}, {80, 60, 127, 100}, 0, 0, FX(0, 10, 30, 70), TAG(PT_FX)},
    {"ORGAN KEYS", {127, 80, 20, 10, 0, 5, 0, 0}, {0, 20, 127, 25}, 0, 0, FX(0, 0, 0, 25), TAG(PT_KEYS)},
    {"HARMONIC SWEEP", {0, 0, 0, 20, 0, 10, 0, 60}, {5, 80, 100, 70}, 0, 0, FX(0, 15, 20, 45), TAG(PT_LEAD)},
};

static const engine_t ENG_WAVE = {
    .name = "WAVE",
    .page_title = {"TABLE", "SHAPE"},
    .edit = {
        {"TABLE", 0, 0, 127, 0, F_PCT, 0},
        {"WAVE", 0, 0, 127, 0, F_PCT, 0},
        {"SUB", 0, 0, 127, 0, F_PCT, 0},
        {"DETUNE", 0, 0, 127, 0, F_PCT, 0},
        {"WARP", 0, 0, 127, 0, F_PCT, 0},
        {"DRIFT", 0, 0, 127, 0, F_PCT, 0},
        {"NOISE", 0, 0, 127, 0, F_PCT, 0},
        {"ENV>WAVE", 0, 0, 127, 0, F_PCT, 0},
    },
    .presets = WAVE_PRESETS,
    .npresets = NELEM(WAVE_PRESETS),
    .note_on = wave_note_on,
    .render = wave_render,
    .knob = {P_E0, P_E1, P_E4, P_E5},
};
