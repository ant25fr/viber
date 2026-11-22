/* SPDX-License-Identifier: GPL-3.0-only
 * Skiff: STYLE BLOCKS' header, knob card and dialog (DESIGN.md, direction B). Included by ui_draw.c; draw_head,
 * draw_column and draw_confirm draw through them. */

/* the header: the transport left, the state right. Left: the play state (stop, play, the song's disc; counting in, play
 * in the accent) or, a track armed, the REC mark in its place (filled: the selected track; outlined: another); the
 * selected track's bar.beat (16 of its steps a bar, 4 a beat, as its step pages count them; stopped, 1.1 in MID: PLAY
 * starts there); the BPM (M, the accent while it is being changed) and "BPM" (S) on its baseline. Right: a message or
 * a layer's label from HB_MSG_X or past the BPM (the octave and the battery give way); else the octave or the song
 * row (head_group, shortened or dropped when the title leaves no room before the BPM: #70) and the battery. No
 * metronome, no BPM LOCK mark (a turn says BPM LOCKED), no rolling digits. The selected track is
 * the footer's (its cushion before the engine). Redrawn when a beat moves while playing. */
#define HB_POS_X 22                                   /* bar.beat's pen */
#define HB_BPM_X 52                                   /* the BPM's pen */
#define HB_MSG_X 104                                  /* a message from here: 132 px to the edge (Viber: IBM Plex Mono, 6 px a character: 22) */
/* (Skiff phase 5) before the title, in the selected track's colour: on a SEQ page that edits the pattern, which one:
 * the entry the track is on, the pattern's number, how many entries name it when more than one ("C \xB7 06 \xB7 2x",
 * the mockup's; b: 16 bytes); ((2.0, #44) on a chain: its entries, the part's number: "A-B \xB7 02", the join
 * sheet's). 0: nothing to say. (the clip launcher) the CLIPS page and its BAR n OF N went with the song's
 * auto-advance */

static int head_mark(char *b)
{
    const page_t *pg = cur_page();
    uint32_t k = song.sel % NTRK, id = trk_pat(k), e = bank_cur(k), n = 0, t, j, b0, b1;
    b[0] = 0;
    if (ui.home || pg->fam != FAM_SEQ)
        return 0;
    if (pg->graph == GR_SONG)
        return 0;                                  /* (the clip launcher) CLIPS: the grid says where each track is */
    if (id == PAT_NONE)
        return 0;
    if (e < NENT && ent_block(k, e, &b0, &b1)) {
        str_cpy(b, "A-B \xB7 ", 16);
        b[0] = (char)('A' + b0);
        b[2] = (char)('A' + b1);
        pat_num(b + str_len(b), id);
        return 1;
    }
    if (e < NENT) {
        b[0] = (char)('A' + e);
        str_cpy(b + 1, " \xB7 ", 15);
    }
    pat_num(b + str_len(b), id);
    for (t = 0; t < NTRK; t++)
        for (j = 0; j < NENT; j++)
            n += lists.ent[t][j][0] == id + 1u;
    if (n > 1u) {
        str_cpy(b + str_len(b), " \xB7 ", 16 - str_len(b));
        fmt_int(b + str_len(b), (int32_t)n);
        str_cpy(b + str_len(b), "x", 16 - str_len(b));
    }
    return 1;
}
/* (2.2, YouTube) a mark that does not fit, shorter: its count goes ("B \xB7 02 \xB7 2x" -> "B \xB7 02"), then its
 * entry ("02"); 0 when nothing is left to take (it went before: some marks said nothing where they were long) */
