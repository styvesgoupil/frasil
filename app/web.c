/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's own exports in the browser build (app.h APP_WEB; base/web/emu/emu_web.c includes it): the River plays a
 * shard with the app as it runs. The page writes the shard's scene_t (web_scene_size() bytes, its STEP LENGTH last) to
 * web_scene_buf, then calls web_scene_play; web_scene_stop lets every note go (web/emu/app_worklet.js: {type: "app",
 * op: "scene" | "stop"}). THE KEYS, the editor's settings, the live view: the engine's (engine/core.c). A shard of an
 * earlier layout (FRSy's 324 bytes, FRSx's 323, FRSw's 322, FRSu's 298, FRSt's 297, and those before made over to FRSt's):
 * web_scene_over(n) first makes it
 * over, as the FM-1 does (scene_over) */
static scene_t web_sc;
__attribute__((used, visibility("default"))) uint8_t *web_scene_buf(void) { return (uint8_t *)&web_sc; }
__attribute__((used, visibility("default"))) uint32_t web_scene_size(void) { return (uint32_t)sizeof web_sc; }
__attribute__((used, visibility("default"))) void web_scene_play(void) { eng_scene_play(&web_sc, 1); }
__attribute__((used, visibility("default"))) void web_scene_stop(void) { eng_scene_stop(); }
/* the scene in web_scene_buf, n bytes of an earlier layout's (324 FRSy .. 297 FRSt: core.c scene_over), made over in
 * place to this one's: 0,
 * or -1 not a size it knows */
__attribute__((used, visibility("default"))) int web_scene_over(uint32_t n) { return scene_over(&web_sc, n); }
/* the River's editor's own engine (silent): the shard taken as a load takes it, no chord held for one that plays
 * nothing (▶'s only: never saved) */
__attribute__((used, visibility("default"))) void web_scene_take(void) { eng_scene_play(&web_sc, 0); }
/* a setting or a step edited (eng_edit_param), then the shard as it is now (web_scene_size() bytes) */
__attribute__((used, visibility("default"))) void web_param(uint32_t p, uint32_t x) { eng_edit_param(p, x); }
__attribute__((used, visibility("default"))) uint8_t *web_scene_edited(void)
{
    static scene_t e;
    eng_scene_edited(&web_sc, &e);
    return (uint8_t *)&e;
}
/* THE KEYS: LATCH (1 latched, 0 let go, -1 nothing), CLEAR, HOLD (eng_latch, eng_clear, eng_hold) */
__attribute__((used, visibility("default"))) int web_latch(uint32_t t, uint32_t note) { return eng_latch(t, note); }
__attribute__((used, visibility("default"))) void web_clear(uint32_t t) { eng_clear(t); }
__attribute__((used, visibility("default"))) void web_hold(uint32_t t, uint32_t note, uint32_t down) { eng_hold(t, note, down); }
/* what sounds now (eng_live: ENG_LIVE floats) */
static float web_lv[ENG_LIVE];
__attribute__((used, visibility("default"))) uint32_t web_live_size(void) { return ENG_LIVE; }
__attribute__((used, visibility("default"))) float *web_live(void)
{
    eng_live(web_lv);
    return web_lv;
}
/* the drone's sequence's steps (3.0), as the editor edits them: the page writes a step's notes (up to DQ_NOTES bytes, as
 * the step keeps them: 1 .. 126; one DQ_TIE, a tie; none, a rest) to web_dseq_buf, then web_dseq_step(step, n); a
 * step changed is an edit (eng_edits), as a setting's */
static uint8_t web_dq[DQ_NOTES];
__attribute__((used, visibility("default"))) uint8_t *web_dseq_buf(void) { return web_dq; }
__attribute__((used, visibility("default"))) void web_dseq_step(uint32_t step, uint32_t n)
{
    if (eng_dseq_step(step, web_dq, n))
        eng_edits();
}
/* a setting as the engine has it now (st.v: a drone's preset's values, GLINT's STEP's own, LATCH as it is), for the
 * editor to show what each op made of the shard; 0 past NPARAM */
__attribute__((used, visibility("default"))) uint32_t web_value(uint32_t p) { return p < NPARAM ? st.v[p] : 0u; }
