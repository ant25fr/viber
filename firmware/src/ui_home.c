/* SPDX-License-Identifier: GPL-3.0-only
 * Skiff: HOME's panel, the tracks at a glance (DESIGN.md rule 10). Included by ui_graph.c (draw_graph). ((Skiff U3, N1)
 * all six: the selected one at today's size, the others compact; phase 6 showed four with a scroll bar)
 * A lane per track: its number (never the colour alone), its sound's name over its engine's, and the bar its sequencer
 * is in: 16 cells in fours, a note or a hit filled in the track's colour, a tie half filled; the step playing marked.
 * STYLE BLOCKS (direction B): each lane a tile on BG, the number big in the track's colour, an empty step a block of
 * LINE, the step playing a line before its cell; the selected track a band of its colour, all on it INK (its empty
 * steps the colour half way to INK). Else in the panel: the number on its cushion, an empty step a stub, the step
 * playing a bar under its cell, the selected track a RAISE row. A muted track's cells DIM. HOME tapped on HOME shows
 * the scope instead (ui.home_scope). Redrawn only when what it shows changes (home_sig), not every other frame. */
#define HL_H 31                                       /* a lane: the B mockup's 31, 3 between them from the panel's top */
#define HL_NUM_X 9                                    /* the number's ink (BLOCKS), the cushion's cell */
#define HL_NAME_X 28                                  /* the sound's name (M) over its engine's (S), to the steps */
#define HL_NAME_CY 2                                  /* their capitals' tops in the lane: 2..13, 16..25 */
#define HL_ENG_CY 16
#define HL_STEP_X 100                                 /* the 16 cells: 6 px, 2 between, 2 more between the fours */
#define HL_CELL_W 6
#define HL_CELL_H 16
static void sound_name(const track_t *t, char *b);   /* (ui_draw.c) */
static int32_t hl_cell_x(uint32_t i) { return HL_STEP_X + (int32_t)i * (HL_CELL_W + 2) + (int32_t)(i / 4u) * 2; }

/* the bar track k shows: the one its sequencer is in (from its first step), and how many of its cells exist */
static uint32_t hl_bar(const track_t *t, uint32_t *n)
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 16u, first = (t->seq_idx % len) / 16u * 16u;
    *n = len - first > 16u ? 16u : len - first;
    return first;
}

/* what the overview shows: a change of it redraws the panel (draw_graph's signature) */
static uint32_t home_sig(void)
{
    uint32_t h = 2166136261u ^ song.sel * 7919u ^ (uint32_t)song.playing * 104729u ^ ux.gen * 131u, k, i, n;
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        const step_t *s = t->step;
        uint32_t first = hl_bar(t, &n);
        h = (h ^ (first + 977u * n + 7u * (uint32_t)t->p[P_MUTE] + 31u * t->eng_req + 257u * t->preset + 65537u * t->user +
                  (uint32_t)midi_mark(k) * 0x40000000u)) * 16777619u;
        if (song.playing)
            h = (h ^ (t->seq_idx + 1u)) * 16777619u;
        for (i = 0; i < n; i++)
            h = (h ^ (s[first + i].n | (uint32_t)s[first + i].hit << 8 | (uint32_t)s[first + i].time << 16)) * 16777619u;
    }
    return h;
}

/* (Skiff U3, the review's N1; design/mock_u3.py, the user's pick A) every track at once: the selected one's lane as
 * before (HL_H, its name over its engine's), the others compact (HL_C_H: the number in M, the name in S, the cells
 * HL_C_CELL_H); where all fit at today's size (four tracks) all as before */