static int head_mark_short(char *b)
{
    uint32_t n = str_len(b), i;
    if (n && b[n - 1u] == 'x') {
        for (i = n - 1u; i > 2u && b[i] != ' '; i--)
            ;
        if (i > 2u) {
            b[i - 2u] = 0;                             /* (" \xB7 " before the count) */
            return 1;
        }
    }
    for (i = 0; i + 2u < n; i++)
        if ((uint8_t)b[i + 1u] == 0xB7u && b[i] == ' ') {
            str_cpy(b, b + i + 3u, 16);                /* (the entry and its " \xB7 ") */
            return 1;
        }
    return 0;
}
/* the title at the header's right: SHIFT's map; (2.5) a FREE TAKE's picker; HOME: the project's name (pn); the page's */
static const char *head_title(const char *pn)
{
    return ui.shift ? "SHIFT" : fr.on ? "FREE TAKE" : ui.home ? pn : page_head();
}
/* the master's level in the header, left of the battery: a 20 px bar, the loudest of the last 512 samples the output made
 * (scope_buf), 60 dB over it, rising at once and falling a pixel a frame; 0 .. MTR_W. Taken once a frame, by draw_head */
#define MTR_W 20
#define MTR_H 6
#define MTR_GAP 4                                     /* between the bar and the battery's cell, and before the title */
static int32_t head_meter(void)
{
    static int32_t held;
    uint32_t i;
    int32_t pk = 0, lg = 0, v, px;
    for (i = 0; i < SCOPE_N; i++) {
        int32_t a = scope_buf[i];
        a = a < 0 ? -a : a;
        if (a > pk)
            pk = a;
    }
    if (pk >= 64) {
        while ((pk >> lg) > 1)
            lg++;
        v = lg * 8 + (((pk << 3) >> lg) & 7);        /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB), as the mixer's meters */
        px = clamp((v - 48) * MTR_W / 80, 0, MTR_W);
    } else {
        px = 0;
    }
    held = px > held ? px : held > 0 ? held - 1 : 0;
    return held;
}
static void draw_head_blocks(void)
{
    const track_t *t = TSEL;
    uint32_t rec = (song.rec >> song.sel) & 1u ? 2u : song.rec != 0u, id, sig;
    int msg = ui.msg_t || ui.layer;
    char pos[12] = "1.1", b[8], pn[16], mk[16];      /* (stopped: PLAY starts from the top) */
    int32_t x, lx;
    uint16_t ic;
    int32_t mtr = head_meter();
    int mark = head_mark(mk);                        /* ((Skiff phase 5) SONG ON, the pattern edited) */
    project_cur_name(pn);                            /* (HOME's title) */
    if (song.playing) {                              /* (in time: REV, PING, RAND count up too) */
        fmt_int(pos, (int32_t)(seq_at(t) / 16u) + 1);
        str_cpy(pos + str_len(pos), ".", 2);
        fmt_int(pos + str_len(pos), (int32_t)(seq_at(t) % 16u / 4u) + 1);
    }
    fmt_int(b, song.g[G_BPM]);
    if (seq_counting())
        id = ICON_X_PLAY, ic = T_ACCENT;
    else if (rec)
        id = rec == 2u ? ICON_X_REC : ICON_X_REC_O, ic = T_REC;
    else
        id = song.playing ? ICON_X_PLAY : ICON_X_STOP, ic = song.playing ? T_THEME : T_MID;
    sig = str_hash(str_hash(id * 31u + ic, pos), b) + (ui.msg_t ? str_hash(7u, ui.msg) : ui.layer * 7919u +
          (uint32_t)layer_locked() * 3u + (ui.layer == LAYER_PERF ? launch_mode() * 131u : 0u)) + (ui.bpm_t != 0) * 31u +
          (uint32_t)(song.octave + 8) * 11u +
          (uint32_t)batt_shown() * 7777u + 0xB10C5u + (uint32_t)mtr * 40503u +
          str_hash(9u + (uint32_t)ui.home, head_title(pn)) +   /* (the title) */
          (mark ? str_hash(3u + song.sel, mk) : 0u) + project_mark() * 104729u;
    ui.roll_bpm = song.g[G_BPM];                      /* (no rolling digits: none left to finish in another style) */
    ui.roll[ROLL_BPM].from[0] = 0;
    if (!ui.force && sig == ui.head_sig)
        return;
    ui.head_sig = sig;
    cv_begin(240, H_HEAD, T_BG);
    cv_icon_mid(4, H_HEAD / 2, 16, id, ic, T_BG);
    GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
    cv_text(HB_POS_X, HEAD_MY, &AF_M, pos, song.playing ? T_TEXT : T_MID);
    GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
    x = cv_text(HB_BPM_X, HEAD_MY, &AF_M, b, ui.bpm_t ? T_ACCENT : T_THEME);
    GFX_HOOK_ALIGN(0, 0, 0, HEAD_MY + AF_M.asc, AL_B, "BPM on its value's baseline");
    x = cv_text(x + 3, HEAD_MY + AF_M.asc - AF_S.asc, &AF_S, "BPM", T_MID);
    lx = x;                                            /* (where the transport's half ends) */
    if (msg && !ui.msg_t) {                           /* a layer: its title at the right, as the mockup's PERFORM BAR, */
        char h[24];                                    /* in words ([GLO] SET: GLO SET); PERFORM: when a switch comes */
        if (ui.layer == LAYER_PERF) {                  /* in (LAUNCH); locked: the lock before it */
            str_cpy(h, "PERFORM ", sizeof h);
            str_cpy(h + 8, LAUNCH_NAME[launch_mode()], sizeof h - 8u);
        } else {
            kc_words(h, sizeof h, layer_head(), 0);
            if (layer_locked() && ui.layer != LAYER_FX) {   /* ((Skiff U2) locked: says so, as FX LATCH does; its
                                                             * button closes it, the footer says DONE) */
                char *sp = h;
                while (*sp && *sp != ' ')
                    sp++;
                str_cpy(sp, " LOCKED", sizeof h - (uint32_t)(sp - h));
            }
        }
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        x = cv_text_r(234, HEAD_SY, &AF_S, h, layer_locked() ? T_THEME : T_MID, T_BG);
        if (layer_locked())
            cv_icon_mid(x - 18, H_HEAD / 2, 16, ICON_X_LOCK, T_THEME, T_BG);
    } else if (msg) {
        x = x + 9 > HB_MSG_X ? x + 9 : HB_MSG_X;
        head_msg(x, 236 - x);
    } else {                                           /* the page's title at the right, as the mockup's (HOME: the */
        int32_t r = HEAD_BAT_X - 4 - MTR_W - MTR_GAP;                   /* project's name); the battery after it, at every level (#5), */
        char h[16];                                     /* the octave or the song row before it while they matter */
        draw_battery(HEAD_BAT_X);
        {   /* the master's level, left of the battery: a track, the level in THEME, its last fifth REC's */
            int32_t mx = HEAD_BAT_X - 4 - MTR_W, my = (H_HEAD - MTR_H) / 2, i;
            cv_rect(mx, my, MTR_W, MTR_H, T_SURF);
            for (i = 0; i < mtr; i++)
                cv_rect(mx + i, my, 1, MTR_H, i >= MTR_W * 4 / 5 ? T_REC : T_THEME);
        }
        str_cpy(h, head_title(pn), sizeof h);   /* ((U5) SHIFT's map) */
        x = r;
        if (h[0]) {
            GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
            x = cv_text_r(r, HEAD_SY, &AF_S, h, T_MID, T_BG) - 8;
        }
        if ((id = project_mark()) != 0u) {            /* ((Skiff U3, A2) the project: its letter in a box before the
                                                        * title, a dot on its corner while it changed since saved or
                                                        * loaded) */
            char pl[2] = {(char)(id & 0xFFu), 0};
            x -= 14;
            cv_frame(x, (H_HEAD - 15) / 2, 14, 15, T_MID);
            GFX_HOOK_ALIGN(x, 0, x + 14, H_HEAD, AL_HV, "header project letter in its box");
            cv_text_in(x, HEAD_SY, 14, &AF_S, pl, T_TEXT, T_BG);
            if (id >> 8)
                cv_rect(x + 11, (H_HEAD - 15) / 2 - 2, 5, 5, T_THEME);
            x -= 8;
        }
        while (mark && x - text_w(&AF_S, mk) - (song.octave ? text_w(&AF_S, "OCT") + 4 + text_w(&AF_M, "+3") + 8 : 0) <
               lx + 8)
            mark = head_mark_short(mk);                 /* ((2.2) shorter, not gone) */
        if (mark) {                                     /* ((Skiff phase 5) where it fits) */
            GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
            x = cv_text_r(x, HEAD_SY, &AF_S, mk, T_TRK(song.sel), T_BG) - 8;
        }
        if (song.octave)
            head_group(x, lx + 8);                     /* (#70) stay clear of the BPM */
    }
    cv_blit(0, Y_HEAD);
}

