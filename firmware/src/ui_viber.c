/* SPDX-License-Identifier: GPL-3.0-only */
/* (Skiff) THEME VIBER's clip launcher (SEQ > CLIPS, ui_graph.c graph_song): Ableton Live's session grid, at Skiff's
 * scale. A clip keeps its track's colour whether it plays or not (never inverted, as BLOCKS inverts the picked or
 * playing one); its corners nubbed a px to read as rounded at this size. The picked entry: a crisp ring over its
 * colour, not a swapped fill. The launch state rides a small play glyph, bottom left, only on the entry that needs
 * one: ACCENT solid while it plays, blinking (ui.frame) while staged to come in at the next launch point. With any
 * other THEME graph_song draws as before (gi stays the right background behind: the chain box bx, else the row's).
 * Included by ui_graph.c. */

/* the 4 corners of a w x h tile at (x, y), one px each, painted `under`: a cheap rounded corner at this scale */
static void lv_round(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t under)
{
    cv_pset(x, y, under);
    cv_pset(x + w - 1, y, under);
    cv_pset(x, y + h - 1, under);
    cv_pset(x + w - 1, y + h - 1, under);
}

/* a small right-pointing play glyph, 3 px wide, 5 px tall, flat edge at x (left-aligned rows 1 2 3 2 1) */
static void lv_play(int32_t x, int32_t y, uint16_t c)
{
    static const uint8_t W[5] = {1, 2, 3, 2, 1};
    uint32_t r;
    for (r = 0; r < 5u; r++)
        cv_rect(x, y + (int32_t)r, W[r], 1, c);
}

/* Ableton's clip: a launch button on the clip's left (LV_BTN px wide, its colour a shade darker), the name beside it. The
 * glyph is always there: a dark play triangle while the clip is stopped, ACCENT solid while it plays, ACCENT blinking
 * (ui.frame) while staged. `c` is the clip's colour, `bg` the ground behind (the shade is mixed towards it). */
#define LV_BTN 8
static void lv_button(int32_t x, int32_t y, int32_t h, uint16_t c, uint16_t bg, int on, int staged, uint16_t ink)
{
    uint16_t gl = on ? T_ACCENT : staged ? ((ui.frame >> 2) & 1u ? T_ACCENT : ink) : ux_mix(c, bg, 70);
    cv_rect(x, y, LV_BTN - 1, h, ux_mix(c, bg, 22));
    lv_play(x + 2, y + (h - 5) / 2, gl);
}

/* Ableton's empty slot: a dark rounded cell with the little stop square where a clip's button would be. `under` is the
 * ground behind, `fill` the cell, `mk` the square */
static void lv_slot(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t fill, uint16_t under, uint16_t mk)
{
    cv_rect(x, y, w, h, fill);
    lv_round(x, y, w, h, under);
    cv_rect(x + 2, y + (h - 3) / 2, 3, 3, mk);
}

/* Ableton's progress on the playing clip: a 1 px line along its foot, from the button's edge to the playhead (the track's
 * step over its pattern's length), in ACCENT. `pos` and `len` are the step and the steps; stopped: nothing */
static void lv_progress(int32_t x, int32_t y, int32_t w, uint32_t pos, uint32_t len, uint16_t c)
{
    int32_t n = len ? (int32_t)((pos % len + 1u) * (uint32_t)w / len) : 0;
    cv_rect(x, y, n, 1, c);
}