#define HL_C_H 18
#define HL_C_CELL_H 12
static void graph_tracks(void)
{
    uint32_t k, i, n;
    int six = (int32_t)NTRK * (HL_H + 3) - 3 > (int32_t)graph_h();
    int32_t y = six ? ((int32_t)graph_h() - (HL_H + ((int32_t)NTRK - 1) * (HL_C_H + 3))) / 2 : 0;
    cv_rect(0, 0, 240, (int32_t)graph_h(), T_BG);      /* (over the panel: the lanes are its tiles) */
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        const step_t *s = t->step;
        uint32_t first = hl_bar(t, &n), play = t->seq_idx % 16u;
        int sel = k == song.sel, muted = t->p[P_MUTE] != 0, big = sel || !six;
        int32_t h = big ? HL_H : HL_C_H, ch = big ? HL_CELL_H : HL_C_CELL_H, b[4];
        int ib = sel;                                 /* (the selected lane a band, INK on it) */
        uint16_t col = muted ? T_DIM : T_TRK(k), lane = sel ? col : T_SURF;
        uint16_t ink = ib ? T_INK : T_TEXT, on = ib ? T_INK : col, off = ib ? ux_mix(col, T_INK, 55) : T_LINE;
        char name[20], d[2] = {(char)('1' + k), 0};
        cv_rect(3, y, 234, h, lane);
        text_ink(big ? &AF_V : &AF_M, d, b);             /* the number itself: the colour on SURF, INK on the band */
        GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "home lane number on its middle");
        cv_text_on(HL_NUM_X - b[0], y + (big ? CAP_IN(V, HL_H) : CAP_IN(M, HL_C_H)), big ? &AF_V : &AF_M, d,
                   ib ? T_INK : col, lane);
        if (midi_mark(k))                             /* ((Skiff U4, F4) MIDI IN playing it: a mark after the number) */
            cv_rect(HL_NUM_X + (big ? 14 : 10), y + h / 2 - 2, 4, 4, ib ? T_INK : T_THEME);
        sound_name(t, name);
        if (big) {
            cv_free_text(HL_NAME_X, y + HL_NAME_CY - AF_M_CAP_Y, &AF_M, name[0] ? name : "--", muted && !sel ? T_DIM : ink,
                         lane, HL_STEP_X - 4 - HL_NAME_X);
            cv_free_text(HL_NAME_X, y + HL_ENG_CY - AF_S_CAP_Y, &AF_S, ENGINES[t->eng_req % NENGINES]->name,
                         ib ? T_INK : T_MID, lane, HL_STEP_X - 4 - HL_NAME_X);
        } else {
            GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "home compact lane name on its middle");
            cv_free_text(HL_NAME_X, y + CAP_IN(S, HL_C_H), &AF_S, name[0] ? name : "--", muted ? T_DIM : T_TEXT, lane,
                         HL_STEP_X - 4 - HL_NAME_X);
        }
        for (i = 0; i < n; i++) {                     /* (past the track's length: nothing) */
            const step_t *st = &s[first + i];
            int32_t x = hl_cell_x(i), cy = y + (h - ch) / 2;
            if (step_on(st) || st->hit) {
                cv_rect(x, cy, HL_CELL_W, ch, on);
            } else {
                cv_rect(x, cy, HL_CELL_W, ch, off);
                if (step_tie(st))               /* a tie: the note held on, half the cell */
                    cv_rect(x, cy + ch / 4, HL_CELL_W, ch / 2, on);
            }
            if (song.playing && i == play && t->seq_idx / 16u * 16u == first)   /* the step playing: a line before it */
                cv_rect(x - 2, y + 2, 1, h - 4, sel ? T_INK : T_TEXT);
        }
        y += h + 3;
    }
}

/* ---- (Vibe 1.1) HOME's third panel, the arrangement (bank.c arr_*; ui_input.c arr_*): a row a track, the bars across,
 * ARR_VIS of them from ui.ar_win; a block of a pattern a tile in its track's colour (the pattern's number in it, DIM
 * when the track left the arrangement for a clip), the selected track's row raised; the cursor an outline of the block
 * OCT+ would put (the length KNOB 2 holds), its bar marked under the ruler's numbers (every 4th bar), the playhead a line */