/* the knob card, Ableton's device view: a tile per knob, at the bottom of the screen. The label (S; M on MENU > LARGE's list
 * pages where it fits) top left and the unit (S, MID) top right on one line; under them a round knob: a ring of 270 degrees
 * open at the bottom, its value arc in the track's colour from the left to the knob's position, the value (V, then M, then S
 * where V lacks a glyph or the value is too wide) in the middle of it. A value that is no gauge (ratio < 0: a name, an
 * action's keycap) has no ring: it is set in the same place across the tile. The knob just turned: the whole tile the accent,
 * all on it INK. A value MOTION drives: the motion icon right on the label's row (a label that leaves no room for it: the
 * label in the theme colour); the parameters' own icons are left out, the value says it, but a track's cushion stays (GLO's
 * LEVEL cards: the card is that track's, its arc that track's colour). Values snap (draw_column): no rolling digits, as on
 * MENU > LARGE's tall cards. */
#define BK_L 4                                        /* the ink's margins in the tile: left, right */
#define BK_R 3
#define BK_LY 1                                       /* the label's line: S capitals rows 4..13 */
#define BK_GH 3                                       /* the bar of a value too wide for the knob: the tile's last 3 rows */
#define BK_GAP 3                                      /* the label's ink to the unit's */
#define KN_CX 28                                      /* the knob: its centre in the tile, */
#define KN_CY 28
#define KN_RO4 64                                     /* its outer radius, 16 px, and the ring's inner one, 13.5 px, in quarter pixels */
#define KN_RI4 54
#define KN_SWEEP 2160                                 /* 270 degrees, in eighths of a degree */
#define KN_START 1800                                 /* the arc starts at 225 degrees clockwise from 12 o'clock (down left) */
#define KN_INNER 23                                   /* the room for the value inside the ring */

