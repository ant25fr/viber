/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* WAVE's compact wavetable storage (tools/gen_wavetables.py, src/eng_wave.c wv_frame): every frame it expands, and the
 * blend of every (TABLE, WAVE) position, is bit-identical to the 8 KB tables the engine shipped with before they were
 * stored compactly (the FNV-1a 64 below were taken from those), and the storage is the size the generator promises.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define FNV0 1469598103934665603ULL
static uint64_t wt_fnv(uint64_t h, int8_t v) { return (h ^ (uint8_t)v) * 1099511628211ULL; }

int main(void)
{
    uint64_t hf = FNV0, hb = FNV0;
    int8_t buf[WAVE_FLEN], out[WAVE_FLEN];
    uint32_t t, f, i;
    int fails = 0;

    for (t = 0; t < WAVE_NTAB; t++)
        for (f = 0; f < WAVE_NFRAME; f++) {
            const int8_t *fr = wv_frame(buf, t, f);
            for (i = 0; i < WAVE_FLEN; i++) hf = wt_fnv(hf, fr[i]);
        }
    if (hf != 0x363ca7b751cfccd1ULL) { printf("FAIL: expanded frames differ from the original tables (%016llx)\n", (unsigned long long)hf); fails++; }
    for (t = 0; t < 128; t++)
        for (f = 0; f < 128; f++) {
            wv_blend(out, (int32_t)t, (int32_t)f);
            for (i = 0; i < WAVE_FLEN; i++) hb = wt_fnv(hb, out[i]);
        }
    if (hb != 0xde6713b812079d06ULL) { printf("FAIL: wv_blend differs from the original (%016llx)\n", (unsigned long long)hb); fails++; }
    if (sizeof WAVE_FULL + sizeof WAVE_ODD + sizeof WAVE_SQON > 4096u + 64u) {
        printf("FAIL: the wavetables take %u bytes\n", (unsigned)(sizeof WAVE_FULL + sizeof WAVE_ODD + sizeof WAVE_SQON)); fails++;
    }
    printf("wave tables: %u B stored for %u samples, %s\n", (unsigned)(sizeof WAVE_FULL + sizeof WAVE_ODD + sizeof WAVE_SQON),
           WAVE_NTAB * WAVE_NFRAME * WAVE_FLEN, fails ? "FAILED" : "all identical");
    return fails != 0;
}