#define AR_X0 20
#define AR_BW 9                                       /* 24 bars of 9 px: to the panel's right edge, 236 */
#define AR_RULER 12
#define AR_RH 19
static uint32_t arr_sig_view(void)
{
    return arr_sig() * 31u + ((uint32_t)ui.ar_bar | (uint32_t)ui.ar_pat << 8 | (uint32_t)ui.ar_len1 << 16 |
                              (uint32_t)ui.ar_win << 24) * 2654435761u + song.sel * 977u +
           (song.playing ? launch_steps + 1u : 0u) * 7919u + ux.gen * 131u;
}
static void graph_arr(void)
{
    uint32_t k, n, st, ln, pt, w0, ph = launch_steps / 16u;
    static uint32_t last_ph = 0xFFFFu;
    int32_t y, x1, x2, cx, cw, rows_h = 6 * (AR_RH + 1);
    char b[8];
    if (song.playing && ph != last_ph && (ph < ui.ar_win || ph >= (uint32_t)ui.ar_win + ARR_VIS))   /* (the playhead
                                                       * leaves what is shown: on to it, a page of four-bar steps) */
        ui.ar_win = (uint8_t)(ph / 4u * 4u > ARR_BARS - ARR_VIS ? ARR_BARS - ARR_VIS : ph / 4u * 4u);
    last_ph = ph;
    w0 = ui.ar_win;
    cv_rect(0, 0, 240, (int32_t)graph_h(), T_BG);
    for (k = 0; k < NTRK; k++) {                      /* the rows */
        int sel = k == song.sel;
        uint16_t col = T_TRK(k), bg = sel ? T_RAISE : T_SURF, dim = ux_mix(col, T_BG, 55);
        char d[2] = {(char)('1' + k), 0};
        y = AR_RULER + (int32_t)k * (AR_RH + 1);
        cv_rect(3, y, 234, AR_RH, bg);
        if (sel)
            cv_rect(3, y, 16, AR_RH, col);
        cv_text_on(3 + (16 - text_w(&AF_M, d)) / 2, y + CAP_IN(M, AR_RH), &AF_M, d, sel ? T_INK : col, sel ? col : bg);
        for (n = 0; n < ARR_VIS; n++)                 /* (the bar lines: the fours stronger) */
            cv_rect(AR_X0 + (int32_t)n * AR_BW, y, 1, AR_RH, (w0 + n) % 4u == 0 ? T_LINE : T_GRID);
        for (n = 0; arr_nth(k, n, &st, &ln, &pt); n++) {
            if (st + ln <= w0 || st >= w0 + ARR_VIS)
                continue;
            x1 = AR_X0 + ((int32_t)(st > w0 ? st : w0) - (int32_t)w0) * AR_BW;
            x2 = AR_X0 + ((int32_t)(st + ln < w0 + ARR_VIS ? st + ln : w0 + ARR_VIS) - (int32_t)w0) * AR_BW;
            cv_rect(x1, y + 1, x2 - x1 - 1, AR_RH - 2, arr_drives(k) ? col : dim);
            if (st >= w0 && x2 - x1 >= 15) {          /* (its pattern, where it starts and there is room) */
                pat_num(b, pt);
                cv_text_on(x1 + 2, y + CAP_IN(S, AR_RH), &AF_S, b, T_INK, arr_drives(k) ? col : dim);
            }
        }
    }
    for (n = 0; n < ARR_VIS; n += 4u) {               /* the ruler: the first bar of every four */
        fmt_int(b, (int32_t)(w0 + n) + 1);
        cv_text_on(AR_X0 + (int32_t)n * AR_BW + 2, CAP_IN(S, AR_RULER - 3), &AF_S, b, T_MID, T_BG);
    }
    if (ui.ar_bar >= w0 && ui.ar_bar < w0 + ARR_VIS) {   /* the cursor: its bar under the ruler, and the block's outline */
        cx = AR_X0 + ((int32_t)ui.ar_bar - (int32_t)w0) * AR_BW;
        cw = (int32_t)((uint32_t)ui.ar_len1 + 1u);
        if ((int32_t)ui.ar_bar + cw > (int32_t)(w0 + ARR_VIS))
            cw = (int32_t)(w0 + ARR_VIS) - (int32_t)ui.ar_bar;
        cw *= AR_BW;
        y = AR_RULER + (int32_t)song.sel * (AR_RH + 1);
        cv_rect(cx, AR_RULER - 3, AR_BW, 2, T_ACCENT);
        cv_rect(cx, y, cw, 1, T_ACCENT);
        cv_rect(cx, y + AR_RH - 1, cw, 1, T_ACCENT);
        cv_rect(cx, y, 1, AR_RH, T_ACCENT);
        cv_rect(cx + cw - 1, y, 1, AR_RH, T_ACCENT);
    }
    if (song.playing && launch_steps >= w0 * 16u && launch_steps < (w0 + ARR_VIS) * 16u)   /* the playhead */
        cv_rect(AR_X0 + (int32_t)((launch_steps - w0 * 16u) * AR_BW / 16u), AR_RULER, 1, rows_h, T_TEXT);
}
