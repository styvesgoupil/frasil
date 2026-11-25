/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* CHORD PADS (card #208; frasil_engine.h, chords.c): an engine alone built with FRS_CHORDS 1 (FrasilVibe's; the FM-1's
 * and the SMK-37's to come). Included before core.c (frasil_engine.c): what the brain plays goes to the sound through
 * the chord's move (chords.c, included last), each of the DSP's note calls renamed here to its cm_ twin: the drones'
 * notes (silent while the move holds them, its chord sounding instead), GLINT's, the bass's, the LEAD's (moved along
 * the scale while they follow). Without a move each twin calls the DSP's own as it was called: the same samples. */
#pragma once
#include "frasil.h"                      /* (the DSP's own, declared before they are renamed) */

static void cm_note_on(int note, float vel);
static void cm_note_off(int note);
static void cm_all_off(void);
static void cm_pressure(int note, float p);
static void cm_glint_on(int note, float vel);
static void cm_glint_off(int note);
static void cm_glint_pressure(int note, float p);
static void cm_bass_on(int note, float vel);
static void cm_bass_retune(int note, float vel);
#define frasil_note_on cm_note_on
#define frasil_note_off cm_note_off
#define frasil_all_off cm_all_off
#define frasil_pressure cm_pressure
#define frasil_glint_on cm_glint_on
#define frasil_glint_off cm_glint_off
#define frasil_glint_pressure cm_glint_pressure
#define frasil_bass_on cm_bass_on
#define frasil_bass_retune cm_bass_retune
#if FRS_LEAD
static void cm_lead_on(int note, float vel, int legato);
static void cm_lead_poly_on(int note, float vel);
static void cm_lead_poly_off(int note);
static void cm_lead_pressure(int note, float p);
#define frasil_lead_on cm_lead_on
#define frasil_lead_poly_on cm_lead_poly_on
#define frasil_lead_poly_off cm_lead_poly_off
#define frasil_lead_pressure cm_lead_pressure
#endif

static void cm_init(void);               /* (frs_init: no move, every track following) */
static void cm_tick(void);               /* a millisecond: a move waiting for the beat, the drones' glide */
static void cm_drop(void);               /* no move (tr_reset's: a scene taken, PANIC, RELEASE ALL, CUT) */
static void cm_zero(void);               /* forgotten, the sound left as it is (a running state loaded over it) */
static void cm_view(frs_view_t *v);
