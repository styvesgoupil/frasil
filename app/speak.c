/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL to a screen reader (fm1-reader: https://charlesvestal.github.io/fm1-reader/; fm1-base's device/announce.c
 * keeps the protocol): what the screen shows, as text. Nothing is built while no reader listens, and never in the
 * render: once a UI frame (app_frame), from what the screen knows already. The screen's own words, in its capitals;
 * the words file (words.tsv, published at frasilsynth.com/words/) says them in full.
 *   a reader new, or its WHERE: everything on the screen, at once
 *   the view changed (a page, the list of names, the help's page, the name, the logo, the screensaver): what it is
 *     and its four settings, once it has stayed 200 ms (pages flipped through are not read)
 *   an encoder turned: what it turned, its name then its value, once it rests 120 ms (ann_later; a long turn heard as
 *     it goes); a list's row with its place, "4 OF 20"
 *   a message (app_say) or a pop-up: at once (a warning heard whole: ANN_KEEP); the same message again while it shows
 *     (an LFO's AMOUNT turning): as a readout
 *   the octave (OCT- OCT+), COMPARE, the name's cursor and its character: as they change
 * The text goes into the pool (srd.t), never the system stack. app_speak_dump (the simulator's speakdump): every page,
 * value, row and help card as the reader gets them, for fm1-reader's audit (tools/screen_reader.sh).
 * Part of the unity build (app.c includes it, after ui.c). */

#define SPK_SETTLE_MS 200u               /* a view said once it has stayed this long */
#define SPK_TURN_MS 120u                 /* a value said once its knob rests this long */

static struct {
    char t[ANN_MAX + 1];                 /* the message being written */
    uint32_t n;                          /* its length */
    uint32_t flags;                      /* the first message's flags (NOW); those after it wait their turn */
    uint32_t view, view_at;              /* the view last said; when it changed (0: said) */
    uint32_t read_sig;                   /* the last readout, hashed: the same again is not said */
    uint32_t msg_sig, msg_at;            /* the last message's first line, hashed; when */
    uint8_t msg;                         /* a message said since the last frame: no readout over it */
    uint8_t part, all;                   /* the view's next part to say (0: none), and of everything (WHERE) or not */
    int8_t oct;                          /* the octave, as said (its value: a press undone as a hand is no change) */
    uint8_t cmp, nm_cur, nm_ch, sec;     /* COMPARE, the name's cursor and character, the TARGETs' section: as said */
} srd PLAT_POOL;                         /* (the pool: zeroed at the start) */

/* ------------------------------------------------------------ the text --- */
static uint32_t sp_hash(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h | 1u;
}

/* s onto the message as the reader can take it: 7-bit, no sign a voice reads out (a > before a target: none; & AND,
 * UTF-8's × TIMES) */
static void sp_add(const char *s)
{
    for (; s && *s && srd.n < ANN_MAX; s++) {
        const char *w = 0;
        uint8_t c = (uint8_t)*s;
        if (c == 0xC3u && (uint8_t)s[1] == 0x97u) {
            w = "TIMES ";
            s++;
        } else if (c == '&')
            w = "AND";
        else if (c == '>' || c == '<' || c >= 0x80u || c < 0x20u)
            continue;
        if (w) {
            while (*w && srd.n < ANN_MAX)
                srd.t[srd.n++] = *w++;
            continue;
        }
        if (c == ' ' && (srd.n == 0 || srd.t[srd.n - 1] == ' '))   /* (one space, none first) */
            continue;
        srd.t[srd.n++] = (char)c;
    }
    srd.t[srd.n] = 0;
}

static void sp_num(int32_t v)
{
    char d[12];
    int_text(v, d);
    sp_add(d);
}

/* a sentence ended: ". " (none twice, none after a colon's) */
static void sp_stop(void)
{
    while (srd.n && srd.t[srd.n - 1] == ' ')
        srd.n--;
    if (srd.n && srd.n < ANN_MAX && srd.t[srd.n - 1] != '.' && srd.t[srd.n - 1] != ':' && srd.t[srd.n - 1] != ',')
        srd.t[srd.n++] = '.';
    srd.t[srd.n] = 0;
    sp_add(" ");
}

static void sp_comma(void)
{
    while (srd.n && srd.t[srd.n - 1] == ' ')
        srd.n--;
    if (srd.n && srd.n < ANN_MAX && srd.t[srd.n - 1] != '.' && srd.t[srd.n - 1] != ',')
        srd.t[srd.n++] = ',';
    srd.t[srd.n] = 0;
    sp_add(" ");
}

static void sp_clear(void)
{
    srd.n = 0;
    srd.t[0] = 0;
}

/* the message out (nothing if empty): the first with srd.flags, then each after it in its turn */
static void sp_send(void)
{
    while (srd.n && (srd.t[srd.n - 1] == ' ' || srd.t[srd.n - 1] == ','))
        srd.t[--srd.n] = 0;
    if (srd.n) {
        ann_say(srd.t, 0, srd.flags);
        srd.flags &= ~ANN_NOW;
    }
    sp_clear();
}

/* ---------------------------------------------------------- the values --- */
/* a value as the screen writes it, put so that the words file can say it: a pan L50 C R50 (LEFT 50, CENTER, RIGHT 50),
 * a time past a minute 4:30 (4 MIN 30 S), a factor ×0.50 (0.50 TIMES) */
static void sp_text(int p, const char *v)
{
    uint32_t n = strlen_(v);
    if (p < (int)NPARAM && PD[p].fmt == F_PAN) {
        if (v[0] == 'C' && !v[1]) {
            sp_add("CENTER");
            return;
        }
        sp_add(v[0] == 'L' ? "LEFT " : "RIGHT ");
        sp_add(v + 1);
        return;
    }
    if (v[0] == '\xC3' && v[1] == '\x97') {
        sp_add(v + 2);
        sp_add(" TIMES");
        return;
    }
    if (n >= 4u && v[n - 3] == ':' && v[n - 1] >= '0' && v[n - 1] <= '9' && v[n - 4] >= '0' && v[n - 4] <= '9') {
        char m[8];
        uint32_t i;
        for (i = 0; i + 3u < n && i < 7u; i++)
            m[i] = v[i];
        m[i] = 0;
        sp_add(m);
        sp_add(" MIN");
        if (v[n - 2] != '0' || v[n - 1] != '0') {
            sp_add(" ");
            m[0] = v[n - 2] == '0' ? v[n - 1] : v[n - 2];
            m[1] = v[n - 2] == '0' ? 0 : v[n - 1];
            m[2] = 0;
            sp_add(m);
            sp_add(" S");
        }
        return;
    }
    sp_add(v[0] == '-' && !v[1] ? "NONE" : v);   /* (a step's LENGTH, UNDO TAKE: nothing) */
}

/* setting p's value x as the screen shows it (value_big's: a player's preset its name, TEMPO the clock's) */
static void sp_value(int p, uint32_t x, int now)
{
    char t[FRS_TEXT + 8];
    int k = list_track((uint32_t)p), un = now && k >= 0 ? up_in_use((uint32_t)k) : -1;
    if (un >= 0) {
        up_name((uint32_t)k, (uint32_t)un, t);
        sp_add(t);
        return;
    }
    if (now && p == P_TEMPO && ext_bpm()) {
        sp_num((int32_t)ext_bpm());
        sp_add(" BPM");
        return;
    }
    value_text(p, (uint8_t)x, t);
    sp_text(p, t);
}

/* a knob's name then its value: setting p (x its value; now: as it is, else x), or a page's own (K_ARP .. K_TUNE) */
static void sp_knob_at(uint32_t pg, uint32_t p, uint32_t x, int now)
{
    uint32_t at;
    if (p == K_ARP || p == K_SEQ) {      /* (its page's title says ARP or SEQ already) */
        sp_add(p == K_ARP ? "ARPEGGIO " : "SEQUENCER ");
        sp_add((now ? ways_on(PAGES[pg].track, p == K_ARP ? SQT_ARP : SQT_SEQ) : (int)x) ? "ON" : "OFF");
        return;
    }
    if (p == K_IDLE) {
        sp_add("IDLE TIME ");
        sp_num((int32_t)(now ? saver.min : x));
        sp_add(" MIN");
        return;
    }
    if (p == K_HEAD) {
        sp_add("HEADER ");
        sp_add((now ? hdr.dark : x) ? "DARK" : "INVERT");
        return;
    }
    if (p == K_TUNE) {
        sp_add("TUNE A4 ");
        sp_num((int32_t)(now ? a4.hz : x));
        sp_add(" HZ");
        return;
    }
    if (p >= NPARAM)
        return;
    if (p == P_TEMPO && now && ext_bpm())
        sp_add("EXTERNAL TEMPO");
    else if (pg == PG_CSEND && (p == P_ESPACE || p == P_GL_SPACE || p == P_BS_SPACE))   /* (its three, not the hands) */
        sp_add(p == P_ESPACE ? "DRONE SEND" : p == P_GL_SPACE ? "GLINT SEND" : "BASS SEND");
    else
        sp_add(p == P_DQ_ON ? "SEQUENCER" : PD[p].name);   /* (DRONE SEQ's ON: its title says SEQ) */
    sp_add(" ");
    at = srd.n;
    sp_value((int)p, now ? pv_shown((int)p) : x, now);
    if (text_is(srd.t + at, PD[p].name)) {   /* (a value that is its name: NOTES with no step before it) */
        srd.n = at ? at - 1u : 0u;
        srd.t[srd.n] = 0;
    }
    if (now && notes_why((int)p)) {      /* ("(ARP ONLY)": ARP ONLY) */
        char w[16];
        uint32_t i, k = 0;
        for (i = 0; notes_why((int)p)[i] && k < sizeof w - 1u; i++)
            if (notes_why((int)p)[i] != '(' && notes_why((int)p)[i] != ')')
                w[k++] = notes_why((int)p)[i];
        w[k] = 0;
        sp_comma();
        sp_add(w);
    }
}
static void sp_knob(uint32_t p) { sp_knob_at(fm.page, p, 0, 1); }

/* the three hands, A B C: the track, its preset, its filter */
static void sp_hands(void)
{
    sp_knob(P_KB);
    sp_comma();
    sp_knob(track_preset());
    sp_comma();
    sp_knob(track_algo());
}

/* ----------------------------------------------------------- the lists --- */
/* the list's first a no choice (OFF, NONE, CUSTOM): not counted */
static int sp_none(uint32_t p) { return p == P_DR_PRESET || p == P_EQ_PRESET || PD[p].fmt == F_LTGT; }

static int sp_section(uint32_t i)        /* an LFO TARGET's section at place i (TGT_HEAD), -1 none */
{
    uint32_t k;
    int s = -1;
    for (k = 0; k < NG_TPAGES * NG_PER; k++) {
        if (TGT_SLOTS[k] < TH_NONE)
            s = -TGT_SLOTS[k] - 2;
        else if (TGT_SLOTS[k] >= 0 && !i--)
            return TGT_SLOTS[k] ? s : -1;
    }
    return -1;
}

/* row i of setting p's list: its name, its place "3 OF 20" (a TARGET: its section first when sec) */
static void sp_row(uint32_t p, uint32_t i, int sec)
{
    char t[FRS_TEXT + 8];
    uint32_t n = list_n(p), none = (uint32_t)sp_none(p);
    if (sec && PD[p].fmt == F_LTGT && sp_section(i) >= 0) {
        sp_add(TGT_HEAD[sp_section(i)]);
        sp_add(": ");
    }
    list_name(p, i, t);
    sp_text((int)p, t);
    sp_comma();                          /* (its first, no choice: 0 OF n, as the screen counts) */
    sp_num((int32_t)(i + 1u - none));
    sp_add(" OF ");
    sp_num((int32_t)(n - none));
}

/* a scene's row on SCENES (scenes_list): its number and name, what it holds, NOW, its place among the seventeen */
static void sp_scene_row(uint32_t s)
{
    char nt[SC_LONG + 1];
    scene_t sc;
    uint32_t used = s < NSCENES && (scn.used >> s & 1u);
    scene_get(s, &sc);
    if (s < NSCENES) {
        sp_num((int32_t)(s + 1u));
        sp_add(" ");
    }
    if (used || s >= NSCENES || s < NFACTORY) {   /* (an empty slot: its number alone, not SCENE 10 again) */
        sp_add(scene_name(s, nt));
        sp_comma();
    }
    if (s == SC_NEW) {
        sp_add("START FRESH");
    } else if (!used) {
        sp_add(s < NFACTORY ? "FACTORY" : "EMPTY");
    } else if (sc.nch || sc.narp || sc.nbarp || sc.bass >= 0) {
        if (sc.nch) {
            sp_num(sc.nch);
            sp_add(sc.nch == 1u ? " DRONE " : " DRONES ");
        }
        if (sc.narp)
            sp_add("ARP ");
        if (sc.bass >= 0 || sc.nbarp)
            sp_add("BASS");
    } else {
        sp_add("SOUND");
    }
    if (s == fm.scene) {
        sp_comma();
        sp_add("NOW");
    }
    sp_comma();
    sp_num(s >= NSCENES ? 1 : (int32_t)s + 2);
    sp_add(" OF ");
    sp_num((int32_t)NSCENES + 1);
}

/* ----------------------------------------------------------- the views --- */
/* the page's title, as the header has it, and its place among its button's pages "(2/9)" (page_dots) */
static void sp_title(uint32_t pg)
{
    uint32_t p, n = 0, i = 0, o;
    sp_add(pg == PG_SON ? "SOUND" : pg == PG_SAVEAS || (pg == PG_SAVE && !sv.load) ? "SAVE" : PAGES[pg].name);
    for (o = 0; o < NPAGES; o++) {
        p = PAGE_ORDER[o];
        if (PAGES[p].button == PAGES[pg].button && !page_hidden(p)) {
            n++;
            if (p == pg)
                i = n;
        }
    }
    if (PAGES[pg].button == B_HOME) {    /* (its last, the logo) */
        n = 5;
        i = pg == PG_KEYS ? 2u : pg == PG_MIXER ? 3u : pg == PG_CSEND ? 4u : 1u;
    }
    if (n >= 2u && i) {
        sp_add(" (");
        sp_num((int32_t)i);
        sp_add("/");
        sp_num((int32_t)n);
        sp_add(")");
    }
}

/* SAVE's two pages: what K1 and K2 choose */
static void sp_saveas(int what, int into)
{
    static const char *const W[4] = {"SCENE", "DRONE PRESET", "GLINT PRESET", "BASS PRESET"};
    char t[SC_LONG + 8];
    if (what) {
        sp_add("WHAT: ");
        sp_add(W[sv.what & 3u]);
    }
    if (into) {
        if (what)
            sp_stop();
        sp_add("INTO: ");
        into_text(t);
        sp_add(t[0] == '0' && t[1] >= '0' && t[1] <= '9' ? t + 1 : t);   /* (03: 3) */
    }
}

/* a page: its title, then what is on it (SOUND: the scene and the three hands; SCENES: the slot picked; SAVE's: the
 * choices; else its four settings). all: and its MORPH, LOAD, how to go on */
static void sp_page(int all)
{
    uint32_t pg = fm.page, i;
    char nt[SC_LONG + 1];
    sp_title(pg);
    if (pg == PG_SON) {
        sp_comma();
        sp_add(scene_name(fm.scene, nt));
        if (st.dirty) {
            sp_comma();
            sp_add("CHANGED");
        }
        sp_stop();
        sp_hands();
        if (!all)
            return;
        sp_stop();
        for (i = 0; i < 4u; i++) {
            sp_knob(page_knob(pg, i));
            sp_comma();
        }
        return;
    }
    if (pg == PG_KEYS) {                 /* (the selected track's) */
        sp_comma();
        sp_add(st.glint ? "GLINT" : st.bass ? "BASS" : "DRONE");
    }
    if (pg == PG_SCENES && st.dirty) {
        sp_comma();
        sp_add("CHANGED");
    }
    sp_stop();
    if (pg == PG_SAVE) {                 /* (its title: SAVE, or LOAD's SCENES) */
        sp_add("K1: LOAD OR SAVE, NOW ");
        sp_add(sv.load ? "LOAD" : "SAVE");
        sp_stop();
        sp_add("SEL: NEXT. FX: BACK.");
        return;
    }
    if (pg == PG_SAVEAS) {
        sp_saveas(1, 1);
        sp_stop();
        sp_add("SEL: NEXT. FX: BACK.");
        return;
    }
    if (pg == PG_SCENES) {
        sp_scene_row(fm.pick);
        sp_stop();
        sp_knob(P_MORPH);
        sp_comma();
        sp_knob(P_LOAD);
        sp_stop();
        sp_add(fm.pick < NSCENES ? "PLAY: LOAD. SAVE + REC: STORE." : "PLAY: LOAD.");
        return;
    }
    if (pg == PG_NOARP) {
        sp_add("NOT AVAILABLE FOR DRONE");
        return;
    }
    for (i = 0; i < 4u; i++) {
        uint32_t p = page_knob(pg, i);
        if (p != NPARAM) {
            sp_knob(p);
            sp_comma();
        }
    }
    if (all && pg == PG_SCREEN) {
        sp_stop();
        sp_add("REDUCE ANIMATIONS TO SAVE CPU IN HEAVY SCENES.");
    }
}

/* a pop-up: its title, its lines, its way out (popup) */
static void sp_popup(void)
{
    uint32_t i;
    sp_add(pop.title);
    sp_stop();
    for (i = 0; i < 3u; i++)
        if (pop.line[i][0]) {
            sp_add(pop.line[i]);
            sp_add(" ");
        }
    sp_stop();
    sp_add("SEL: OK.");
}

/* the name being written: what it is for, the name; all: and its controls */
static void sp_char(char c)
{
    char t[2] = {c, 0};
    sp_add(c == ' ' ? "SPACE" : c == '-' ? "DASH" : c == '.' ? "DOT" : t);
}
static void sp_name(uint32_t part)       /* 0: what it is for, the name, the cursor; 1: its controls */
{
    char t[SC_LONG + 1];
    int32_t n = (int32_t)nm.len, i;
    if (part) {
        if (part == 1u) {
            sp_add("OCT- OCT+ OR K1: MOVE THE CURSOR. K2: SPACE. K3: DIGITS. K4: LETTERS. ");
            sp_add(nm.flow ? "SAVE: STORE. FX: BACK." : "SAVE: KEEP THE NAME. FX: CANCEL.");
        }
        return;
    }
    sp_add("NAME IT");
    sp_comma();
    if (nm.flow && sv.what) {
        static const char *const W[3] = {"DRONE PRESET", "GLINT PRESET", "BASS PRESET"};
        sp_add(W[(sv.what - 1u) % 3u]);
        sp_add(" ");
        if (!sv.into)
            sp_add("NEW");
        else
            sp_num(sv.into);
    } else {
        sp_add(nm.flow ? "SCENE " : "STORED IN ");
        sp_num((int32_t)(nm.flow ? sv.into : nm.slot) + 1);
    }
    sp_stop();
    while (n > 0 && nm.name[n - 1] == ' ')
        n--;
    for (i = 0; i < n; i++)
        t[i] = nm.name[i];
    t[i] = 0;
    sp_add(n ? t : "NO NAME");
    sp_stop();
    sp_add("CURSOR ");
    sp_char(nm.name[nm.cur]);
    sp_comma();
    sp_num(nm.cur + 1);
    sp_add(" OF ");
    sp_num(nm.len);
    sp_stop();
}

/* the grid of names: the setting, its row; how to answer when it asks */
static void sp_grid(void)
{
    sp_add(PD[ng.p].name);
    sp_add(PD[ng.p].fmt == F_LTGT && sp_section(ng.v) >= 0 ? ", " : ": ");   /* (TARGET 3, DRONE: TIMBRE: one colon) */
    sp_row(ng.p, ng.v, 1);
    srd.sec = (uint8_t)(sp_section(ng.v) + 1);
    if (grid_asks()) {
        sp_stop();
        sp_add("SEL: SELECT. FX: CANCEL.");
    }
}

/* the help's page: its name, then each line, a message each (each starts with its control) */
static void sp_help_line(const help_t *h)
{
    const char *s;
    sp_add(h->control);
    sp_add(h->kind == H_BTN ? " BUTTON: " : h->kind == H_ENC ? (h->b ? " ENCODER: " : ": ") : ": ");
    sp_add(h->does);
    sp_stop();
    for (s = h->held; *s; s++) {         /* (its two ways apart: " / ") */
        if (s[0] == ' ' && s[1] == '/' && s[2] == ' ') {
            sp_stop();
            s += 2;
            continue;
        }
        if (s[0] == '+' && s[1] == ':') {   /* ("OCT- +: COMPARE": OCT- held and this button) */
            sp_add("PLUS");
            continue;
        }
        {
            char c[2] = {*s, 0};
            sp_add(c);
        }
    }
    sp_stop();
}
static void sp_help(int all, uint32_t part)   /* 0: its name (all: how to turn it); 1 .. 4: its lines */
{
    uint32_t pg = fm.help_page % HELP_PAGES;
    if (part) {
        if (part <= 4u)
            sp_help_line(&HELP[pg][part - 1u]);
        return;
    }
    sp_add("HELP, ");
    sp_add(HELP_TITLE[pg]);
    sp_add(" (");
    sp_num((int32_t)pg + 1);
    sp_add("/");
    sp_num(HELP_PAGES);
    sp_add(")");
    sp_stop();
    if (all)
        sp_add("SELECT: NEXT PAGE. HOME: CLOSE.");
}

static void sp_logo(int all)
{
    sp_add(APP_NAME " ");
    sp_add(APP_VERSION);
    sp_stop();
    if (!all)
        return;
    sp_add(APP_TAGLINE);
    sp_stop();
    sp_add(APP_SITE ", " APP_SITE_NOTE);
    sp_stop();
    sp_add(APP_AUTHOR);
}

/* what the screen shows besides the page (WHERE), in two parts: the keys' track, the octave, the modes on (sp_modes);
 * what plays, the load, the battery (sp_playing) */
static void sp_modes(void)
{
    if (rec_any()) {
        sp_add("RECORDING ");
        sp_add(keys_word());
        sp_comma();
    }
    if (fm.page != PG_SON) {
        sp_hands();
        sp_stop();
    }
    if (fm.octave) {
        sp_add("OCTAVE ");
        if (fm.octave > 0)
            sp_add("+");
        sp_num(fm.octave);
        sp_comma();
    }
    if (st.v[P_FREEZE]) {
        sp_add("FREEZE ON");
        sp_comma();
    }
    if (st.compare) {
        sp_add("COMPARE, AS SAVED");
        sp_comma();
    }
    if (mo.on && !mo.anc) {
        sp_add("MORPH TO ");
        sp_add(mo.name);
        sp_comma();
    }
}
static void sp_playing(void)
{
    if (fm.page == PG_SON) {             /* what the river's corner says (river_status) */
        if (st.v[P_SWELL] || st.swell_btn > 0.0f) {
            sp_add("SWELL");
            sp_comma();
        }
        if (lfo_any) {
            uint32_t i, k = 0;
            for (i = 0; i < NLFO; i++)
                if (lfo[i].on) {
                    sp_add(k++ ? " " : "LFO ");
                    sp_num((int32_t)i + 1);
                }
            if (k)
                sp_add(" RUNNING");
            sp_comma();
        }
        if (st.v[P_DQ_ON]) {
            sp_add(st.v[P_DQ_REC] ? "DRONE REC" : "DRONE SEQ");
            sp_comma();
        }
        if (st.v[P_BQ_ON] == SQT_SEQ) {
            sp_add(st.v[P_BQ_REC] ? "BASS REC" : "BASS SEQ");
            sp_comma();
        } else if (st.v[P_BQ_ON] == SQT_ARP && barp.n) {
            sp_add("BASS ARP");
            sp_comma();
        }
        if (st.v[P_SQ_TYPE] == SQT_SEQ) {
            sp_add(st.v[P_SQ_REC] ? "GLINT REC" : "GLINT SEQ");
            sp_comma();
        } else if (st.v[P_SQ_TYPE] == SQT_ARP && arp.n) {
            sp_add(arp.latch ? "ARP LATCH" : "ARP");
            sp_comma();
        }
        if (st.v[P_SCALE] || st.v[P_CHORD]) {
            sp_add(NOTE_NAME[st.v[P_ROOT] % 12u]);
            sp_add(" ");
            sp_add(SCALE_NAME[st.v[P_SCALE] % NSCALES]);
            sp_comma();
        }
    }
    if (fm.page == PG_GLOBAL) {
        sp_add("CPU ");
        sp_num((int32_t)cpu_load());
        sp_add("%");
        sp_comma();
    }
    if (batt.pct >= 0) {
        sp_add("BATTERY ");
        sp_num(batt.pct);
        sp_add("%");
        sp_comma();
    }
}

/* the view, as one number: what changes it is a new view to say */
static uint32_t sp_view(void)
{
    if (saver.on)
        return 1u;
    if (pop.on)
        return 2u;
    if (nm.on)
        return 3u | (uint32_t)nm.flow << 8;
    if (ng.on)
        return 4u | (uint32_t)ng.p << 8;
    if (fm.help)
        return 5u | (uint32_t)fm.help_page << 8;
    if (fm.logo)
        return 6u;
    return 7u | (uint32_t)fm.page << 8 | (fm.page == PG_KEYS ? (uint32_t)st.v[P_KB] << 16 : 0u);
}

/* the view now, its part k (all: everything, WHERE): written, not sent; nothing written, no such part. Its first part
 * goes at once, the others a frame each (several in one frame could fill the USB queue: one that finds no room is
 * dropped whole) */
#define SPK_PARTS 5u
static void sp_view_part(int all, uint32_t k)
{
    if (saver.on) {
        if (k)
            return;
        sp_add("SCREENSAVER");
        if (all)
            sp_add(". TOUCH ANY CONTROL TO WAKE THE SCREEN.");
    } else if (pop.on) {
        if (k || !all)
            return;                      /* (said as it came: popup) */
        sp_popup();
    } else if (nm.on) {
        if (k <= (uint32_t)all)
            sp_name(k);
    } else if (ng.on) {
        if (!k)
            sp_grid();
    } else if (fm.help) {
        sp_help(all, k);
    } else if (fm.logo) {
        if (!k)
            sp_logo(all);
    } else if (!k) {
        sp_page(all);
    } else if (all && k == 1u) {
        sp_modes();
    } else if (all && k == 2u) {
        sp_playing();
    }
}

/* the view said: its first part now (flags), the rest after it (speak: srd.part) */
static void sp_say_view(int all, uint32_t flags)
{
    sp_clear();
    sp_view_part(all, 0);
    srd.flags = flags | (all && pop.on && pop.kind == POP_WARN ? ANN_KEEP : 0u);
    sp_send();
    srd.all = (uint8_t)all;
    srd.part = 1;
}

/* the view's next part, if it has one left: 1 one sent */
static int sp_next_part(void)
{
    while (srd.part && srd.part < SPK_PARTS) {
        sp_clear();
        sp_view_part(srd.all, srd.part++);
        if (srd.n) {
            srd.flags = 0;
            sp_send();
            return 1;
        }
    }
    srd.part = 0;
    return 0;
}

/* encoder e turned: what it turned, as the screen shows it now (0: nothing to say) */
static int sp_readout(uint32_t e)
{
    uint32_t p;
    if (fm.help || fm.logo || saver.on || pop.on || nm.on)   /* (the help's SELECT at its last page: nothing moved) */
        return 0;
    if (ng.on) {                         /* the list: the row (its section when it changes) */
        int s = sp_section(ng.v) + 1;
        sp_row(ng.p, ng.v, s != srd.sec);
        srd.sec = (uint8_t)s;
        return 1;
    }
    if (fm.page == PG_SCENES && (e == EN_K1 || e == EN_SELECT)) {
        sp_scene_row(fm.pick);
        return 1;
    }
    if (fm.page == PG_SAVE && e == EN_K1) {
        sp_add(sv.load ? "LOAD" : "SAVE");
        return 1;
    }
    if (fm.page == PG_SAVEAS && (e == EN_K1 || e == EN_K2)) {
        sp_saveas(e == EN_K1, 1);
        return 1;
    }
    if (e == EN_SELECT) {                /* the track, and its two hands */
        sp_hands();
        return 1;
    }
    if (fm.touched_enc == e && fm.touched < NPARAM && plat_ms() - fm.touched_at < 100u)
        p = fm.touched;                  /* (what it turned: MIXER's PAN with OCT- held, GLO's CUTOFF) */
    else if (e < EN_K1)
        p = macro_of(e);
    else
        p = page_knob(fm.page, e - EN_K1);
    if (p == NPARAM)
        return 0;
    sp_knob(p);
    return 1;
}

/* ------------------------------------------------------------- the hooks --- */
/* app_say's words (a + b), at once; the same message again as a knob turns quickly (an LFO's AMOUNT, its detents under
 * SPK_TURN_MS apart) as a readout: said once the knob rests */
static void spk_msg(const char *a, const char *b, uint32_t flags)
{
    uint32_t now = plat_ms(), h;
    if (!ann_on() || !a)
        return;
    h = sp_hash(a);
    sp_clear();
    sp_add(a);
    if (b && b[0]) {
        sp_add(" ");
        sp_add(b);
    }
    if (h == srd.msg_sig && spk_turned && now - srd.msg_at < SPK_TURN_MS && !(flags & ANN_KEEP)) {
        ann_later(srd.t, ANN_NOW, SPK_TURN_MS, now);
        sp_clear();
    } else {
        srd.flags = flags;
        sp_send();
    }
    srd.msg_sig = h;
    srd.msg_at = now;
    srd.msg = 1;
}

/* a pop-up, as it comes (popup): heard whole if it warns */
static void spk_popup(void)
{
    if (!ann_on())
        return;
    sp_clear();
    sp_popup();
    srd.flags = ANN_NOW | (pop.kind == POP_WARN ? ANN_KEEP : 0u);
    sp_send();
    srd.msg_sig = sp_hash(pop.title);
    srd.msg_at = plat_ms();
    srd.msg = 1;
}

/* what the reader has heard, as it is now (everything said, a new view, a message) */
static void sp_heard(void)
{
    srd.oct = fm.octave;
    srd.cmp = st.compare;
    srd.nm_cur = nm.cur;
    srd.nm_ch = (uint8_t)nm.name[nm.cur];
}

/* once a UI frame (app_frame) */
static void speak(void)
{
    uint32_t now = plat_ms(), v, turned = spk_turned, msg = srd.msg, e;
    spk_turned = 0;
    srd.msg = 0;
    if (!ann_on()) {
        srd.part = 0;
        ann_tick(now);
        return;
    }
    v = sp_view();
    if (ann_where()) {                   /* a reader new, or asking: everything, now */
        srd.view = v;
        srd.view_at = 0;
        srd.read_sig = 0;
        sp_heard();
        sp_say_view(1, ANN_NOW);
        ann_tick(now);
        return;
    }
    if (v != srd.view) {                 /* a new view: said once it has stayed */
        srd.view = v;
        srd.view_at = now | 1u;
        srd.read_sig = 0;
        srd.part = 0;
        sp_heard();
        spk_turned = turned;             /* (kept till it is said: SOUND's K1 .. K4 then) */
    } else if (srd.view_at) {
        if (now - srd.view_at < SPK_SETTLE_MS) {
            spk_turned = turned;
        } else {
            srd.view_at = 0;
            sp_say_view(0, saver.on || now - srd.msg_at < 1300u ? 0u : ANN_NOW);   /* (after a message still heard,
                                                                                * not over it; the screensaver, come by
                                                                                * itself) */
            if ((v & 0xFFu) == 7u && fm.page == PG_SON)   /* (SOUND's short view: not its four settings) */
                spk_turned = turned & 0xFu << EN_K1;
        }
    } else if (msg) {                    /* a message said it: no readout over it */
        sp_heard();
        srd.read_sig = 0;
    } else {                             /* the same view: what the hand changed */
        sp_clear();
        if (nm.on && (nm.cur != srd.nm_cur || (uint8_t)nm.name[nm.cur] != srd.nm_ch)) {   /* the name: the cursor's place
                                                                                          * and its character */
            int moved = nm.cur != srd.nm_cur;
            srd.nm_cur = nm.cur;
            srd.nm_ch = (uint8_t)nm.name[nm.cur];
            sp_char(nm.name[nm.cur]);
            if (moved) {
                sp_comma();
                sp_num(nm.cur + 1);
                sp_add(" OF ");
                sp_num(nm.len);
            }
        } else if (fm.octave != srd.oct) {   /* OCT- OCT+, ANCHOR's back (its value: OCT- a hand, its step undone, is no
                                              * change) */
            srd.oct = fm.octave;
            sp_add("OCTAVE ");
            if (fm.octave > 0)
                sp_add("+");
            sp_num(fm.octave);
        } else if (st.compare != srd.cmp) {
            srd.cmp = st.compare;
            sp_add(st.compare ? "COMPARE, AS SAVED" : "COMPARE OFF");
        } else {
            for (e = 0; e < NE && !srd.n; e++)
                if (turned >> e & 1u)
                    sp_readout(e);
        }
        if (srd.n) {
            uint32_t h = sp_hash(srd.t);
            if (h != srd.read_sig)
                ann_later(srd.t, ANN_NOW, SPK_TURN_MS, now);
            srd.read_sig = h;
        }
        sp_clear();
        sp_next_part();                  /* (the view's next part, a frame each) */
    }
    ann_tick(now);
}

/* ------------------------------------------------ the simulator's dump --- */
#if defined PLAT_HOST && !defined PLAT_WEB
/* every page (each track's), every value of each of its knobs, every list's rows, the scenes, SAVE's choices, the
 * name, the help, the logo: KIND<TAB>the text, as the device sends it (fm1-reader's audit, tools/screen_reader.sh) */
static void sp_out(const char *kind)
{
    while (srd.n && (srd.t[srd.n - 1] == ' ' || srd.t[srd.n - 1] == ','))
        srd.t[--srd.n] = 0;
    if (srd.n)
        printf("%s\t%s\n", kind, srd.t);
    sp_clear();
}

static void sp_dump_knob(uint32_t pg, uint32_t p)
{
    uint32_t x, hi = p == K_IDLE ? 60u : p == K_HEAD || p == K_ARP || p == K_SEQ ? 1u : p == K_TUNE ? A4_HI : PD[p].max;
    if (p < NPARAM && list_param(p)) {
        for (x = 0; x < list_n(p); x++) {
            sp_row(p, x, 1);
            sp_out("ROW");
        }
        return;
    }
    for (x = p == K_IDLE ? 1u : p == K_TUNE ? A4_LO : 0u; x <= hi; x++) {
        sp_knob_at(pg, p, x, 0);
        sp_out("KNOB");
    }
}

void app_speak_dump(void)
{
    uint32_t o, t, i, s, page = fm.page, kb = st.v[P_KB], gl = st.glint, bs = st.bass, load = sv.load, what = sv.what,
                         into = sv.into, pick = fm.pick, hp = fm.help_page;
    uint32_t on = ann.on;
    ann.on = 0;                          /* (nothing sent meanwhile) */
    sp_clear();
    for (o = 0; o < NPAGES; o++)
        for (t = 0; t < 3u; t++) {
            uint32_t pg = PAGE_ORDER[o];
            if (PAGES[pg].track != T_ANY && PAGES[pg].track != t)
                continue;
            fm.page = (uint8_t)pg;
            st.v[P_KB] = (uint8_t)t;
            st.glint = (uint8_t)(t == 1u);
            st.bass = (uint8_t)(t == 2u);
            sp_page(0);
            sp_out("SCREEN");
            sp_page(1);
            sp_out("SCREEN");
            for (i = 0; i < 4u; i++)
                if (page_knob(pg, i) != NPARAM)
                    sp_dump_knob(pg, page_knob(pg, i));
            if (pg == PG_SON) {
                sp_dump_knob(pg, P_KB);
                sp_dump_knob(pg, track_preset());
                sp_dump_knob(pg, track_algo());
                sp_dump_knob(pg, P_FL_CUT);
            }
            if (pg == PG_MIXER)
                for (i = 0; i < 4u; i++)
                    sp_dump_knob(pg, P_PN_DRONE + i);
        }
    fm.page = PG_SCENES;
    for (s = 0; s <= NSCENES; s++) {
        fm.pick = (uint8_t)s;
        sp_scene_row(s);
        sp_out("ROW");
        sp_page(1);
        sp_out("SCREEN");
    }
    fm.page = PG_SAVE;
    for (s = 0; s < 2u; s++) {
        sv.load = (uint8_t)s;
        sp_page(1);
        sp_out("SCREEN");
    }
    fm.page = PG_SAVEAS;
    for (sv.what = 0; sv.what < 4u; sv.what++)
        for (sv.into = 0; sv.into < (sv.what ? up_n(sv.what - 1u) + 1u : NSCENES); sv.into++) {
            sp_saveas(1, 1);
            sp_out("KNOB");
            sp_page(1);
            sp_out("SCREEN");
        }
    sv.load = (uint8_t)load;
    sv.what = (uint8_t)what;
    sv.into = (uint8_t)into;
    for (fm.help_page = 0; fm.help_page < HELP_PAGES; fm.help_page++) {
        sp_add("HELP, ");
        sp_add(HELP_TITLE[fm.help_page]);
        sp_add(" (");
        sp_num((int32_t)fm.help_page + 1);
        sp_add("/");
        sp_num(HELP_PAGES);
        sp_add(")");
        sp_stop();
        sp_add("SELECT: NEXT PAGE. HOME: CLOSE.");
        sp_out("SCREEN");
        for (i = 0; i < 4u; i++) {
            sp_help_line(&HELP[fm.help_page][i]);
            sp_out(HELP[fm.help_page][i].kind == H_BTN ? "HELP" : "GESTURE");
        }
    }
    {                                    /* the name: a scene's, a preset's; each character at the cursor */
        uint8_t non = nm.on, nfl = nm.flow, ncur = nm.cur, nlen = nm.len, nslot = nm.slot;
        char nname[SC_LONG + 1];
        uint32_t c;
        memcpy(nname, nm.name, sizeof nname);
        name_fill("NORTH RIVER", SC_LONG);
        for (s = 0; s < 3u; s++) {
            nm.flow = (uint8_t)(s > 0u);
            sv.what = (uint8_t)(s == 2u);
            sp_name(0);
            sp_out("SCREEN");
            sp_name(1);
            sp_out("SCREEN");
        }
        for (i = 0; i < 2u; i++)
            for (c = 0; (i ? NM_DIGITS : NM_LETTERS)[c]; c++) {
                sp_char((i ? NM_DIGITS : NM_LETTERS)[c]);
                sp_comma();
                sp_num((int32_t)c + 1);
                sp_add(" OF 32");
                sp_out("KNOB");
            }
        nm.on = non;
        nm.flow = nfl;
        nm.cur = ncur;
        nm.len = nlen;
        nm.slot = nslot;
        memcpy(nm.name, nname, sizeof nname);
        sv.what = (uint8_t)what;
    }
    sp_logo(1);
    sp_out("SCREEN");
    sp_add("SCREENSAVER. TOUCH ANY CONTROL TO WAKE THE SCREEN.");
    sp_out("SCREEN");
    sp_add("OCTAVE +2");
    sp_out("KNOB");
    sp_add("OCTAVE -3");
    sp_out("KNOB");
    sp_add("COMPARE, AS SAVED");
    sp_out("KNOB");
    sp_add("COMPARE OFF");
    sp_out("KNOB");
    fm.page = (uint8_t)page;
    st.v[P_KB] = (uint8_t)kb;
    st.glint = (uint8_t)gl;
    st.bass = (uint8_t)bs;
    fm.pick = (uint8_t)pick;
    fm.help_page = (uint8_t)hp;
    ann.on = (uint8_t)on;
}
#endif
