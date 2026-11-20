#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""WAVE's wavetables (run by tools/build.py generate()): our own synthesis, no third-party material
(as tools/gen_waves.py's sample library).

  gen_wavetables.py OUT.h

8 tables of 16 single-cycle frames, 64 samples each, 8-bit (int8_t): a PPG-style table, deliberately
small and a little lo-fi (8 x 16 x 64 = 8192 samples, stored in about 4 KB: see compact storage below) rather than the smooth, long tables a
desktop softsynth would use - that is WAVE's character, not a shortcut. A frame is generated at
f = 0..15 sweeping from simple to complex (or tonal to noisy); src/eng_wave.c's WAVE engine blends
the four frames around a continuous (TABLE, WAVE) position at render time (bilinear, in Q8).

Tables, in WAVE_TBL order: HARMONIC (sine building a sawtooth, one more harmonic a frame), SQUARE
(pulse width 50% -> ~6%), FORMANT (three vowel-ish partial clusters, A E I O U), BELL (two-operator
FM, growing ratio and index), FOLD (a sine folded harder each frame), NOISE (comb-filtered noise,
tonal to hiss), CHIP (a saw crushed to fewer levels each frame), ORGAN (odd-harmonic drawbar stack,
hollow to full).
"""
import math
import random
import sys
from pathlib import Path

NFRAME = 16
FLEN = 64


def norm(s, peak=127):
    m = max(1e-9, max(abs(v) for v in s))
    return [int(round(v * peak / m)) for v in s]


def harmonic(f):
    h = min(f + 1, FLEN // 2 - 1)
    return norm([sum(math.sin(2 * math.pi * k * i / FLEN) / k for k in range(1, h + 1)) for i in range(FLEN)])


def square(f):
    duty = 0.5 - f / 15 * 0.42                      # 50 % .. 8 %
    on = max(1, int(round(duty * FLEN)))
    return norm([1.0 if (i % FLEN) < on else -1.0 for i in range(FLEN)])


def formant(f):
    # five vowel-ish formant pairs (relative partial number, amplitude), A E I O U; f sweeps across them
    vowels = [[(1, 1.0), (3, 0.7), (9, 0.3)], [(2, 1.0), (7, 0.6), (12, 0.2)], [(1, 1.0), (10, 0.8), (16, 0.3)],
              [(1, 1.0), (4, 0.7), (8, 0.25)], [(1, 1.0), (2, 0.8), (6, 0.2)]]
    t = f / 15 * (len(vowels) - 1)
    a, b, m = vowels[int(t)], vowels[min(int(t) + 1, len(vowels) - 1)], t - int(t)
    parts = [(ka, va + (vb - va) * m) for (ka, va), (kb, vb) in zip(a, b)]
    return norm([sum(amp * math.sin(2 * math.pi * k * i / FLEN) for k, amp in parts) for i in range(FLEN)])


def bell(f):
    ratio = 1.4 + f * 0.18
    index = 0.4 + f * 0.5
    return norm([math.sin(2 * math.pi * i / FLEN + index * math.sin(2 * math.pi * ratio * i / FLEN))
                 for i in range(FLEN)])


def fold(f):
    gain = 1.0 + f * 1.3
    out = []
    for i in range(FLEN):
        x = gain * math.sin(2 * math.pi * i / FLEN)
        for _ in range(8):                           # reflect back into -1..1
            if x > 1:
                x = 2 - x
            elif x < -1:
                x = -2 - x
            else:
                break
        out.append(x)
    return norm(out)


def noise_tbl(f):
    rnd = random.Random(1000 + f)                    # fixed seed: deterministic, reproducible builds
    npart = max(2, 24 - f)                            # fewer, wider partials as f grows: tonal -> hiss
    parts = [(rnd.randint(1, FLEN // 2 - 1), rnd.uniform(0.3, 1.0), rnd.uniform(0, 2 * math.pi))
             for _ in range(npart)]
    return norm([sum(a * math.sin(2 * math.pi * k * i / FLEN + ph) for k, a, ph in parts) for i in range(FLEN)])


def chip(f):
    levels = max(2, 32 - f * 2)
    raw = [2 * (i / FLEN) - 1 for i in range(FLEN)]   # a bare saw
    return norm([round((v + 1) / 2 * (levels - 1)) / (levels - 1) * 2 - 1 for v in raw])


def organ(f):
    bars = [1, 3, 5, 7, 9]                            # odd harmonics: a hollow, clarinet-like stack
    h = min(1 + f // 3, len(bars))
    return norm([sum(math.sin(2 * math.pi * k * i / FLEN) / k for k in bars[:h]) for i in range(FLEN)])


TABLES = [("HARMONIC", harmonic), ("SQUARE", square), ("FORMANT", formant), ("BELL", bell), ("FOLD", fold),
          ("NOISE", noise_tbl), ("CHIP", chip), ("ORGAN", organ)]


# ---- compact storage (lossless: every frame the firmware expands is bit-identical to the tables above) ----
# WK_FULL   64 samples stored.
# WK_ODD    a sum of sines (x[-i] == -x[i], x[0] == x[32] == 0): samples 1..31 stored, the rest mirrored.
# WK_SQUARE a pulse, +127 for the first on samples, else -127: only on stored (16 bytes for the table).
# WK_CHIP   a saw in 32 - 2 f levels (min 2): no storage, wv_frame() works it out (same integer rounding as below).
WK_FULL, WK_ODD, WK_SQUARE, WK_CHIP = 0, 1, 2, 3
KINDS = {"HARMONIC": WK_ODD, "SQUARE": WK_SQUARE, "FORMANT": WK_ODD, "BELL": WK_FULL, "FOLD": WK_ODD,
         "NOISE": WK_FULL, "CHIP": WK_CHIP, "ORGAN": WK_ODD}


def square_on(frame):
    on = sum(1 for v in frame if v > 0)
    assert frame == [127 if i < on else -127 for i in range(FLEN)], "SQUARE is not a pulse"
    return on


def chip_frame(f):
    """what src/eng_wave.c wv_frame() computes for CHIP, in integers (round half to even, as Python's round)"""
    lv1 = max(2, 32 - f * 2) - 1
    out = []
    for i in range(FLEN):
        n = i * lv1
        k = (n + 32) >> 6
        if (n & 63) == 32 and (k & 1):
            k -= 1
        v = (2 * k - lv1) * 127
        out.append((v + (lv1 // 2 if v >= 0 else -(lv1 // 2))) // lv1 if v >= 0 else -((-v + lv1 // 2) // lv1))
    return out


def check_odd(frame):
    assert frame[0] == 0 and frame[32] == 0 and all(frame[FLEN - i] == -frame[i] for i in range(1, 32)), \
        "not odd-symmetric"


def main(path):
    rows = [[gen(f) for f in range(NFRAME)] for _, gen in TABLES]
    full, odd, sq = [], [], []
    idx = []
    for (name, _), frames in zip(TABLES, rows):
        kind = KINDS[name]
        if kind == WK_FULL:
            idx.append(len(full))
            full.append((name, frames))
        elif kind == WK_ODD:
            for fr in frames:
                check_odd(fr)
            idx.append(len(odd))
            odd.append((name, frames))
        elif kind == WK_SQUARE:
            idx.append(0)
            sq = [square_on(fr) for fr in frames]
        else:
            assert all(chip_frame(f) == frames[f] for f in range(NFRAME)), "CHIP's integer form differs"
            idx.append(0)
    out = ["/* generated by tools/gen_wavetables.py: WAVE's wavetables (src/eng_wave.c wv_frame() expands them) */",
           "#pragma once", "#include <stdint.h>", "", f"#define WAVE_NTAB {len(TABLES)}u",
           f"#define WAVE_NFRAME {NFRAME}u", f"#define WAVE_FLEN {FLEN}u",
           f"#define WK_FULL {WK_FULL}u   /* 64 samples stored */",
           f"#define WK_ODD {WK_ODD}u    /* samples 1..31 stored, mirrored with the sign flipped */",
           f"#define WK_SQUARE {WK_SQUARE}u /* a pulse: its length stored */",
           f"#define WK_CHIP {WK_CHIP}u   /* a crushed saw: computed */",
           "/* " + ", ".join(f"{i} {n}" for i, (n, _) in enumerate(TABLES)) + " */",
           "static const uint8_t WAVE_KIND[WAVE_NTAB] = {" + ", ".join(str(KINDS[n]) for n, _ in TABLES) + "};",
           "static const uint8_t WAVE_SLOT[WAVE_NTAB] = {" + ", ".join(str(i) for i in idx) + "};",
           f"static const uint8_t WAVE_SQON[WAVE_NFRAME] = {{{', '.join(str(v) for v in sq)}}};",
           f"static const int8_t WAVE_FULL[{len(full)}][WAVE_NFRAME][WAVE_FLEN] = {{"]
    for name, frames in full:
        out.append(f"    {{   /* {name} */")
        out += ["        {" + ", ".join(str(v) for v in fr) + "}," for fr in frames]
        out.append("    },")
    out += ["};", f"static const int8_t WAVE_ODD[{len(odd)}][WAVE_NFRAME][31] = {{"]
    for name, frames in odd:
        out.append(f"    {{   /* {name} */")
        out += ["        {" + ", ".join(str(v) for v in fr[1:32]) + "}," for fr in frames]
        out.append("    },")
    out += ["};", ""]
    Path(path).write_text("\n".join(out))
    nbytes = len(full) * NFRAME * FLEN + len(odd) * NFRAME * 31 + NFRAME
    print(f"wavetables: {len(TABLES)} tables x {NFRAME} frames x {FLEN} samples, "
          f"{nbytes} B stored (of {len(TABLES) * NFRAME * FLEN}) -> {path}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: gen_wavetables.py OUT.h")
    main(sys.argv[1])
