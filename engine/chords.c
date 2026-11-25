/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* CHORD PADS (card #208; frasil_engine.h says what they do): the chords of HARMONY's key by degree and type, their
 * words, and the scene moved into one. Included last by frasil_engine.c (FRS_CHORDS 1); chords.h, before core.c,
 * renamed the DSP's note calls to the twins below, which call the DSP's own: so every note the brain plays (the drones',
 * GLINT's, the bass's, the LEAD's) passes here on its way to the sound, the brain itself as it was (its notes the
 * scene's own: a scene stored has them, never the move).
 *   The drones while the move holds them: the notes the brain holds there (held_by) go on silent, the chord sounds
 *   instead (cm.snd: the drones' notes as the DSP has them); given back, what the brain holds then sounds again.
 *   GLINT, the bass, the LEAD: a note moved as it is struck (cm_map); GLINT's and the LEAD's let go as struck (their
 *   notes as struck kept by the note as it came); the bass's sounding note retuned at each move.
 * Part of the unity build: core.c's statics (held_by, bass_note, grid, st) in sight. */
#undef frasil_note_on
#undef frasil_note_off
#undef frasil_all_off
#undef frasil_pressure
#undef frasil_glint_on
#undef frasil_glint_off
#undef frasil_glint_pressure
#undef frasil_bass_on
#undef frasil_bass_retune
#if FRS_LEAD
#undef frasil_lead_on
#undef frasil_lead_poly_on
#undef frasil_lead_poly_off
#undef frasil_lead_pressure
#endif

/* ---------------------------------------------------------------- chords --- */
#define CM_FIXED 7u                      /* the types from MAJOR on: semitones over the degree's note, not scale steps */
#define CM_NMAX 5u                       /* a chord's notes at most (a 9TH's) */
static const char *const CM_TYPE[FRS_CM_TYPES] = {"TRIAD", "6TH", "7TH", "9TH", "SUS2", "SUS4", "ADD9", "MAJOR",
                                                  "MINOR", "DOM 7"};
static const int8_t CM_SHAPE[FRS_CM_TYPES][CM_NMAX] = {
    {0, 2, 4, -1, -1}, {0, 2, 4, 5, -1}, {0, 2, 4, 6, -1}, {0, 2, 4, 6, 8}, {0, 1, 4, -1, -1}, {0, 3, 4, -1, -1},
    {0, 2, 4, 8, -1},                    /* steps up the scale */
    {0, 4, 7, -1, -1}, {0, 3, 7, -1, -1}, {0, 4, 7, 10, -1},   /* semitones */
};
static const uint8_t CM_DIATONIC[FRS_CM_TYPES] = {0, 1, 2, 3, 4, 5, 6, 0, 0, 2};   /* (a fixed type: the one up the scale
                                                                                    * it alters, TRIAD's or 7TH's) */

/* the scale the chords are built on, seven notes: a pentatonic's mode (MAJOR's, MINOR's), CHROMATIC on MAJOR's */
static uint16_t cm_bits(uint32_t scale)
{
    scale %= NSCALES;
    return SCALE_BITS[scale == 0u || scale == 10u ? 1u : scale == 11u ? 2u : scale];
}

/* the chord's notes, low to high, its root at base .. base + 11: how many (0: no such degree or type) */
static uint32_t cm_build(uint32_t root, uint32_t scale, uint32_t degree, uint32_t type, int base, int *out)
{
    int r = (int)(root % 12u), rn;
    uint16_t bits = cm_bits(scale);
    uint32_t i, k = 0;
    if (degree > 6u || type >= FRS_CM_TYPES)
        return 0;
    rn = scale_up(r, (int)degree, r, bits);
    rn = base + ((rn - base) % 12 + 12) % 12;
    for (i = 0; i < CM_NMAX && CM_SHAPE[type][i] >= 0; i++)
        out[k++] = type < CM_FIXED ? scale_up(rn, CM_SHAPE[type][i], r, bits) : rn + CM_SHAPE[type][i];
    return k;
}

uint32_t frs_chord_notes(uint32_t root, uint32_t scale, uint32_t degree, uint32_t type, int8_t *out)
{
    int n[CM_NMAX];
    uint32_t i, k = cm_build(root, scale, degree, type, 60, n);
    for (i = 0; i < k; i++)
        out[i] = (int8_t)n[i];
    return k;
}

const char *frs_chord_type_name(uint32_t type) { return type < FRS_CM_TYPES ? CM_TYPE[type] : ""; }

/* ----------------------------------------------------------------- words --- */
static const char CM_LETTER[7] = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};
static const uint8_t CM_NAT[7] = {0, 2, 4, 5, 7, 9, 11};

static int cm_acc(int pc, int letter)    /* pc as that letter: its sharps (+) or flats (-) */
{
    int a = ((pc - CM_NAT[letter]) % 12 + 12) % 12;
    return a > 6 ? a - 12 : a;
}

/* the key's letters: its tonic's (of the two a black key could be, the one whose seven notes need the fewest sharps and
 * flats; even, the flat), each degree the next letter */
static int cm_tonic_letter(int r, uint16_t bits)
{
    int c, best = 0, score = 1000;
    for (c = 0; c < 7; c++) {
        int a = cm_acc(r, c), n = 0, i, note = r;
        if (a < -1 || a > 1)
            continue;
        for (i = 0; i < 7; i++) {
            int x = cm_acc(note % 12, (c + i) % 7);
            n += x < 0 ? -x : x;
            note = scale_up(note, 1, r, bits);
        }
        n = 2 * n + (a > 0);
        if (n < score) {
            score = n;
            best = c;
        }
    }
    return best;
}

static char *cm_put(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    *p = 0;
    return p;
}

/* the quality after the root's name (from the chord's semitones over its root, and its type) */
static void cm_quality(uint32_t type, const int *n, uint32_t k, char *q)
{
    uint32_t rb = 0, i;
    int M, m, P, b5, s5, x;
    const char *tri;
    char *p;
    for (i = 0; i < k; i++)
        rb |= 1u << ((n[i] - n[0]) % 12);
    M = rb >> 4 & 1u, m = (rb >> 3 & 1u) && !M, P = rb >> 7 & 1u, b5 = (rb >> 6 & 1u) && !P, s5 = (rb >> 8 & 1u) && !P;
    tri = M && P ? "" : m && P ? "m" : m && b5 ? "dim" : M && s5 ? "aug" : M && b5 ? "(b5)" : m && s5 ? "m(#5)" : "";
    *q = 0;
    if (type == 4u || type == 5u) {      /* SUS2, SUS4 */
        cm_put(cm_put(q, type == 4u ? "sus2" : "sus4"), b5 ? "(b5)" : "");
        return;
    }
    x = k > 3u ? (n[3] - n[0]) % 12 : -1;   /* (the fourth note: a 6TH's sixth, a 7TH's seventh, ADD9's ninth) */
    if (type == 1u) {                    /* 6TH */
        cm_put(cm_put(q, tri), x == 9 ? "6" : "(b6)");
        return;
    }
    if (type == 6u) {                    /* ADD9 */
        cm_put(cm_put(q, tri), x == 2 ? (*tri ? "(add9)" : "add9") : "(addb9)");
        return;
    }
    if (type != 2u && type != 3u && type != 9u) {   /* TRIAD, MAJOR, MINOR */
        cm_put(q, tri);
        return;
    }
    if (M && P)
        p = cm_put(q, x == 11 ? "maj7" : "7");
    else if (m && P)
        p = cm_put(q, x == 11 ? "m(maj7)" : "m7");
    else if (m && b5)
        p = cm_put(q, x == 9 ? "dim7" : "m7b5");
    else if (M && s5)
        p = cm_put(q, x == 11 ? "maj7#5" : "7#5");
    else
        p = cm_put(cm_put(q, tri), x == 11 ? "maj7" : "7");
    if (type == 3u && k > 4u) {          /* 9TH: the 7 a 9, else its ninth after it */
        int nine = (n[4] - n[0]) % 12;
        if (nine == 2) {
            for (p = q; *p && *p != '7'; p++)
                ;
            if (*p)
                *p = '9';
        } else {
            cm_put(p, nine == 1 ? "b9" : "#9");
        }
    }
}

void frs_chord_name(uint32_t root, uint32_t scale, uint32_t degree, uint32_t type, char *name, char *numeral)
{
    static const char *const RN[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    static const char *const rn[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    int n[CM_NMAX], r = (int)(root % 12u), lt, a, M, m, dim, aug;
    uint16_t bits = cm_bits(scale);
    uint32_t k = cm_build(root, scale, degree, type, 60, n), rb = 0, i;
    char q[16], *p;
    if (name)
        *name = 0;
    if (numeral)
        *numeral = 0;
    if (!k)
        return;
    for (i = 0; i < k; i++)
        rb |= 1u << ((n[i] - n[0]) % 12);
    M = rb >> 4 & 1u, m = (rb >> 3 & 1u) && !M;
    dim = m && (rb >> 6 & 1u) && !(rb >> 7 & 1u);
    aug = M && (rb >> 8 & 1u) && !(rb >> 7 & 1u);
    cm_quality(type, n, k, q);
    if (name) {                          /* the root as the key spells it, then the quality */
        lt = (cm_tonic_letter(r, bits) + (int)degree) % 7;
        a = cm_acc(n[0] % 12, lt);
        p = name;
        *p++ = CM_LETTER[lt];
        for (; a > 0 && a <= 2; a--)
            *p++ = '#';
        for (; a < 0 && a >= -2; a++)
            *p++ = 'b';
        *p = 0;
        cm_put(p, q);
    }
    if (numeral) {                       /* the degree, lower-case minor or diminished, then its figure */
        static const char *const FIG[FRS_CM_TYPES] = {"", "6", "7", "9", "sus2", "sus4", "add9", "", "", "7"};
        p = cm_put(numeral, (m || dim) ? rn[degree] : RN[degree]);
        p = cm_put(p, dim ? "\xC2\xB0" : aug ? "+" : "");
        cm_put(p, FIG[type]);
    }
}

void frs_chord_spell(uint32_t root, uint32_t scale, uint32_t degree, uint32_t type, char *out)
{
    int n[CM_NMAX], lt, a;
    uint16_t bits = cm_bits(scale);
    uint32_t k = cm_build(root, scale, degree, type, 60, n), i, d = type < CM_FIXED ? type : CM_DIATONIC[type];
    char *p = out;
    *p = 0;
    if (!k)
        return;
    lt = cm_tonic_letter((int)(root % 12u), bits) + (int)degree;
    for (i = 0; i < k; i++) {            /* (each its letter by its step up the scale: a fixed type's as the one it alters) */
        int l = (lt + CM_SHAPE[d][i]) % 7;
        if (i)
            *p++ = ' ';
        *p++ = CM_LETTER[l];
        for (a = cm_acc(n[i] % 12, l); a > 0 && a <= 2; a--)
            *p++ = '#';
        for (a = cm_acc(n[i] % 12, l); a < 0 && a >= -2; a++)
            *p++ = 'b';
    }
    *p = 0;
}

/* ------------------------------------------------------------------ move --- */
static const uint32_t CM_MORPH_MS[9] = {0, 1000, 2000, 4000, 8000, 15000, 30000, 60000, 120000};   /* (MORPH's:
                                                                                                    * core.c's) */
static uint32_t cm_follow = FRS_CM_ALL;  /* the tracks that follow (frs_chord_follow) */
static struct {
    uint8_t on, degree, type;            /* a move in effect, its chord */
    int8_t shift;                        /* the steps along the scale the tracks that follow move (-3 .. 3) */
    int8_t alt[12];                      /* then a pitch class moved a semitone or two (the chord's own, out of the
                                          * scale) */
    uint8_t hold;                        /* the drones its: what the brain holds there silent, the chord sounding */
    int8_t center;                       /* their root nearest it (their lowest note when it took them) */
    int8_t chord[8];                     /* the chord as they voice it */
    uint8_t n;
    uint32_t snd[4];                     /* the drones' notes sounding while it holds them (bit n % 32 of [n / 32]) */
    int8_t g_off[16], g_on[16];          /* the glide: a voice let go and one struck at each of its steps (-1 none) */
    uint8_t g_n, g_done;
    uint32_t g_t0, g_dur;
    uint8_t wait, w_degree, w_type;      /* a move waiting for the grid's next beat */
    int32_t w_beat;
    uint32_t w_ms;
    float bvel;                          /* the bass's last velocity (a retune keeps it) */
    uint8_t gsnd[128], lsnd[128];        /* GLINT's and the LEAD's notes as struck, by the note as it came, + 1 (0: as
                                          * it came) */
} cm;

static void cm_zero(void)
{
    memset(&cm, 0, sizeof cm);
}

static void cm_init(void)                /* (frs_init: no move, every track following) */
{
    cm_zero();
    cm_follow = FRS_CM_ALL;
}

/* a note n moved d steps along the scale (r, bits: the scale's note at or below it moved, what it was above that kept) */
static int cm_shift(int n, int d, int r, uint16_t bits)
{
    int base = n;
    while (!in_scale(base, r, bits))
        base--;
    n -= base;
    while (d > 0) {
        base++;
        d -= in_scale(base, r, bits);
    }
    while (d < 0) {
        base--;
        d += in_scale(base, r, bits);
    }
    return base + n;
}

/* a note as track t (FRS_CM_*) plays it now */
static int cm_map(uint32_t t, int note);
static int cm_beat(void) { return (int)(grid.tick * (1.0f / 24.0f)); }
static void cm_apply(uint32_t degree, uint32_t type, uint32_t dur);
static void cm_due(void)                 /* a move waiting: there once the grid's beat came (or 4 s, no clock) */
{
    if (cm.wait && (cm_beat() != cm.w_beat || eng_ms() - cm.w_ms >= 4000u))
        cm_apply(cm.w_degree, cm.w_type, 0);
}

static int cm_map(uint32_t t, int note)
{
    int r, n;
    if (cm.wait)
        cm_due();                        /* (a step on the beat: struck in the chord it waited for) */
    if (!cm.on || !(cm_follow & t) || note < 0 || note > 127)
        return note;
    r = (int)(st.v[P_ROOT] % 12u);
    n = cm_shift(note, cm.shift, r, cm_bits(st.v[P_SCALE]));
    n += cm.alt[(n % 12 + 12) % 12];
    return n < 0 ? 0 : n > 127 ? 127 : n;
}

/* the alterations: a fixed type's notes against the same degree's up the scale (MAJOR's third against TRIAD's) */
static void cm_alter(void)
{
    int a[CM_NMAX], b[CM_NMAX];
    uint32_t i, k;
    memset(cm.alt, 0, sizeof cm.alt);
    if (cm.type < CM_FIXED)
        return;
    k = cm_build(st.v[P_ROOT], st.v[P_SCALE], cm.degree, cm.type, 60, a);
    if (cm_build(st.v[P_ROOT], st.v[P_SCALE], cm.degree, CM_DIATONIC[cm.type], 60, b) != k)
        return;
    for (i = 0; i < k; i++)
        if (a[i] != b[i] && a[i] - b[i] >= -2 && a[i] - b[i] <= 2)
            cm.alt[b[i] % 12] = (int8_t)(a[i] - b[i]);
}

/* ---- the drones ---- */
static void cm_voice(void)               /* the chord as the drones voice it: its root nearest their center, VOICING on it */
{
    int out[CM_NMAX], vo = st.v[P_VOICING] % 3u, t;
    uint32_t k = cm_build(st.v[P_ROOT], st.v[P_SCALE], cm.degree, cm.type, cm.center - 5, out), i, j, n = 0;
    if (vo == 1 && k >= 3u)
        out[1] += 12;
    else if (vo == 2 && k >= 2u)
        out[0] -= 12;
    for (i = 1; i < k; i++)              /* (low to high) */
        for (j = i; j > 0 && out[j - 1u] > out[j]; j--) {
            t = out[j];
            out[j] = out[j - 1u];
            out[j - 1u] = t;
        }
    for (i = 0; i < k; i++)
        if (out[i] >= 1 && out[i] <= 126 && (!n || cm.chord[n - 1u] != out[i]))
            cm.chord[n++] = (int8_t)out[i];
    cm.n = (uint8_t)n;
}

static void cm_take(void)                /* the drones the move's: what sounds there now is where it glides from */
{
    uint32_t n;
    int low = -1;
    memset(cm.snd, 0, sizeof cm.snd);
    for (n = 0; n < 128u; n++)
        if (held_by[n]) {
            bit_put(cm.snd, (int)n, 1);
            if (low < 0)
                low = (int)n;
        }
    cm.center = (int8_t)(low >= 0 ? low : 48 + (int)(st.v[P_ROOT] % 12u));
    cm.hold = 1;
    cm.g_n = cm.g_done = 0;
}

static void cm_step(uint32_t j)          /* one step of the glide: a voice let go, one struck */
{
    if (cm.g_off[j] >= 0) {
        frasil_note_off(cm.g_off[j]);
        bit_put(cm.snd, cm.g_off[j], 0);
    }
    if (cm.g_on[j] >= 0) {
        frasil_note_on(cm.g_on[j], 100.0f * (1.0f / 127.0f));   /* (at 100, as a scene's chords) */
        bit_put(cm.snd, cm.g_on[j], 1);
    }
}

static void cm_glide_run(void)
{
    uint32_t now = eng_ms();
    while (cm.g_done < cm.g_n &&
           (uint64_t)(now - cm.g_t0) * cm.g_n >= (uint64_t)cm.g_dur * cm.g_done)
        cm_step(cm.g_done++);
}

/* from the drones' notes sounding to the chord's: the notes both have stay; the others, the lowest first, a voice let
 * go and one struck together, the steps spread over dur (0: all at once) */
static void cm_glide(uint32_t dur)
{
    uint32_t want[4] = {0, 0, 0, 0}, i, n, nr = 0, nn = 0;
    for (i = 0; i < cm.n; i++)
        bit_put(want, cm.chord[i], 1);
    for (n = 0; n < 128u; n++) {
        int s = bit_get(cm.snd, (int)n), w = bit_get(want, (int)n);
        if (s && !w) {
            if (nr < 16u) {
                cm.g_off[nr++] = (int8_t)n;
            } else {                     /* (more than 16 sounding: the rest let go at once) */
                frasil_note_off((int)n);
                bit_put(cm.snd, (int)n, 0);
            }
        } else if (w && !s && nn < 16u) {
            cm.g_on[nn++] = (int8_t)n;
        }
    }
    cm.g_n = (uint8_t)(nr > nn ? nr : nn);
    for (i = nr; i < cm.g_n; i++)
        cm.g_off[i] = -1;
    for (i = nn; i < cm.g_n; i++)
        cm.g_on[i] = -1;
    cm.g_done = 0;
    cm.g_t0 = eng_ms();
    cm.g_dur = dur;
    cm_glide_run();
}

static void cm_give(void)                /* the drones given back: what the brain holds there sounds again */
{
    uint32_t n;
    if (!cm.hold)
        return;
    cm.hold = 0;
    cm.g_n = cm.g_done = 0;
    for (n = 0; n < 128u; n++) {
        int s = bit_get(cm.snd, (int)n), h = held_by[n] != 0;
        if (s && !h)
            frasil_note_off((int)n);
        else if (h && !s)
            frasil_note_on((int)n, 100.0f * (1.0f / 127.0f));
    }
    memset(cm.snd, 0, sizeof cm.snd);
}

/* ---- the bass ---- */
static void cm_bass_now(void)            /* its note sounding where it plays now (gliding as GLIDE does) */
{
    if (bass_note >= 0)
        frasil_bass_retune(cm_map(FRS_CM_BASS, bass_note), cm.bvel);
}

/* ---- the move ---- */
static void cm_apply(uint32_t degree, uint32_t type, uint32_t dur)
{
    cm.wait = 0;
    cm.on = 1;
    cm.degree = (uint8_t)degree;
    cm.type = (uint8_t)type;
    cm.shift = (int8_t)(degree <= 3u ? (int)degree : (int)degree - 7);
    cm_alter();
    if (cm_follow & FRS_CM_BASS)
        cm_bass_now();
    if (cm_follow & FRS_CM_DRONE) {
        if (!cm.hold)
            cm_take();
        cm_voice();
        cm_glide(dur);
    }
}

void frs_chord_go(uint32_t degree, uint32_t type, int on_beat)
{
    if (degree > 6u || type >= FRS_CM_TYPES)
        return;
    if (on_beat) {
        cm.wait = 1;
        cm.w_degree = (uint8_t)degree;
        cm.w_type = (uint8_t)type;
        cm.w_beat = cm_beat();
        cm.w_ms = eng_ms();
        return;
    }
    cm_apply(degree, type, CM_MORPH_MS[st.v[P_MORPH] % 9u]);
}

void frs_chord_follow(uint32_t tracks)
{
    uint32_t was = cm_follow;
    cm_follow = tracks & FRS_CM_ALL;
    if (!cm.on)
        return;
    if ((was ^ cm_follow) & FRS_CM_BASS)
        cm_bass_now();
    if ((was ^ cm_follow) & FRS_CM_DRONE) {
        if (cm_follow & FRS_CM_DRONE) {
            cm_take();
            cm_voice();
            cm_glide(0);
        } else {
            cm_give();
        }
    }
}

void frs_chord_home(void)
{
    cm.wait = 0;
    if (!cm.on)
        return;
    cm_give();
    cm.on = 0;
    if (cm_follow & FRS_CM_BASS)
        cm_bass_now();
}

int frs_chord_note(uint32_t track, int note) { return cm_map(track, note); }

static void cm_drop(void)                /* (tr_reset's) */
{
    cm.wait = 0;
    cm_give();
    cm.on = 0;
}

static void cm_tick(void)
{
    if (cm.wait)
        cm_due();
    if (cm.g_done < cm.g_n)
        cm_glide_run();
}

static void cm_view(frs_view_t *v)
{
    v->cm_on = cm.on;
    v->cm_degree = cm.degree;
    v->cm_type = cm.type;
    v->cm_follow = (uint8_t)cm_follow;
    v->cm_wait = cm.wait;
    v->cm_degree_next = cm.w_degree;
    v->cm_type_next = cm.w_type;
    v->cm_hold = cm.hold;
    memcpy(v->cm_notes, cm.chord, sizeof v->cm_notes);
    v->cm_n = cm.n;
    v->cm_glide = cm.g_done >= cm.g_n || !cm.g_dur ? 1.0f
                  : eng_ms() - cm.g_t0 >= cm.g_dur ? 1.0f : (float)(eng_ms() - cm.g_t0) / (float)cm.g_dur;
    {                                    /* (cm_map's on C4's octave: the same in every one) */
        int pc, r = (int)(st.v[P_ROOT] % 12u), n;
        uint16_t bits = cm_bits(st.v[P_SCALE]);
        for (pc = 0; pc < 12; pc++) {
            n = cm.on ? cm_shift(60 + pc, cm.shift, r, bits) : 60 + pc;
            v->cm_off[pc] = (int8_t)(cm.on ? n + cm.alt[n % 12] - (60 + pc) : 0);
        }
    }
}

/* the running state's (frasil_engine.c alone_lv: the engine alone's part) */
static void cm_lv(rs_io *s)
{
    V32(cm_follow);
    V8(cm.on);
    V8(cm.degree);
    V8(cm.type);
    V8(cm.shift);
    VB(cm.alt);
    V8(cm.hold);
    V8(cm.center);
    VB(cm.chord);
    V8(cm.n);
    VU32S(cm.snd);
    VB(cm.g_off);
    VB(cm.g_on);
    V8(cm.g_n);
    V8(cm.g_done);
    V32(cm.g_t0);
    V32(cm.g_dur);
    V8(cm.wait);
    V8(cm.w_degree);
    V8(cm.w_type);
    V32(cm.w_beat);
    V32(cm.w_ms);
    VF(cm.bvel);
    VB(cm.gsnd);
    VB(cm.lsnd);
    if (cm.n > 8u || cm.g_n > 16u || cm.g_done > cm.g_n || cm.degree > 6u || cm.type >= FRS_CM_TYPES ||
        cm.w_degree > 6u || cm.w_type >= FRS_CM_TYPES)
        s->bad = 1;
}

/* ------------------------------------------------- the DSP's, through it --- */
static void cm_note_on(int note, float vel)
{
    if (!cm.hold)
        frasil_note_on(note, vel);
}

static void cm_note_off(int note)
{
    if (!cm.hold)
        frasil_note_off(note);
}

static void cm_pressure(int note, float p)
{
    if (!cm.hold)
        frasil_pressure(note, p);
}

static void cm_all_off(void)             /* every drone note let go: the move's too, the drones given back */
{
    frasil_all_off();
    cm.hold = 0;
    cm.g_n = cm.g_done = 0;
    memset(cm.snd, 0, sizeof cm.snd);
}

static void cm_glint_on(int note, float vel)
{
    int s = cm_map(FRS_CM_GLINT, note);
    if (note >= 0 && note < 128)
        cm.gsnd[note] = (uint8_t)(s + 1);
    frasil_glint_on(s, vel);
}

static void cm_glint_off(int note)
{
    int s = note;
    if (note < 0) {
        memset(cm.gsnd, 0, sizeof cm.gsnd);
    } else if (note < 128 && cm.gsnd[note]) {
        s = cm.gsnd[note] - 1;
        cm.gsnd[note] = 0;
    }
    frasil_glint_off(s);
}

static void cm_glint_pressure(int note, float p)
{
    frasil_glint_pressure(note >= 0 && note < 128 && cm.gsnd[note] ? cm.gsnd[note] - 1 : note, p);
}

static void cm_bass_on(int note, float vel)
{
    cm.bvel = vel;
    frasil_bass_on(cm_map(FRS_CM_BASS, note), vel);
}

static void cm_bass_retune(int note, float vel)
{
    cm.bvel = vel;
    frasil_bass_retune(cm_map(FRS_CM_BASS, note), vel);
}

#if FRS_LEAD
static void cm_lead_on(int note, float vel, int legato)
{
    int s = cm_map(FRS_CM_LEAD, note);
    if (note >= 0 && note < 128)
        cm.lsnd[note] = (uint8_t)(s + 1);
    frasil_lead_on(s, vel, legato);
}

static void cm_lead_poly_on(int note, float vel)
{
    int s = cm_map(FRS_CM_LEAD, note);
    if (note >= 0 && note < 128)
        cm.lsnd[note] = (uint8_t)(s + 1);
    frasil_lead_poly_on(s, vel);
}

static void cm_lead_poly_off(int note)
{
    int s = note;
    if (note >= 0 && note < 128 && cm.lsnd[note]) {
        s = cm.lsnd[note] - 1;
        cm.lsnd[note] = 0;
    }
    frasil_lead_poly_off(s);
}

static void cm_lead_pressure(int note, float p)
{
    frasil_lead_pressure(note >= 0 && note < 128 && cm.lsnd[note] ? cm.lsnd[note] - 1 : note, p);
}
#endif
