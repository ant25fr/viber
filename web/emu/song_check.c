/* SPDX-License-Identifier: GPL-3.0-only */
/* (the clip launcher) web/emu/felucca_web.c built natively (cc, as native_check.c): the device's own loop, then a
 * power cycle on the same flash.
 *   a: a grid for every track ((Skiff phase 6) six: LEN 16 12 8 16 12 16, REPS 1..3, empty entries between), the SONG
 *      byte on (an old project's: read as OFF), PLAY: each track loops its clip, whatever its REPS; an edit made in
 *      T1's A while playing sounds every round; the empty entries between are never reached by themselves. STOP,
 *      then the autosave's quiet stop writes the patterns and the project. The flash to FILE.
 *   b: a fresh device on FILE: the grid, each track on its entry with that entry's steps, the edit in T1's A.
 *   cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/song_check web/emu/song_check.c -lm
 *   build/emu/song_check a FILE && build/emu/song_check b FILE */
#include "felucca_web.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("song_check: %-90s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static void render(uint32_t ms)
{
    uint32_t n = ms * 441u / 10u, k;
    for (k = 0; k < n; k += 128u)
        web_render(n - k < 128u ? n - k : 128u);
}
static void tap(uint32_t b)
{
    web_buttons(1u << b);
    render(60);
    web_buttons(0);
    render(60);
}

static const uint8_t LEN[6] = {16, 12, 8, 16, 12, 16};                 /* ((Skiff phase 6) T5, T6 too) */
static const uint8_t REPS[6][4] = {{2, 1, 0, 1}, {1, 3, 0, 0}, {3, 0, 2, 0}, {1, 0, 0, 0}, {1, 2, 0, 0}, {2, 0, 0, 1}};
                                                                       /* (0: empty) */
_Static_assert(NTRK <= 6, "a list for every track");
static uint8_t note_of(uint32_t k, uint32_t e) { return (uint8_t)(48u + 12u * (k % 3u) + 2u * e); }
static uint32_t reps_of(uint32_t k, uint32_t e) { return e < 4u ? REPS[k][e] : 0u; }
static uint32_t first_of(uint32_t k) { uint32_t e = 0; while (e < 3u && !REPS[k][e]) e++; return e; }
static uint32_t last_of(uint32_t k) { uint32_t e = 3; while (e && !REPS[k][e]) e--; return e; }
static int rest_of(uint32_t k, uint32_t e)        /* ((1.2.1, #12) an empty entry between the track's first and last) */
{
    return !reps_of(k, e) && e > first_of(k) && e < last_of(k);
}
static uint32_t passes_of(uint32_t k, uint32_t e) { return rest_of(k, e) ? 1u : reps_of(k, e); }   /* (a rest: REPS 1) */
static uint32_t next_of(uint32_t k, uint32_t e)   /* the next entry, a pattern or a rest; after the last, the first */
{
    return e >= first_of(k) && e < last_of(k) ? e + 1u : first_of(k);
}
static int steps_are(uint32_t k, uint32_t e)      /* track k plays entry e's steps (fill) */
{
    return trk[k].step[0].n == 1u && trk[k].step[0].note[0] == note_of(k, e) && trk[k].step[4].note[0] == note_of(k, e) &&
           trk[k].p[P_SLEN] == LEN[k];
}
static int steps_at(uint32_t k, uint32_t e)       /* .. or a rest's: none, the track's LEN */
{
    return rest_of(k, e) ? !trk[k].step[0].n && !trk[k].step[4].n && trk[k].p[P_SLEN] == LEN[k] : steps_are(k, e);
}
static void fill(uint32_t k, uint32_t e)        /* a note on every fourth step of the pass */
{
    uint32_t i;
    uint8_t n = note_of(k, e);
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[k].step[i]);
    for (i = 0; i < LEN[k]; i += 4u)
        put_step(&trk[k], i, 1, &n, ST_NOTE, 0);
}