static int face_has(const aafont_t *f, const char *s)   /* every glyph of s in a sparse face (V, L) */
{
    for (; *s; s++)
        if (glyph_at(f, fold(f, (uint8_t)*s)) < 0)
            return 0;
    return 1;
}

static uint32_t kn_isqrt(uint32_t v)                  /* floor of the square root */
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    for (; b; b >>= 2) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
    }
    return r;
}
/* the angle of (y, x), both >= 0, from the x axis in eighths of a degree, 0 .. 720 (atan to 0.2 degrees, no float) */
static int32_t kn_atan8(int32_t y, int32_t x)
{
    int32_t r;
    if (x <= 0 && y <= 0)
        return 0;
    if (x < y)
        return 720 - kn_atan8(x, y);
    r = y * 1024 / x;                                 /* 0 .. 1024 for x >= y */
    return (360 * r) / 1024 + (125 * r * (1024 - r)) / (1024 * 1024);
}
/* the angle of the pixel offset (dx, dy) (y down) clockwise from 12 o'clock, 0 .. 2880 eighths of a degree */
static int32_t kn_angle(int32_t dx, int32_t dy)
{
    if (dx >= 0 && dy < 0)
        return kn_atan8(dx, -dy);
    if (dx > 0 && dy >= 0)
        return 720 + kn_atan8(dy, dx);
    if (dx <= 0 && dy > 0)
        return 1440 + kn_atan8(-dx, dy);
    return 2160 + kn_atan8(-dy, -dx);
}
/* the ring, in quarter pixels; track: its colour (the whole 270 degrees), arc: the value's (the first ratio / 1000 of them) */
static void knob_ring(int32_t ratio, uint16_t fill, uint16_t track, uint16_t arc, uint16_t body)
{
    int32_t x, y, prog = ratio * KN_SWEEP / 1000;
    WEB_RING_DRAW(KN_CX, KN_CY + cv_oy, ratio, fill, track, arc, body);
    for (y = KN_CY - 18; y <= KN_CY + 18; y++)
        for (x = KN_CX - 18; x <= KN_CX + 18; x++) {
            int32_t dx4 = 4 * x + 2 - 4 * KN_CX, dy4 = 4 * y + 2 - 4 * KN_CY;
            int32_t d = (int32_t)kn_isqrt((uint32_t)(dx4 * dx4 + dy4 * dy4));
            int32_t co = KN_RO4 - d + 2, ci = d - KN_RI4 + 2, cov, off;
            uint16_t col;
            if (d > KN_RO4 + 2)
                continue;
            if (d < KN_RI4 - 2) {                      /* inside the ring: the knob's body */
                cv_pset(x, y, body);
                continue;
            }
            cov = co < ci ? co : ci;
            cov = cov > 4 ? 4 : cov;
            if (cov <= 0)
                continue;
            off = (kn_angle(dx4, dy4) - KN_START + 2880) % 2880;
            if (off > KN_SWEEP)                        /* the open bottom */
                continue;
            col = off <= prog ? arc : track;
            cv_pset(x, y, ux_mix(d < KN_RI4 ? body : fill, col, cov * 25));
        }
}

