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