static int run_a(FILE *f)
{
    uint32_t k, e, t, moves[NTRK] = {0}, made = 1, edits = 0, rounds = 0;
    char b[96];
    web_nor_erase();
    web_boot();
    render(600);
    for (k = 0; k < NTRK; k++) {
        trk[k].p[P_SLEN] = LEN[k];
        for (e = 0; e < 4u; e++) {
            if (!REPS[k][e])
                continue;
            trk_pid[k] = 0;                        /* (off the pattern just made: these steps a new one) */
            fill(k, e);
            made &= bank_put(k, e, 0) == 0;
            lists.ent[k][e][1] = (uint8_t)(REPS[k][e] - 1u);
        }
        made &= ent_go(k, 0) == 0 && steps_are(k, 0);
    }
    lists.song = 1;                                /* (the byte of an old project: read as what it now is, OFF) */
    check("the grid made, each track on its A with A's steps", made);
    tap(B_PLAY);
    check("PLAY", song.playing);
    for (t = 0; t < 12000u; t += 10u) {           /* 12 s: several rounds of every track's clip */
        render(10);
        for (k = 0; k < NTRK; k++)
            moves[k] += (lists.cur[k] % NENT) != 0u;   /* (no track moves by itself, its REPS read by nothing) */
        if (!edits && t == 2000u) {                    /* T1's A, playing: a note put on its step 6 */
            uint8_t n = 72;
            put_step(&trk[0], 5, 1, &n, ST_NOTE, 0);
            edits = 1;
        } else if (edits && t % 4000u == 0u)
            rounds += trk[0].step[5].n == 1u && trk[0].step[5].note[0] == 72u && steps_are(0, 0);
    }
    for (k = 0; k < NTRK; k++) {
        snprintf(b, sizeof b, "T%u: loops its clip, REPS or not; the entries between (T4's one too) never reached", k + 1);
        check(b, moves[k] == 0u && steps_at(k, lists.cur[k] % NENT));
    }
    check("the edit made in T1's A while playing sounds every round (a loop, not a copy)", rounds >= 2u);
    tap(B_PLAY);
    check("STOP", !song.playing);
    render(16000);                                 /* the autosave's quiet stop: 2 s, then a round */
    check("the autosave's round: every pattern written, then the project", pend_free() == PEND_N && as.writes >= 1u &&
          as.saved == autosave_sig());
    for (k = 0; k < NTRK; k++)                     /* (where each stopped, for b) */
        fputc((int)lists.cur[k], f);
    fwrite(nor, 1, sizeof nor, f);
    return fails;
}

static int run_b(FILE *f)
{
    uint32_t k, e, ok = 1;
    uint8_t at[NTRK];
    if (fread(at, 1, NTRK, f) != NTRK || fread(nor, 1, sizeof nor, f) != sizeof nor)
        return 2;
    web_boot();
    render(600);
    check("a fresh device on that flash: the clip launcher, the old SONG byte read as OFF", lists.song == SONG_OFF);
    for (k = 0; k < NTRK; k++)
        for (e = 0; e < NENT; e++)
            ok &= (lists.ent[k][e][0] != 0u) == (reps_of(k, e) != 0u) &&
                  (!reps_of(k, e) || lists.ent[k][e][1] + 1u == reps_of(k, e));
    check("its lists: the same entries, the same REPS", ok);
    for (k = 0, ok = 1; k < NTRK; k++)
        ok &= lists.cur[k] == at[k] && trk_pid[k] == lists.ent[k][at[k]][0] && steps_at(k, at[k]);
    check("each track on the entry it stopped on, with that entry's steps", ok);
    ok = ent_go(0, 0) == 0 && steps_are(0, 0) && trk[0].step[5].n == 1u && trk[0].step[5].note[0] == 72u;
    check("T1's A with the edit made while playing, kept by the autosave", ok);
    return fails;
}

int main(int argc, char **argv)
{
    FILE *f;
    int rc;
    if (argc < 3 || (argv[1][0] != 'a' && argv[1][0] != 'b') || !(f = fopen(argv[2], argv[1][0] == 'a' ? "wb" : "rb")))
        return 2;
    rc = argv[1][0] == 'a' ? run_a(f) : run_b(f);
    fclose(f);
    return rc != 0;
}