/* the pen y that centres face f's capitals' band on the knob's middle (not the ink: its rows are measured on the host only) */
static int32_t kn_ty(const aafont_t *f)
{
    if (f == &AF_V)
        return KN_CY - HALF_UP(AF_V_CAP_H) - AF_V_CAP_Y;
    if (f == &AF_M)
        return KN_CY - HALF_UP(AF_M_CAP_H) - AF_M_CAP_Y;
    return KN_CY - HALF_UP(AF_S_CAP_H) - AF_S_CAP_Y;
}

static void draw_column_blocks(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc,
                               int32_t ratio, uint32_t icon, int hot, int mot, int32_t kid, uint32_t kind)
{
    uint16_t fill = hot ? T_ACCENT : T_SURF;
    uint16_t body = hot ? fill : ux_mix(fill, T_BG, 45);
    uint32_t tk = icon - ICON_TRK < NTRK ? icon - ICON_TRK : song.sel;   /* whose card: a cushion's track, else the selected */
    int32_t room = COL_W - BK_L - BK_R, ub[4] = {0, 0, 0, 0}, ur = 0;
    int meter = ratio >= 0 && kid < 0 && val[0];
    int gauge = meter && ink_w(&AF_S, val) <= KN_INNER;   /* (a name too wide for the knob: its text across the tile, a bar under it) */
    cv_begin(COL_W, COL_H, T_BG);
    cv_rect(0, 0, COL_W, COL_H, fill);
    if (unit[0] && val[0] && kid < 0) {               /* the unit: top right, on the label's line, where the label leaves room */
        const aafont_t *lf = kind == LK_LABEL && text_w(&AF_M, label) <= room - 12 ? &AF_M : &AF_S;
        int32_t tw = text_w(lf, label) + (icon - ICON_TRK < NTRK ? ICON_CELL + ICON_GAP : 0);
        text_ink(&AF_S, unit, ub);
        ur = ub[2] - ub[0];
        if (tw + BK_GAP + ur <= room)
            cv_text_on(COL_W - BK_R - ur - ub[0], BK_LY, &AF_S, unit, hot ? T_INK : T_MID, fill);
        else
            ur = 0;
    }
    if (label[0]) {
        const aafont_t *lf = kind == LK_LABEL && text_w(&AF_M, label) <= room - (ur ? ur + BK_GAP : 0) ? &AF_M : &AF_S;
        int32_t ly = lf == &AF_M ? 0 : BK_LY, cy = ly + (lf == &AF_M ? AF_M_CAP_Y : AF_S_CAP_Y);
        int32_t ch = lf == &AF_M ? AF_M_CAP_H : AF_S_CAP_H, lx = BK_L, lw = room - (ur ? ur + BK_GAP : 0);
        uint16_t lc = hot ? T_INK : T_MID;
        if (icon - ICON_TRK < NTRK) {                 /* the track's cushion before the label (DIM: the track silent) */
            GFX_HOOK_ALIGN(0, cy, 0, cy + ch, AL_V, "card icon on its label's line");
            lx += cv_icon_in(BK_L, cy, 0, ch, ICON_CELL, icon, vc == T_DIM ? T_DIM : T_ACCENT, fill) + ICON_GAP;
            lw -= lx - BK_L;
        }
        if (mot && FELUCCA_ICONS && !ur && text_w(lf, label) + ICON_GAP + ICON_CELL <= lw) {
            GFX_HOOK_ALIGN(0, cy, 0, cy + ch, AL_V, "card icon on its label's line");
            cv_icon_in(COL_W - BK_R - ICON_CELL, cy, 0, ch, ICON_CELL, ICON_X_MOTION, hot ? T_INK : T_THEME, fill);
            lw -= ICON_GAP + ICON_CELL;
        } else if (mot && !hot) {
            lc = T_THEME;
        }
        cv_text_fit(lx, ly, lf, label, lc, fill, lw);
    }
    if (gauge)
        knob_ring(ratio, fill, hot ? ux_mix(fill, T_INK, 30) : T_DIM, hot ? T_INK : vc == T_DIM ? T_DIM : T_TRK(tk), body);
    if (meter && !gauge) {
        int32_t fx = ratio * COL_W / 1000;
        cv_rect(0, COL_H - BK_GH, fx < 3 ? 3 : fx, BK_GH, hot ? T_INK : vc == T_DIM ? T_DIM : T_TRK(tk));
    }
    if (kid >= 0) {                                   /* "[OCT+]": the keycap in the middle of the tile (on the hot tile
                                                       * inverted: INK, its word the tile's) */
        const char *kl = KC[(uint32_t)kid < KC_COUNT ? (uint32_t)kid : 0u].label;
        int32_t kb[4];
        text_ink(&AF_M, kl, kb);
        cv_text_on(HALF_UP(COL_W - (kb[2] - kb[0])) - kb[0], kn_ty(&AF_M), &AF_M, kl,
                   hot ? T_INK : vc == T_ACCENT || vc == T_DIM ? vc : T_THEME, fill);
    } else if (val[0]) {
        const aafont_t *vf = gauge ? &AF_M : &AF_V;     /* (in a knob: M, smaller than the tile's V) */
        int32_t vb[4], vr = gauge ? KN_INNER : room;
        uint16_t bg = gauge ? body : fill, fg = hot ? T_INK : vc;
        if ((vf == &AF_V && !face_has(vf, val)) || ink_w(vf, val) > vr)
            vf = &AF_M;
        if (vf == &AF_M && ink_w(vf, val) > vr)
            vf = &AF_S;
        text_ink(vf, val, vb);
        if (vf == &AF_S && vb[2] - vb[0] > vr) {      /* (a value always fits: the lint reports one cut) */
            cv_text_fit(HALF_UP(COL_W - vr), kn_ty(vf), vf, val, fg, bg, vr);
        } else {
            GFX_HOOK_ALIGN(0, KN_CY, COL_W, KN_CY, AL_HV, "card value in its knob");
            cv_text_on(HALF_UP(COL_W - (vb[2] - vb[0])) - vb[0], kn_ty(vf), vf, val, fg, bg);
        }
    }
    cv_blit((uint32_t)CARD_X(c), card_y());
}

