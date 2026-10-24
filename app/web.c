/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's own exports in the browser build (app.h APP_WEB; base/web/emu/emu_web.c includes it): the River plays a
 * shard with the app as it runs. The page writes the shard's scene_t (272 bytes, its bass's sequence last) to
 * web_scene_buf, then calls web_scene_play; web_scene_stop lets every note go (web/emu/app_worklet.js: {type: "app",
 * op: "scene" | "stop"}) */
static scene_t web_sc;
__attribute__((used, visibility("default"))) uint8_t *web_scene_buf(void) { return (uint8_t *)&web_sc; }
__attribute__((used, visibility("default"))) uint32_t web_scene_size(void) { return (uint32_t)sizeof web_sc; }
__attribute__((used, visibility("default"))) void web_scene_play(void) { scene_audition(&web_sc); }
__attribute__((used, visibility("default"))) void web_scene_stop(void) { audition_stop(); }