/* the OCT- / OCT+ dialog (ui_draw.c draw_confirm draws through it): a SURF tile on the cleared page; the question (M,
 * S where M is too wide); what it is about (S, MID: its name, or a sequence's steps and notes); whether SAVE held
 * undoes it (else, in REC's colour, that it can't be undone); CANCEL filled, the look of the safe choice, and the
 * action outlined in REC's colour, each over its key (OCT- cancels, OCT+ does it: ui_input.c) */
#define BD_X 16                                       /* the tile: x 16..223, y 56..196 */
#define BD_Y 56
#define BD_W 208
#define BD_H 141
#define BD_BY 80                                      /* the buttons: 89 x 29, their keys under them */
#define BD_BW 89
#define BD_BH 29
static void confirm_text(char *a, char *b);            /* (ui_draw.c) */
static void draw_frame(void);
static void draw_columns(void);
static void draw_foot(void);
/* the page under the dialog, as the B mockup's: drawn again with every colour 70 % of the way to BG (the header stays
 * itself: REC shows over a dialog); the lint takes it as a fill, a backdrop never read */
static void confirm_backdrop(void)
{
    __typeof__(ux) keep = ux;
    uint16_t *c[] = {&ux.surf, &ux.text, &ux.theme, &ux.accent, &ux.mid, &ux.dim, &ux.line, &ux.sel, &ux.tint, &ux.ink,
                     &ux.rec, &ux.raise, &ux.key, &ux.lane, &ux.grid};
    uint32_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++)
        *c[i] = ux_mix(*c[i], ux.bg, 70);
    for (i = 0; i < UI_NTRK; i++)                     /* (every track's colour) */
        ux.trk[i] = ux_mix(ux.trk[i], ux.bg, 70);
    ux.gen++;                                         /* (the text ramps: the dimmed ones) */
    draw_frame();
    draw_columns();
    draw_graph();
    draw_foot();
    ux = keep;
    GFX_HOOK_BACKDROP(0, H_HEAD, 240, 240 - H_HEAD);
}
static void draw_confirm_blocks(void)
{
    static const char *const VERB[CF_COUNT] = {     /* by ui.confirm (ui.c CF_*) */
        [CF_CLEAR_SEQ] = "CLEAR", [CF_OVR_PROJ] = "OVERWRITE", [CF_OVR_USER] = "OVERWRITE",
        [CF_INIT_SOUND] = "INITIALIZE",
        [CF_CLEAR_MOTION] = "CLEAR", [CF_ERASE_USER] = "ERASE",
        [CF_ERASE_BANK] = "CLEAR", [CF_CLEAR_ENTRY] = "CLEAR", [CF_LOAD_PROJ] = "LOAD",
        [CF_STORE_SCENE] = "OVERWRITE", [CF_RESET_SET] = "RESET",
        [CF_ERASE_PROJ] = "DELETE", [CF_NEW_PROJ] = "NEW", [CF_EMPTY_BANK] = "EMPTY", [CF_CLEAR_UNUSED] = "CLEAR",
        [CF_CLEAR_SONG] = "CLEAR", [CF_CLEAR_TRKS] = "CLEAR"};   /* ((2.5, 3A) SAVE > PROJECT's KNOB 3) */
    static const uint8_t KEY[2] = {KC_OCTDN, KC_OCTUP};
    char a[24], b[40];
    const aafont_t *tf = &AF_M;
    uint32_t k = ui.confirm, i;
    int undoable = k == CF_CLEAR_SEQ || k == CF_INIT_SOUND || k == CF_CLEAR_MOTION || k == CF_CLEAR_SONG ||
                   k == CF_CLEAR_TRKS;              /* ((2.5) CLEAR TRACKS: not undone, its patterns kept: MID too) */
    const char *hint = k == CF_CLEAR_TRKS ? "the sounds stay" : undoable ? "hold SAVE: undo" : "can't be undone";   /* ((Skiff
                                                       * U4) the undo hint's words, in
                                                       * the explanations' sentence case) */
    confirm_text(a, b);
    if (k == CF_CLEAR_SEQ && !b[0]) {                 /* what goes: the steps, the notes among them */
        const track_t *t = &trk[ui.confirm_trk % NTRK];
        uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 0u, n = 0;
        for (i = 0; i < len && i < NSTEP; i++)
            n += (uint32_t)step_on(&t->step[i]);
        say_count(b, sizeof b, len, " STEP");
        say_count(b, sizeof b, n, " NOTE");
        lower_words(b);                               /* ("16 steps, 9 notes"; a name keeps its capitals) */
    }
    confirm_backdrop();
    ui.head_sig = ~0u;
    cv_begin(BD_W, BD_H, T_SURF);
    if (text_w(tf, a) > BD_W - 16)
        tf = &AF_S;
    GFX_HOOK_ALIGN(0, 0, BD_W, 0, AL_H, "dialog question centred");
    cv_text_in(0, 30 - tf->asc, BD_W, tf, a, T_TEXT, T_SURF);
    if (k == CF_CLEAR_ENTRY) {                        /* (Skiff phase 5) SONG: the pattern's fate, who uses it, the
                                                       * track that goes empty (MID), then "can't be undone" when the
                                                       * pattern goes: 1 .. 3 rows, as the mockup's */
        static const uint8_t ROW_Y[3][3] = {{57, 0, 0}, {50, 64, 0}, {44, 58, 72}};   /* (baselines, 14 apart: the
                                                                                          * descenders clear) */
        char l[3][32];
        int gone;
        uint32_t n = song_clear_text(ui.confirm_trk, l, &gone), rows = n + (uint32_t)gone;
        for (i = 0; i < rows && i < 3u; i++) {
            const char *s = i < n ? l[i] : hint;
            GFX_HOOK_ALIGN(0, 0, BD_W, 0, AL_H, "dialog detail centred");
            cv_text_in(0, ROW_Y[rows - 1u][i] - AF_S.asc, BD_W, &AF_S, s, i < n ? T_MID : T_REC, T_SURF);
        }
    } else {
        if (b[0]) {
            char f[40];
            text_fit(f, sizeof f, b, &AF_S, BD_W - 16);   /* a name: free text */
            GFX_HOOK_ALIGN(0, 0, BD_W, 0, AL_H, "dialog detail centred");
            cv_text_flags(ink_in(&AF_S, f, BD_W), 50 - AF_S.asc, &AF_S, f, T_MID, T_SURF, 8u | (f[str_len(f) - 1u] == ELLIPSIS));
        }
        /* (centred as the header centres a message: head_hint_w) */
        cv_free_hint(HALF_UP(BD_W - head_hint_w(hint, BD_W)), (b[0] ? 64 : 57) - AF_S.asc, hint, undoable ? T_MID : T_REC,
                     T_SURF, BD_W);                   /* (alone: where the two lines' middle is) */
    }
    for (i = 0; i < 2u; i++) {                        /* CANCEL (OCT-), the action (OCT+) */
        int32_t x = i ? BD_W - 12 - BD_BW : 12;
        const char *w = i ? VERB[k < CF_COUNT && VERB[k] ? k : CF_CLEAR_SEQ] : "CANCEL";
        const aafont_t *wf = text_w(&AF_M, w) <= BD_BW - 8 ? &AF_M : &AF_S;
        if (i)
            cv_frame(x, BD_BY, BD_BW, BD_BH, T_REC);
        else
            cv_rect(x, BD_BY, BD_BW, BD_BH, T_TEXT);
        GFX_HOOK_ALIGN(x, BD_BY, x + BD_BW, BD_BY + BD_BH, AL_HV, "dialog button word");
        cv_text_in(x, BD_BY + (wf == &AF_M ? CAP_IN(M, BD_BH) : CAP_IN(S, BD_BH)), BD_BW, wf, w, i ? T_REC : T_INK,
                   i ? T_SURF : T_TEXT);
        GFX_HOOK_ALIGN(x, 0, x + BD_BW, 0, AL_H, "dialog key under its button");
        cv_keycap(x + ink_in(&AF_S, KC[KEY[i]].label, BD_BW), BD_BY + BD_BH + 3, KEY[i], T_KEY, T_INK, T_SURF);   /* (its
                                                       * ink centred: CRYPT's spacing trails its letters) */
    }
    cv_blit(BD_X, BD_Y);
}
