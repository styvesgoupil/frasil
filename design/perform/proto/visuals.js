/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Perform's VISUALS: the other view, nothing to read, light only, the whole window (a stage's backdrop). What sounds,
 * drawn as fire and ice in a field that keeps what was drawn, moves it and lets it fade (WebGL2: the field's three
 * energies, fire, ice and frasil, a frame's, bloomed, then coloured: electric orange, electric blue, white):
 *   the drones   a ring each voice, as wide as its note is low (a chord: rings nested at its intervals), turning at the
 *                pace of their ratios to the lowest; CHOIR thickens them, PARTIALS rings their harmonics inside, BREATH
 *                turns them to dust; WARMTH and the low notes burn them (fire rises), the high ones freeze (ice turns)
 *   GLINT        each note struck a spark where its note sits on the circle (C at the top, octaves outwards), its
 *                ripple spreading through the field
 *   the bass     fire from below, burning where its note is
 *   the sound    heard (an AnalyserNode: the FM-1's USB audio, the River player's): its lows push the field out and
 *                feed the embers, its highs the frasil (specks of ice) and the grain, an attack a flash
 *   the hand     a knob turned stirs a whirl at its cell (up one way, down the other); a scene loaded writes its name
 *                into the field and opens another chapter; OCTAVE pulls it in or out; FREEZE stills it and frosts it
 *   the effects  CLOUD how long it all stays (DECAY), how wide (SIZE, WIDTH), how it blurs (DIFFUSION, BLUR), MODULATION
 *                a shimmer; the MASTER FILTER how soft; LOFI TAPE its SATURATION the colours driven, its WOBBLE a
 *                shockwave each time the tape drags (as strong as the drag); SINK a blue veil, the ice sinking; GLINT's delay its sparks echoed (TIME,
 *                FEEDBACK, MIX, left then right)
 *   the timbre   CHOIR's SHAPE the rings' shape (a saw round, the square a square, the pulse a crystal), DETUNE two
 *                rings beating; PARTIALS' BEAT their harmonics pulsing; BREATH's GRAIN the screen's grain, coarser;
 *                GLINT's VIBRATO its sparks trembling; the bass's FILTER its flames' height, RESONANCE their white heart
 * Never twice the same: slow currents (three sines each, their periods prime to each other, a scene its own phases)
 * and a chapter every minute or so (rings or comets, lobes, mirrored or not, which way it turns and breathes).
 * Over it FRASIL's logo top right, alive as the header's (logo.js), its water rippling with the sound, FRASILSYNTH.COM
 * under it, and the scene's name blurred.
 *   PerformVisuals.create(canvas) -> {ok, draw(f, now, s), gesture(kind, d), heard (how loud, -1: nothing heard)}
 *     f: web_live's floats · s: {n(p): a setting as it sounds 0..1, scene, artist, bare, an: an AnalyserNode or null}
 *     gesture: "knob" {cell, by} · "hand" {by} · "page" · "scene" · "button" {key} (PerformShow.onGesture's) */
"use strict";
window.PerformVisuals = (() => {
  const ID = window.PERFORM_SCHEMA.ids;   /* (the settings named: their numbers move as FRASIL gains some) */
  const VS = `#version 300 es
in vec2 a;
out vec2 uv;
void main() { uv = a * 0.5 + 0.5; gl_Position = vec4(a, 0.0, 1.0); }`;

  /* gradient noise from an integer hash (Chris Wellons' lowbias32), its curl: a flow without sinks */
  const NOISE = `
uint hsh(uint x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
vec2 grd(vec2 i) {
  uint n = hsh(uint(int(i.x) + 8192) * 1597334677u ^ uint(int(i.y) + 8192) * 3812015801u);
  float a = float(n) * (6.2831853 / 4294967296.0);
  return vec2(cos(a), sin(a));
}
float gn(vec2 p) {
  vec2 i = floor(p), f = fract(p), u = f * f * (3.0 - 2.0 * f);
  return mix(mix(dot(grd(i), f), dot(grd(i + vec2(1, 0)), f - vec2(1, 0)), u.x),
             mix(dot(grd(i + vec2(0, 1)), f - vec2(0, 1)), dot(grd(i + vec2(1, 1)), f - vec2(1, 1)), u.x), u.y);
}
float psi(vec2 p, float t) { return gn(p + vec2(t * 0.31, -t * 0.17)) + 0.5 * gn(p * 2.03 + vec2(-t * 0.23, t * 0.29) + 7.3); }
vec2 curl(vec2 p, float t) {
  const float e = 0.04;
  return vec2(psi(p + vec2(0, e), t) - psi(p - vec2(0, e), t), psi(p - vec2(e, 0), t) - psi(p + vec2(e, 0), t)) / (2.0 * e);
}
mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
float rnd(uint a, uint b, uint c) { return float(hsh(a * 1973u + b * 9277u + c * 26699u)) / 4294967296.0; }`;

  /* the field, a frame on: what it held moved (the ice turned, zoomed, carried, stirred; the fire risen), fading, the
   * fire melting the ice into frasil where they meet; then what sounds now drawn into it */
  const FIELD = `#version 300 es
precision highp float;
in vec2 uv;
out vec4 o;
uniform sampler2D uPrev, uText;
uniform vec2 uTx;
uniform float uAsp, uT, uFrame;
uniform vec2 uC;
uniform vec4 uIce;    /* a frame's turn, zoom, flow; the flow's scale */
uniform vec4 uFire;   /* a frame's rise, flow; the flow's scale; the ice's sharpness (0 blurred, 1 as is, more: crisper) */
uniform vec4 uKeep;   /* what a frame keeps of the ice, the fire, the frasil; the blur (texels) */
uniform vec4 uMix;    /* the melt, the mirror, SINK's fall, the light drawn a frame */
uniform vec4 uDr[8];  /* each drone: radius, level, its arc's angle, temperature (0 ice, 1 fire) */
uniform vec4 uDr2[8]; /* its arc's half width (more than pi: the whole ring), its centre off the field's (x, y), white */
uniform vec2 uLobe;   /* the rings' lobes, their depth */
uniform vec4 uLay;    /* CHOIR, PARTIALS, BREATH (as they sound), the ripples' strength */
uniform vec4 uSp[24]; /* GLINT's sparks (its delay's echoes too): x, y, age (s), energy */
uniform vec4 uVx[6];  /* the hand's whirls: x, y, a frame's twist, radius */
uniform vec4 uBass;   /* level, x, DRIVE, the lows heard */
uniform vec4 uShape;  /* the rings' shape (CHOIR's SHAPE: 2 a circle, more a square, under 1 a star), DETUNE's split,
                       * CLOUD's WIDTH (wider), its MODULATION (a shimmer) */
uniform vec4 uLay2;   /* PARTIALS' BEAT, BASS's FILTER, its RESONANCE, - */
uniform vec4 uMisc;   /* the frasil's specks (a pixel's chance), the name written, -, - */
${NOISE}
vec2 toUV(vec2 q) { return q / vec2(uAsp, 1.0) + 0.5; }
vec4 tap(vec2 u, float b) {
  vec2 d = uTx * b;
  return (texture(uPrev, u) * 2.0 + texture(uPrev, u + d) + texture(uPrev, u - d)
          + texture(uPrev, u + vec2(d.x, -d.y)) + texture(uPrev, u + vec2(-d.x, d.y))) / 6.0;
}
float edge(vec2 u) { return smoothstep(0.0, 0.015, min(min(u.x, 1.0 - u.x), min(u.y, 1.0 - u.y))); }
void main() {
  vec2 P = (uv - 0.5) * vec2(uAsp, 1.0);
  vec2 rip = vec2(0.0);
  for (int i = 0; i < 24; i++) {
    vec4 s = uSp[i];
    if (s.w <= 0.0) continue;
    vec2 d = P - s.xy;
    float r = length(d) + 1e-4, R = s.z * 0.21;
    rip += d / r * sin((r - R) * 90.0) * exp(-pow((r - R) * 20.0, 2.0)) * exp(-s.z * 1.3) * s.w;
  }
  rip *= uLay.w * 0.0022;

  vec2 q = P - uC;
  q = rot(uIce.x) * q * (1.0 - uIce.y);
  q += curl(q * uIce.w, uT) * uIce.z;
  q += uShape.w * 0.0016 * vec2(sin(q.y * 17.0 + uT * 2.3), cos(q.x * 15.0 - uT * 1.9));
  float stir = 0.0;
  for (int i = 0; i < 6; i++) {
    vec4 v = uVx[i];
    if (v.z == 0.0) continue;
    vec2 c = v.xy - uC, d = q - c;
    float w = exp(-dot(d, d) / (v.w * v.w));
    q = c + rot(v.z * w) * d;
    stir += abs(v.z) * w;
  }
  q.y += uMix.z;
  q += rip;
  vec2 ui = toUV(q + uC), um = toUV(vec2(-q.x, q.y) + uC);
  vec2 qf = P + vec2(0.0, -uFire.x) + curl(P * uFire.z + vec2(0.0, -uT * 0.6), uT * 1.7) * uFire.y + rip * 0.6;
  vec2 uf = toUV(qf);

  float b = uKeep.w;
  vec4 A = tap(ui, b), Ac = texture(uPrev, ui), B = tap(uf, b * 1.4);
  if (uMix.y > 0.0) { A = mix(A, tap(um, b), uMix.y * 0.5); Ac = mix(Ac, texture(uPrev, um), uMix.y * 0.5); }
  float ice = max(0.0, A.g + (Ac.g - A.g) * uFire.w) * uKeep.x * edge(ui);
  float fro = max(0.0, A.b + (Ac.b - A.b) * 0.6) * uKeep.z * edge(ui);
  float fire = B.r * uKeep.y * edge(uf);
  float melt = uMix.x * fire * ice;
  ice = max(0.0, ice - melt);
  fire = max(0.0, fire - 0.4 * melt);
  fro += 0.7 * melt + min(stir, 0.12) * 0.5 * (ice + fire);   /* (the hand's whirl lights what it stirs) */

  float g = uMix.w;
  float hr = rnd(uint(gl_FragCoord.x), uint(gl_FragCoord.y), uint(uFrame));
  float dust = mix(1.0, step(0.74, hr) * 3.4, uLay.z);
  for (int i = 0; i < 8; i++) {
    vec4 d = uDr[i];
    if (d.y <= 0.0) continue;
    vec4 e = uDr2[i];
    vec2 v = P - uC - e.yz, w = rot(d.z * 0.25) * (v * vec2(1.0 / uShape.z, 1.0));
    float ang = atan(v.y, v.x), r = pow(pow(abs(w.x), uShape.x) + pow(abs(w.y), uShape.x), 1.0 / uShape.x);
    float R = d.x * (1.0 + uLobe.y * sin(uLobe.x * ang + uT * (0.13 + 0.05 * float(i))));
    float th = 0.0014 + 0.0045 * uLay.x + 0.0012 * d.y;
    float da = abs(mod(ang - d.z + 3.14159265, 6.2831853) - 3.14159265);
    float win = e.x >= 3.1 ? 1.0 : smoothstep(e.x, e.x * 0.2, da);
    float sp = uShape.y * (0.7 + 0.3 * sin(uT * 2.7 + float(i) * 1.3));   /* DETUNE: two rings, beating */
    float ring = 0.5 * (exp(-pow((r - R * (1.0 + sp)) / th, 2.0)) + exp(-pow((r - R * (1.0 - sp)) / th, 2.0)));
    ring += uLay.y * (0.35 * exp(-pow((r - R * 0.5) / (th * 0.8), 2.0)) + 0.18 * exp(-pow((r - R / 3.0) / (th * 0.7), 2.0)))
          * (1.0 - uLay2.x * 0.75 * (0.5 + 0.5 * sin(uT * 6.0 + float(i) * 1.7)));   /* BEAT: the harmonics pulsing */
    float en = ring * win * d.y * g * dust;
    ice += en * (1.0 - d.w);
    fire += en * d.w * 1.25;
    fro += en * e.w;
  }
  for (int i = 0; i < 24; i++) {
    vec4 s = uSp[i];
    if (s.w <= 0.0) continue;
    vec2 d = P - s.xy;
    float r2 = dot(d, d), rr = sqrt(r2), R = s.z * 0.21;
    float core = exp(-r2 / (0.00003 + 0.0001 * s.w)) * max(0.0, 1.0 - s.z / 0.09);
    float ring = exp(-pow((rr - R) / 0.0035, 2.0)) * exp(-s.z * 2.6) * 0.1
               * smoothstep(-0.15, 0.45, gn(vec2(atan(d.y, d.x) * 2.5, s.z * 1.5 + float(i) * 7.1)));
    fro += (core * 2.4 + ring) * s.w * g * 6.0;
    ice += (core * 0.8 + ring * 1.4) * s.w * g * 3.0;
  }
  float y = P.y + 0.5;
  float fl = exp(-y / (0.03 + 0.035 * uBass.z + 0.08 * uLay2.y)) * (0.45 + 0.55 * (0.5 + gn(vec2(P.x * 6.0 - uT * 0.3, uT * 1.9))))
           * exp(-pow((P.x - uBass.y) / (0.22 + 0.45 * uBass.w), 2.0));
  fire += fl * (uBass.x * 0.9 + uBass.w * uBass.w * 0.25) * g * 0.6;
  fro += fl * uBass.x * uLay2.z * g * 0.4;   /* its RESONANCE: white at the flames' heart */
  if (rnd(uint(gl_FragCoord.y), uint(gl_FragCoord.x), uint(uFrame) + 77u) > 1.0 - uMisc.x) { fro += 0.9; ice += 0.4; }
  if (uMisc.y > 0.0) { float tx = texture(uText, uv).g; fro += tx * uMisc.y * 0.06; ice += tx * uMisc.y * 0.04; }
  o = vec4(min(fire, 4.0), min(ice, 4.0), min(fro, 4.0), 1.0);
}`;

  /* the bloom: halved four times, then grown back, each size added (a dual filter) */
  const DOWN = `#version 300 es
precision highp float;
in vec2 uv;
out vec4 o;
uniform sampler2D uSrc;
uniform vec2 uH;
void main() {
  o = (texture(uSrc, uv) * 4.0 + texture(uSrc, uv - uH) + texture(uSrc, uv + uH)
       + texture(uSrc, uv + vec2(uH.x, -uH.y)) + texture(uSrc, uv - vec2(uH.x, -uH.y))) / 8.0;
}`;
  const UP = `#version 300 es
precision highp float;
in vec2 uv;
out vec4 o;
uniform sampler2D uSrc, uAdd;
uniform vec2 uH;
void main() {
  vec4 s = texture(uSrc, uv + vec2(-uH.x * 2.0, 0.0)) + texture(uSrc, uv + vec2(uH.x * 2.0, 0.0))
         + texture(uSrc, uv + vec2(0.0, uH.y * 2.0)) + texture(uSrc, uv + vec2(0.0, -uH.y * 2.0))
         + (texture(uSrc, uv + vec2(-uH.x, uH.y)) + texture(uSrc, uv + vec2(uH.x, uH.y))
            + texture(uSrc, uv + vec2(uH.x, -uH.y)) + texture(uSrc, uv + vec2(-uH.x, -uH.y))) * 2.0;
  o = s / 12.0 + texture(uAdd, uv);
}`;

  /* the logo's box, as the header draws it (logo.js's canvas, 798 x 162) with the address under it (perform.css's
   * .brand at three times its size), room around for its halo */
  const LG = { w: 798, h: 162, gap: 27, url: 51, pad: 48 };
  LG.W = LG.w + 2 * LG.pad;
  LG.H = LG.h + LG.gap + LG.url + 2 * LG.pad;

  /* the picture: the field coloured (its fire electric orange, its ice electric blue, its frasil white, burning to
   * white as they grow), its bloom, the name's glow; filmic, a vignette; the logo; the grain over everything */
  const OUT = `#version 300 es
precision highp float;
in vec2 uv;
out vec4 o;
uniform sampler2D uField, uBloom, uText, uLogo;
uniform float uAsp, uT, uFrame, uDpr;
uniform vec4 uLook;    /* exposure, bloom, the colours' split, softness (MASTER FILTER low pass) */
uniform vec4 uGrade;   /* grain, TAPE's wobble, a flash, SINK's veil */
uniform vec4 uTitle;   /* presence, sharpness, the grain's size (BREATH's GRAIN), - */
uniform vec4 uLogoBox; /* the logo's box: x, y, width, height (pixels from the bottom left) */
uniform vec4 uLogoK;   /* opacity, the ripples' strength, glow, the texture's level for its size on the screen */
uniform vec3 uTint;    /* the glow's colour, as the field's */
uniform vec4 uRip[6];  /* the logo's ripples: age (s), strength, where on the waterline (0..1) */
uniform vec4 uTape;    /* LOFI TAPE (on): SATURATION, a drag's shockwave: its strength, its way out (0..1), - */
uniform float uFrost;  /* FREEZE: the ice's crystals drawn (only drawn: the field itself never sharpens) */
const vec2 LG_PX = vec2(${LG.W}.0, ${LG.H}.0);
const float LG_H = ${(LG.h / LG.H).toFixed(5)}, LG_WL = ${(1 - (LG.pad + LG.h * 223 / 472) / LG.H).toFixed(5)}, LG_LO = ${(1 - (LG.pad + LG.h) / LG.H).toFixed(5)};   /* the logo's box: its letters' height, waterline, foot */
${NOISE}
float crystal(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float d1 = 8.0, d2 = 8.0;
  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
    vec2 g = vec2(x, y);
    uint h = hsh(uint(int(i.x + g.x) + 8192) * 1597334677u ^ uint(int(i.y + g.y) + 8192) * 3812015801u);
    float d = length(g + vec2(float(h & 65535u), float(h >> 16)) / 65535.0 - f);
    if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
  }
  return 1.0 - smoothstep(0.0, 0.035, d2 - d1);
}
vec3 heat(vec3 e) {
  float m = min(e.r, e.g);              /* where fire meets ice: steam, white, never a purple */
  e.rg -= m;
  e.b += m * 1.3;
  return vec3(1.0, 0.27, 0.015) * e.r + vec3(1.0, 0.62, 0.25) * e.r * e.r * 0.16   /* (the heat: electric orange) */
       + vec3(0.008, 0.16, 1.0) * e.g + vec3(0.2, 0.58, 1.0) * e.g * e.g * 0.1        /* (the ice: a deeper blue) */
       + vec3(0.86, 0.93, 1.0) * e.b;
}
vec3 aces(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
void main() {
  vec2 u = uv;
  u.x += uGrade.y * 0.003 * sin(u.y * 5.0 + uT * 1.1) * sin(uT * 0.37 + 1.0);
  u.y += uGrade.y * 0.0015 * sin(uT * 0.83);
  float shock = 0.0;
  if (uTape.y > 0.0) {                   /* WOBBLE: a drag's shockwave from the middle, bending what it crosses */
    vec2 sd = (u - 0.5) * vec2(uAsp, 1.0);
    float sr = length(sd) + 1e-4, R = uTape.z * 1.3;
    shock = exp(-pow((sr - R) / 0.09, 2.0)) * (1.0 - uTape.z) * uTape.y;
    u -= sd / sr / vec2(uAsp, 1.0) * shock * 0.028 * sin((sr - R) * 30.0 + 1.2);
  }
  vec2 dc = u - 0.5;
  float ca = uLook.z * 0.004 + shock * 0.004;
  vec3 c = heat(texture(uField, u).rgb);
  if (ca > 0.0) { c.r = heat(texture(uField, u + dc * ca).rgb).r; c.b = heat(texture(uField, u - dc * ca).rgb).b; }
  vec3 bl = heat(texture(uBloom, u).rgb);
  if (uFrost > 0.01) {
    vec2 fp = dc * vec2(uAsp, 1.0) * 13.0;
    float ice = texture(uField, u).g, ed = crystal(fp) * 0.7 + crystal(fp * 2.6 + 3.1) * 0.3;
    c += ed * uFrost * min(ice, 1.2) * vec3(0.55, 0.85, 1.0) * 0.2;   /* (faint: a frost seen, not a net) */
    c = mix(c, vec3(dot(c, vec3(0.3, 0.5, 0.2))) * vec3(0.8, 0.95, 1.12), 0.2 * uFrost);
  }
  c = mix(c, bl * 1.5, uLook.w) + bl * uLook.y;
  vec2 tu = uv + vec2(sin(uv.y * 8.0 + uT * 0.6), cos(uv.x * 6.0 - uT * 0.45)) * 0.0012;
  vec3 tx = texture(uText, tu).rgb;
  c += (tx.r * uTitle.y + tx.g * 0.4 + tx.b * 0.65) * uTitle.x * vec3(0.8, 0.88, 1.0) * 0.55;
  c = c * (1.0 + shock * 0.35) + shock * 0.025 * vec3(0.55, 0.8, 1.0);   /* (its front lit) */
  c *= uLook.x * (1.0 + uGrade.z * 0.7);
  c = aces(c + vec3(0.002, 0.003, 0.006));
  c = mix(c, c * vec3(0.7, 0.86, 1.18), uGrade.w * 0.55);
  if (uTape.x > 0.0) {                   /* SATURATION: the colours driven */
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = pow(max(vec3(0.0), mix(vec3(l), c, 1.0 + 1.3 * uTape.x)), vec3(1.0 + 0.3 * uTape.x));
  }
  c *= mix(0.45, 1.0, smoothstep(1.25, 0.3, length(dc * vec2(uAsp, 1.0) * 1.05)));
  c = pow(c, vec3(1.0 / 2.2));

  vec2 lp = (gl_FragCoord.xy - uLogoBox.xy) / uLogoBox.zw;
  if (uLogoK.x > 0.0 && lp.x > 0.0 && lp.x < 1.0 && lp.y > 0.0 && lp.y < 1.0) {
    vec2 P = lp * uLogoBox.zw;           /* the sound's ripples on the water under the letters, faint above it */
    float hh = uLogoBox.w * LG_H;
    for (int i = 0; i < 6; i++) {
      vec4 R = uRip[i];
      if (R.y <= 0.0) continue;
      vec2 d = P - vec2(R.z * uLogoBox.z, LG_WL * uLogoBox.w);
      float r = length(d) + 1e-3, F = R.x * hh * 0.9;
      P += d / r * sin((r - F) / hh * 40.0) * exp(-pow((r - F) / (hh * 0.2), 2.0)) * exp(-R.x * 1.2) * R.y * hh * 0.05
           * (lp.y < LG_WL ? (lp.y > LG_LO ? uLogoK.y : 0.0) : 0.2 * uLogoK.y);
    }
    vec4 m = textureLod(uLogo, P / uLogoBox.zw, uLogoK.w);   /* (its level given: no derivatives at the box's edge) */
    vec2 ed = min(lp, 1.0 - lp) * vec2(LG_PX);   /* (the halo gone before the box's edge: no frame round it) */
    float glow = textureLod(uLogo, lp, 4.0).a * smoothstep(0.0, 40.0, min(ed.x, ed.y));
    c *= 1.0 - 0.5 * glow * uLogoK.x;
    c += glow * uLogoK.z * uTint * uLogoK.x;
    c = c * (1.0 - m.a * uLogoK.x) + m.rgb * uLogoK.x;
  }

  vec2 gp = floor(gl_FragCoord.xy / max(1.0, uDpr * (0.75 + 1.5 * uTitle.z)));   /* (BREATH's GRAIN: coarser) */
  uint s0 = uint(gp.x) * 73856093u ^ uint(gp.y) * 19349663u ^ uint(uFrame) * 83492791u;
  float n = (float(hsh(s0)) + float(hsh(s0 + 1u)) + float(hsh(s0 + 2u))) / 4294967296.0 / 3.0 - 0.5;
  float L = dot(c, vec3(0.299, 0.587, 0.114));
  c += n * uGrade.x * (1.25 - L) + 0.01;
  o = vec4(c, 1.0);
}`;

  const TAU = Math.PI * 2;
  const ease = (a, b, tau, dt) => a + (b - a) * (1 - Math.exp(-dt / Math.max(1e-4, tau)));
  const clamp = (x, a = 0, b = 1) => Math.max(a, Math.min(b, x));
  let seed = 1;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  const hashStr = (s) => { let h = 2166136261; for (const ch of s || "") h = Math.imul(h ^ ch.charCodeAt(0), 16777619); return (h >>> 0) % 2147483646 + 1; };
  const pick = (a) => a[Math.floor(rnd() * a.length)];

  function create(canvas) {
    const none = { ok: false, draw() {}, gesture() {} };
    const gl = canvas && canvas.getContext("webgl2", { antialias: false, alpha: false, depth: false, stencil: false, powerPreference: "high-performance" });
    if (!gl) return none;
    const float = !!gl.getExtension("EXT_color_buffer_float");
    const FMT = float ? [gl.RGBA16F, gl.HALF_FLOAT] : [gl.RGBA8, gl.UNSIGNED_BYTE];

    function prog(fs) {
      const p = gl.createProgram();
      for (const [type, src] of [[gl.VERTEX_SHADER, VS], [gl.FRAGMENT_SHADER, fs]]) {
        const s = gl.createShader(type);
        gl.shaderSource(s, src);
        gl.compileShader(s);
        if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s));
        gl.attachShader(p, s);
      }
      gl.bindAttribLocation(p, 0, "a");
      gl.linkProgram(p);
      if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(p));
      const u = {};
      for (let i = 0, n = gl.getProgramParameter(p, gl.ACTIVE_UNIFORMS); i < n; i++) {
        const name = gl.getActiveUniform(p, i).name;
        u[name.replace(/\[0\]$/, "")] = gl.getUniformLocation(p, name);
      }
      return { p, u };
    }
    let PF, PD, PU, PO;
    try { PF = prog(FIELD); PD = prog(DOWN); PU = prog(UP); PO = prog(OUT); }
    catch (e) { console.error("VISUALS", e.message); return none; }
    const vao = gl.createVertexArray();
    gl.bindVertexArray(vao);
    gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);

    function texture(w, h, fmt, type, src) {
      const t = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, t);
      if (src) {
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, src);
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
      } else gl.texImage2D(gl.TEXTURE_2D, 0, fmt, w, h, 0, gl.RGBA, type, null);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      return t;
    }
    function target(w, h) {
      const t = texture(w, h, FMT[0], FMT[1]), f = gl.createFramebuffer();
      gl.bindFramebuffer(gl.FRAMEBUFFER, f);
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, t, 0);
      gl.clearColor(0, 0, 0, 1);
      gl.clear(gl.COLOR_BUFFER_BIT);
      return { t, f, w, h };
    }
    const drop = (r) => { if (r) { gl.deleteTexture(r.t); gl.deleteFramebuffer(r.f); } };
    function pass(pr, tgt, set) {
      gl.bindFramebuffer(gl.FRAMEBUFFER, tgt ? tgt.f : null);
      gl.viewport(0, 0, tgt ? tgt.w : canvas.width, tgt ? tgt.h : canvas.height);
      gl.useProgram(pr.p);
      set(pr.u);
      gl.drawArrays(gl.TRIANGLES, 0, 3);
    }
    function bind(unit, tex, loc) {
      gl.activeTexture(gl.TEXTURE0 + unit);
      gl.bindTexture(gl.TEXTURE_2D, tex);
      gl.uniform1i(loc, unit);
    }

    // ---------------------------------------------------------------- the logo, alive (the header's, logo.js), its address
    const lgc = document.createElement("canvas"), url = document.createElement("canvas");
    lgc.width = LG.W;
    lgc.height = LG.H;
    url.width = LG.w;
    url.height = LG.url;
    let urlDrawn = false;
    function drawUrl() {                 /* FRASILSYNTH.COM, F to L under the letters (perform.css's .brand .url, x3) */
      const x = url.getContext("2d"), s = "FRASILSYNTH.COM";
      x.clearRect(0, 0, url.width, url.height);
      x.font = '600 48px "Barlow Semi Condensed", system-ui, sans-serif';
      x.letterSpacing = "0px";
      const w0 = x.measureText(s).width;
      x.letterSpacing = `${((694.8 - w0) / (s.length - 1)).toFixed(2)}px`;
      x.fillStyle = "#8b95a4";
      x.textBaseline = "alphabetic";
      x.fillText(s, 50.5, 42);
      urlDrawn = true;
    }
    const logoTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, logoTex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    function logoFrame() {               /* the header's logo as it is now, into the texture (premultiplied, mipmapped) */
      const src = document.querySelector(".brand canvas.logo"), x = lgc.getContext("2d");
      if (!urlDrawn) drawUrl();
      x.clearRect(0, 0, LG.W, LG.H);
      if (src) x.drawImage(src, LG.pad, LG.pad, LG.w, LG.h);
      x.drawImage(url, LG.pad, LG.pad + LG.h + LG.gap);
      gl.bindTexture(gl.TEXTURE_2D, logoTex);
      gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
      gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, lgc);
      gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
      gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
      gl.generateMipmap(gl.TEXTURE_2D);
    }


    // ---------------------------------------------------------------- the name (red sharp, green and blue blurred)
    const tc = document.createElement("canvas");
    let textTex = null, titleKey = "";
    function drawTitle(name, artist) {
      const w = Math.max(16, Math.round(canvas.width / 2)), h = Math.max(16, Math.round(canvas.height / 2));
      tc.width = w;
      tc.height = h;
      const x = tc.getContext("2d"), fs = Math.round(Math.min(h * 0.064, w * 0.05));
      x.fillStyle = "#000";
      x.fillRect(0, 0, w, h);
      x.globalCompositeOperation = "lighter";
      x.textAlign = "center";
      x.textBaseline = "middle";
      const line = (t, size, weight, y, sp) => {
        x.font = `${weight} ${size}px "Barlow Semi Condensed", system-ui, sans-serif`;
        x.letterSpacing = `${Math.round(sp * size)}px`;
        x.fillText(t, w / 2 + sp * size / 2, y);
      };
      for (const [col, blur] of [["#ff0000", 0], ["#00ff00", fs * 0.14], ["#0000ff", fs * 0.5]]) {
        x.filter = blur ? `blur(${blur.toFixed(1)}px)` : "none";
        x.fillStyle = col;
        if (name) line(name, fs, 700, h * 0.835, 0.36);
        if (name && artist) line(`BY ${artist}`, Math.round(fs * 0.34), 600, h * 0.835 + fs * 0.92, 0.55);
      }
      if (textTex) gl.deleteTexture(textTex);
      textTex = texture(0, 0, 0, 0, tc);
    }

    // ---------------------------------------------------------------- the targets, the window's size
    const Q = { px: 1.05e6, slow: 0, slowT: 0 };
    let field = [null, null], down = [], up = [], cur = 0, asp = 16 / 9;
    function size() {
      const dpr0 = Math.min(2, devicePixelRatio || 1), cw = canvas.clientWidth || innerWidth, ch = canvas.clientHeight || innerHeight;
      const dpr = Math.min(dpr0, Math.sqrt(4.4e6 / Math.max(1, cw * ch)));
      const w = Math.max(16, Math.round(cw * dpr)), h = Math.max(16, Math.round(ch * dpr));
      if (w === canvas.width && h === canvas.height && field[0] && field[0].px === Q.px) return;
      canvas.width = w;
      canvas.height = h;
      asp = w / h;
      const sw = Math.round(Math.sqrt(Q.px * asp)), sh = Math.max(8, Math.round(sw / asp));
      field.forEach(drop);
      down.forEach(drop);
      up.forEach(drop);
      field = [target(sw, sh), target(sw, sh)];
      field[0].px = field[1].px = Q.px;
      down = [];
      for (let k = 1, ww = sw, hh = sh; k <= 4; k++) down.push(target(ww = Math.max(2, ww >> 1), hh = Math.max(2, hh >> 1)));
      up = [down[0], down[1], down[2]].map((d) => target(d.w, d.h));
      titleKey = "";
    }

    // ---------------------------------------------------------------- what is heard
    const BANDS = [20, 90, 250, 900, 3000, 8000, 16000];
    const H = { f: null, prev: null, t: null, bin: null, peak: [0, 0, 0, 0, 0, 0], band: [0, 0, 0, 0, 0, 0], flux: 0, fluxAvg: 0,
                lastOn: 0, level: 0, cen: 0.4, onset: 0, on: false };
    function hear(an, dt, now, f) {
      H.onset = 0;
      if (!an) {                         /* nothing heard: what sounds, from the frame (the tracks' meters, the notes) */
        H.on = false;
        const m = (i) => f[42 + i] || 0, gl4 = Math.max(f[20], f[21], f[22], f[23], 0);
        const want = [f[25] || 0, Math.max(f[25] || 0, m(0) * 0.6), m(0), m(0) * 0.7 + gl4 * 0.3, gl4 * 0.8, gl4 * 0.5];
        for (let b = 0; b < 6; b++) H.band[b] = ease(H.band[b], clamp(want[b]), want[b] > H.band[b] ? 0.05 : 0.4, dt);
        H.level = ease(H.level, clamp((m(0) + m(1) + m(2) + m(3)) * 0.6), 0.2, dt);
        return;
      }
      H.on = true;
      const n = an.frequencyBinCount, hz = an.context.sampleRate / an.fftSize;
      if (!H.f || H.f.length !== n) {
        H.f = new Float32Array(n);
        H.prev = new Float32Array(n);
        H.t = new Float32Array(an.fftSize);
        H.bin = new Int8Array(n).map((_, i) => { const q = i * hz; for (let b = 0; b < 6; b++) if (q >= BANDS[b] && q < BANDS[b + 1]) return b; return -1; });
      }
      an.getFloatFrequencyData(H.f);
      an.getFloatTimeDomainData(H.t);
      const e = [0, 0, 0, 0, 0, 0], cnt = [0, 0, 0, 0, 0, 0];
      let flux = 0, num = 0, den = 0;
      for (let i = 1; i < n; i++) {
        const db = H.f[i], m = db > -140 ? Math.pow(10, db / 20) : 0;
        if (m > H.prev[i]) flux += m - H.prev[i];
        H.prev[i] = m;
        num += i * hz * m;
        den += m;
        const b = H.bin[i];
        if (b >= 0) { e[b] += m * m; cnt[b]++; }
      }
      for (let b = 0; b < 6; b++) {
        const a = Math.sqrt(e[b] / Math.max(1, cnt[b]));
        H.peak[b] = Math.max(a, H.peak[b] * Math.exp(-dt / 25), 2e-4);   /* (the loudest of the last half minute, never silence's) */
        const v = Math.pow(clamp(a / H.peak[b]), 1.4);
        H.band[b] = ease(H.band[b], v, v > H.band[b] ? 0.03 : 0.3, dt);
      }
      let s = 0;
      for (const x of H.t) s += x * x;
      const db = 20 * Math.log10(Math.sqrt(s / H.t.length) + 1e-9);
      H.level = ease(H.level, clamp((db + 58) / 46), 0.12, dt);
      if (den > 0) H.cen = ease(H.cen, clamp(Math.log2(num / den / 200) / Math.log2(30)), 0.3, dt);
      H.fluxAvg = ease(H.fluxAvg, flux, 0.6, dt);
      if (flux > H.fluxAvg * 1.7 && flux > 0.02 && now - H.lastOn > 140 && H.level > 0.15) { H.onset = clamp(flux / (H.fluxAvg * 4 + 1e-6)); H.lastOn = now; }
    }

    // ---------------------------------------------------------------- the state: voices, sparks, whirls, chapters
    const LFO_SHAPE = [94, 102, 115], LFO_DEPTH = [96, 104, 117];   /* (LFO 1-3's SHAPE and DEPTH; 5: RANDOM) */
    const LFO_SLOW = 2.5;                /* (the rings follow the LFOs this many times slower: the owner, too quick) */
    const T = { t: 0, frame: 0, last: 0, kick: 0, flash: 0, textE: 0, titleA: 0.4, sharp: 0.15, chapterAt: 0, scene: "", fps: [] };
    const D = Array.from({ length: 8 }, () => ({ note: -1, r: 0.3, rt: 0.3, lv: 0, ang: Math.random() * TAU, temp: 0.3, tt: 0.3 }));
    const G = Array.from({ length: 4 }, () => ({ n: -1, age: 9 }));
    const B = { n: -1, l: 0, x: 0, xt: 0 };
    const sparks = [], whirls = [], rips = [], echoes = [];   /* echoes: GLINT's delay, struck again later */
    const DL = { time: 0.4, fb: 0, mix: 0 };
    const W = { spin: 0.12, weave: 0, arc: 3.2, k: 0, depth: 0, mirror: 0, rotDir: 1, zoomDir: 1, flowScale: 2.4, wander: 0.05, spread: 0 };
    let want = { ...W, kT: 0, depthT: 0 };
    const moods = [];
    function chapter(s) {
      if (s) seed = s;
      const ring = rnd() < 0.45;
      want = {
        spin: 0.05 + rnd() * 0.22, weave: rnd() < 0.4 ? 1 : 0, arc: ring ? 3.2 : pick([1.6, 1.0, 0.6, 0.35]),
        kT: pick([0, 0, 2, 3, 4, 5, 6]), depthT: 0.02 + rnd() * 0.07, mirror: rnd() < 0.22 ? 0.5 + rnd() * 0.5 : 0,
        rotDir: rnd() < 0.5 ? -1 : 1, zoomDir: rnd() < 0.78 ? 1 : -1, flowScale: 1.2 + rnd() * 3.4, wander: 0.015 + rnd() * 0.09,
        spread: rnd() < 0.5 ? 0 : 0.03 + rnd() * 0.13,   /* the rings off each other's centre: not a tunnel, a cluster */
      };
      if (!want.kT) want.depthT = 0;
      T.chapterAt = T.t + 50 + rnd() * 60;
      if (s) {                           /* a scene's own currents: its phases and periods */
        moods.length = 0;
        for (let i = 0; i < 8; i++) moods.push([61 + rnd() * 28, 23 + rnd() * 14, 131 + rnd() * 66, rnd() * TAU, rnd() * TAU, rnd() * TAU]);
      }
    }
    chapter(hashStr("FRASIL"));
    const mood = (i) => { const m = moods[i], t = T.t; return 0.5 * Math.sin(TAU * t / m[0] + m[3]) + 0.3 * Math.sin(TAU * t / m[1] + m[4]) + 0.2 * Math.sin(TAU * t / m[2] + m[5]); };
    const radius = (n) => clamp(0.44 * Math.pow(2, -(n - 36) / 24), 0.06, 0.47);
    const C = { x: 0, y: 0 };
    function strike(n, lv) {
      const a = Math.PI / 2 - (n % 12) / 12 * TAU, r = clamp(0.1 + (Math.floor(n / 12) - 4) * 0.075 + (n % 12) / 12 * 0.075, 0.05, 0.45);
      const x = C.x + Math.cos(a) * r, y = C.y + Math.sin(a) * r;
      sparks.push({ x, y, age: 0, e: lv });
      if (sparks.length > 24) sparks.shift();
      for (let k = 1, e = lv * DL.mix; k <= 5 && e > 0.05; k++, e *= DL.fb)   /* its echoes, ping-pong: left, right */
        echoes.push({ at: T.t + k * DL.time, x: x + (k % 2 ? -1 : 1) * 0.05 * Math.ceil(k / 2), y, e });
      if (echoes.length > 40) echoes.splice(0, echoes.length - 40);
      ripple(0.3 * lv);
    }
    function ripple(s, x = 0.32 + Math.random() * 0.36) {
      rips.push({ age: 0, s, x });
      if (rips.length > 6) rips.shift();
    }
    function gesture(kind, d = {}) {
      if (kind === "knob" || kind === "hand") {
        const cell = kind === "hand" ? -1 : d.cell, by = d.by || 0;
        let w = whirls.find((x) => x.cell === cell && x.age < 1.2);
        if (!w) {
          w = kind === "hand" ? { cell, x: C.x, y: C.y, rad: 0.3, tw: 0, age: 0 }
            : { cell, x: (cell - 1.5) * 0.23 * asp, y: (cell % 2 ? 0.07 : -0.07), rad: 0.17, tw: 0, age: 0 };
          whirls.push(w);
          if (whirls.length > 6) whirls.shift();
        }
        w.tw = clamp(w.tw + by * 1.1, -0.11, 0.11);
        w.age = 0;
      } else if (kind === "scene") {
        T.textE = 1;
        T.titleA = 1;
        T.sharp = 0.75;
        T.flash = Math.max(T.flash, 0.5);
        sparks.push({ x: C.x, y: C.y, age: 0, e: 0.9 });
        ripple(1, 0.5);
        chapter(hashStr(d.name));
      } else if (kind === "page") {
        sparks.push({ x: C.x, y: C.y, age: 0.14, e: 0.25 });
      } else if (kind === "button") {
        if (d.key === "OCT+") T.kick += 0.9;
        else if (d.key === "OCT-") T.kick -= 0.9;
        else T.flash = Math.max(T.flash, 0.2);
      }
      if (sparks.length > 24) sparks.shift();
    }

    // ---------------------------------------------------------------- a frame
    const U = { dr: new Float32Array(32), dr2: new Float32Array(32), sp: new Float32Array(96), vx: new Float32Array(24), rip: new Float32Array(24) };
    function draw(f, now, s) {
      size();
      const dt = clamp((now - (T.last || now)) / 1000, 0.001, 0.1), k = dt * 60;
      T.last = now;
      T.t += dt;
      T.frame++;
      quality(dt);
      const N = (p) => (s && s.n ? clamp(s.n(p)) : 0);
      hear(s && s.an, dt, now, f);
      const choir = N(0), partials = N(1), breath = N(2), warmth = N(3), motion = N(16), rate = N(17);
      const size_ = N(20), decay = N(21), shimmer = N(23), diff = N(24), timbre = N(31), space = N(32);
      const bright = N(39), ping = N(50), echo = N(52) * N(54), drive = N(55);
      const mixD = N(73), mixB = N(74), mixG = N(75), mixC = N(76);
      const sparkle = N(124), blur = N(125), sink = N(126), swell = N(ID.P_SWELL);
      const ftype = Math.round(N(138) * 2), cutoff = N(139), reso = N(140), tape = N(142) > 0.5, sat = N(143), wob = N(144);
      T.fz = ease(T.fz || 0, N(ID.P_FREEZE) > 0.5 ? 1 : 0, 0.6, dt);
      const fz = 1 - 0.96 * T.fz;           /* FREEZE: what moves, as much as it still does */
      const [sub, lows, lmid, , high, air] = H.band;
      const detune = N(4), cmod = N(25), cwidth = N(29), vib = N(63), bfilt = N(56), bres = N(64), beat = N(86);
      /* WOBBLE's shockwave: one each time the tape drags (heard: slower a moment), as strong as the drag is deep */
      const drag = f[52] || 0;
      if (tape && drag > 0.02 && !(T.dragWas > 0.02)) { T.shockA = 0; T.shockS = Math.min(1, 0.35 + 0.9 * drag); }
      T.dragWas = drag;
      T.shockA = Math.min(1, (T.shockA === undefined ? 1 : T.shockA) + dt / 1.6);
      const shape = N(81), bgrain = N(89) * Math.min(1, breath * 2), ch = Math.min(1, choir * 2);
      DL.time = 0.14 + N(51) * 1.0;
      DL.fb = 0.25 + 0.6 * N(52);
      DL.mix = 0.9 * N(54);

      /* the chapters, the currents */
      if (T.t > T.chapterAt) chapter();
      for (const key of ["spin", "weave", "arc", "mirror", "rotDir", "zoomDir", "flowScale", "wander", "spread"]) W[key] = ease(W[key], want[key], 7, dt);
      if (W.k !== want.kT) { W.depth = ease(W.depth, 0, 1.2, dt); if (W.depth < 0.003) W.k = want.kT; }   /* (lobes change only flat) */
      else W.depth = ease(W.depth, want.depthT, 4, dt);
      const lfo = (i) => (f[26 + i] > 0 ? f[32 + i] || 0 : 0);
      /* the rings' place: the LFOs move it (1 across, 2 up and down, 3 on the diagonal; each as far as its DEPTH), their
       * shapes drawn LFO_SLOW times slower (their turns and places, the page's lfo_shape), eased (a square, a saw's
       * edge: a glide, not a jump), RANDOM's steps left out (it would leap about); a little drift */
      T.lf = T.lf || [0, 0, 0];
      for (let i = 0; i < 3; i++) {
        const sh = Math.round(N(LFO_SHAPE[i]) * 6), on = f[26 + i] > 0 && sh !== 5 && window.PerformShow;
        const at = on ? ((f[35 + i] || 0) + (f[29 + i] || 0)) / LFO_SLOW : 0, c = Math.floor(at);
        T.lf[i] = ease(T.lf[i], on ? PerformShow.lfoShape(sh, at - c, c, i) * (0.4 + 0.6 * N(LFO_DEPTH[i])) : 0, 0.8, dt);
      }
      C.x = ease(C.x, clamp(W.wander * 0.5 * mood(3) * asp + (0.22 * T.lf[0] + 0.1 * T.lf[2]) * asp, -0.3 * asp, 0.3 * asp), 0.6, dt);
      C.y = ease(C.y, clamp(W.wander * 0.35 * mood(4) + 0.16 * T.lf[1] + 0.09 * T.lf[2], -0.24, 0.24), 0.6, dt);

      /* the drones */
      let low = 200;
      for (let v = 0; v < 8; v++) if (f[v] >= 0 && f[8 + v] > 0.003) low = Math.min(low, f[v]);
      const tempMood = 0.35 * mood(0), gD = 0.35 + 0.9 * mixD;
      let warmSum = 0, lvSum = 0;
      for (let v = 0; v < 8; v++) {
        const d = D[v], n = f[v], lv = n >= 0 ? Math.sqrt(clamp(f[8 + v])) : 0;
        if (n >= 0) {
          if (d.note < 0 || d.lv < 0.01) d.r = radius(n);
          d.note = n;
          d.rt = radius(n) * (1 + 0.03 * lfo(2));
          const tq = clamp(0.2 + (warmth - 0.5) * 0.6 + (45 - n) / 30 + 0.6 * tempMood);   /* (the cold first: under A2 it burns) */
          d.tt = tq * tq * (3 - 2 * tq) * 0.9 + (tq > 0.5 ? 0.1 : 0);   /* (each voice fire or ice, rarely between) */
        }
        d.r = ease(d.r, d.rt, 0.3, dt);
        d.lv = ease(d.lv, lv * gD, lv > d.lv ? 0.06 : 0.7, dt);
        if (n < 0 && d.lv < 0.004) d.note = -1;
        d.temp = ease(d.temp, d.tt, 1.2, dt);
        if (d.note >= 0) {
          const ratio = Math.pow(2, (((d.note - low) % 12) + 12) % 12 / 12);
          d.ang += dt * (W.spin * (0.4 + 1.2 * rate)) * ratio * (v % 2 && W.weave > 0.5 ? -1 : 1) * W.rotDir * fz;
        }
        const on = d.note >= 0 && d.lv > 0.002;
        U.dr.set([d.r, on ? d.lv : 0, d.ang, d.temp], v * 4);
        const oa = v * 2.39996 + T.t * 0.03 * (v % 2 ? 1 : -1), os = W.spread * (1 + 0.3 * mood(5));
        U.dr2.set([W.arc, Math.cos(oa) * os, Math.sin(oa) * os, 0.08 + 0.5 * timbre + 0.25 * H.cen], v * 4);
        if (on) { warmSum += d.temp * d.lv; lvSum += d.lv; }
      }

      /* GLINT, the bass */
      for (let g = 0; g < 4; g++) {
        const n = f[16 + g], lv = f[20 + g], age = f[48 + g], p = G[g];
        if (n >= 0 && lv > 0.05 && age < 0.12 && (n !== p.n || age < p.age - 0.02)) strike(n, lv * (0.45 + 0.8 * mixG) * (0.6 + 0.6 * bright));
        p.n = n;
        p.age = age;
      }
      const bn = f[24], bl = bn >= 0 ? clamp(f[25]) : 0;
      if (bn >= 0 && bn !== B.n && bl > 0.05) { B.xt = ((bn % 12) / 11 - 0.5) * 0.6 * asp; T.kick += 0.5 * bl; ripple(0.8 * bl); }
      B.n = bn;
      B.l = ease(B.l, Math.sqrt(bl) * (0.35 + 0.9 * mixB), bl > B.l ? 0.05 : 0.5, dt);
      B.x = ease(B.x, B.xt, 0.8, dt);
      if (H.onset > 0.2) { T.flash = Math.max(T.flash, 0.35 * H.onset); if (H.onset > 0.5) ripple(0.25 * H.onset); }

      /* the sparks, the whirls, the logo's ripples age */
      for (let i = echoes.length - 1; i >= 0; i--)
        if (T.t >= echoes[i].at) {
          sparks.push({ x: echoes[i].x, y: echoes[i].y, age: 0, e: echoes[i].e });
          if (sparks.length > 24) sparks.shift();
          echoes.splice(i, 1);
        }
      U.sp.fill(0);
      for (let i = sparks.length - 1; i >= 0; i--) if ((sparks[i].age += dt * (1 - 0.75 * T.fz)) > 3) sparks.splice(i, 1);
      sparks.forEach((p, i) => {          /* (GLINT's VIBRATO: they tremble) */
        const j = vib * 0.012 * Math.sin(p.age * 38 + i * 2.1);
        U.sp.set([p.x + j, p.y + 0.6 * j, p.age, p.e], i * 4);
      });
      U.vx.fill(0);
      for (let i = whirls.length - 1; i >= 0; i--) if ((whirls[i].age += dt) > 3) whirls.splice(i, 1);
      whirls.forEach((w, i) => U.vx.set([w.x, w.y, w.tw * Math.exp(-w.age * 1.8) * k, w.rad], i * 4));
      U.rip.fill(0);
      for (let i = rips.length - 1; i >= 0; i--) if ((rips[i].age += dt) > 4) rips.splice(i, 1);
      rips.forEach((r, i) => U.rip.set([r.age, r.s, r.x, 0], i * 4));
      T.kick = ease(T.kick, 0, 0.35, dt);
      T.flash = ease(T.flash, 0, 0.25, dt);
      T.textE = ease(T.textE, 0, 1.1, dt);
      T.titleA = ease(T.titleA, 0.5 + 0.16 * H.level, T.titleA > 0.6 ? 7 : 2, dt);
      T.sharp = ease(T.sharp, 0.2 + 0.12 * H.level, 6, dt);
      const name = (s && s.scene) || "", artist = (s && s.artist) || "";
      if (`${name}|${artist}|${canvas.width}` !== titleKey) { titleKey = `${name}|${artist}|${canvas.width}`; drawTitle(name, artist); }
      const bare = !!(s && s.bare);
      T.over = ease(T.over === undefined ? 1 : T.over, bare ? 0 : 1, 0.6, dt);

      /* the field */
      const turn = ((0.0006 + 0.002 * Math.abs(mood(1))) * W.rotDir + 0.0008 * lfo(0)) * k * fz;
      const zoom = ((0.0005 + 0.0024 * Math.max(size_, space)) * W.zoomDir + 0.0006 * swell * Math.sin(T.t * 0.7)
                    + 0.0022 * (sub + lows) * 0.5 + 0.004 * T.kick) * k * fz;
      const flow = (0.0003 + 0.0022 * motion + 0.0007 * lmid) * k * fz;
      const keep = 0.94 + 0.042 * decay + 0.006 * mixC, iceKeep = Math.pow(keep + (0.994 - keep) * T.fz, k);
      /* (what a frame keeps, always under 1: over it a channel grows on itself and the screen goes white for good) */
      const fireKeep = Math.pow(0.92 + 0.03 * drive, k), froKeep = Math.pow(Math.min(0.97, 0.86 + 0.1 * echo + 0.03 * shimmer + 0.04 * T.fz), k);
      const prev = field[cur], next = field[1 - cur];
      pass(PF, next, (u) => {
        bind(0, prev.t, u.uPrev);
        bind(1, textTex, u.uText);
        gl.uniform2f(u.uTx, 1 / prev.w, 1 / prev.h);
        gl.uniform1f(u.uAsp, asp);
        T.ft = (T.ft || 0) + dt * (0.04 + 0.12 * rate) * 4 * fz;   /* (the flow's own time: FREEZE stops it) */
        gl.uniform1f(u.uT, T.ft);
        gl.uniform1f(u.uFrame, T.frame % 65536);
        gl.uniform2f(u.uC, C.x, C.y);
        gl.uniform4f(u.uIce, turn, zoom, flow, W.flowScale);
        gl.uniform4f(u.uFire, (0.0016 + 0.002 * drive + 0.0012 * lows) * k * fz, (0.0012 + 0.0018 * drive) * k * fz, 3.2,
                     1 - 0.55 * clamp(diff * 0.6 + blur) * (1 - T.fz));
        gl.uniform4f(u.uKeep, iceKeep, fireKeep, froKeep, 0.35 + 0.7 * diff + 1.1 * blur);
        gl.uniform4f(u.uMix, 0.06 * k, W.mirror, 0.0012 * sink * k * fz, 0.016 * k * (1 - 0.7 * T.fz));
        gl.uniform4fv(u.uDr, U.dr);
        gl.uniform4fv(u.uDr2, U.dr2);
        gl.uniform2f(u.uLobe, W.k, W.depth);
        gl.uniform4f(u.uLay, choir, partials, breath, 0.6 + 0.9 * ping);
        gl.uniform4fv(u.uSp, U.sp);
        gl.uniform4fv(u.uVx, U.vx);
        gl.uniform4f(u.uBass, B.l, B.x, drive, (sub + lows) * 0.5);
        const pw = shape < 0.5 ? 2 + 1.6 * shape : 2.8 - 2.8 * (shape - 0.5);   /* (SHAPE as CHOIR sounds: a squircle at most, a soft diamond; never a square's straight sides) */
        gl.uniform4f(u.uShape, 2 + (pw - 2) * ch, 0.014 * detune * ch, 1 + 0.45 * cwidth, cmod);
        gl.uniform4f(u.uLay2, beat * Math.min(1, partials * 2), bfilt, bres, 0);
        gl.uniform4f(u.uMisc, (0.000001 + 0.00003 * (high + air) * 0.5 + 0.00006 * sparkle + 0.00002 * shimmer) * k, T.textE, 0, 0);
      });
      cur = 1 - cur;

      /* the bloom */
      let src = next;
      for (const d of down) { pass(PD, d, (u) => { bind(0, src.t, u.uSrc); gl.uniform2f(u.uH, 0.5 / src.w, 0.5 / src.h); }); src = d; }
      for (let i = 2; i >= 0; i--) {
        const from = i === 2 ? down[3] : up[i + 1];
        pass(PU, up[i], (u) => { bind(0, from.t, u.uSrc); bind(1, down[i].t, u.uAdd); gl.uniform2f(u.uH, 0.5 / from.w, 0.5 / from.h); });
      }

      /* the picture */
      const warm = lvSum > 0 ? warmSum / lvSum : 0.2, heatMix = clamp(warm * 0.6 + B.l * 0.5);
      const sc = canvas.height * 0.27 / LG.w, m = canvas.height * 0.05;   /* (top right: the letters m from the right, from the top) */
      const lx = Math.round(canvas.width - m - (LG.pad + LG.w) * sc), ly = Math.round(canvas.height - m + LG.pad * sc - LG.H * sc);
      if (T.over > 0.01) logoFrame();
      pass(PO, null, (u) => {
        bind(0, next.t, u.uField);
        bind(1, up[0].t, u.uBloom);
        bind(2, textTex, u.uText);
        bind(3, logoTex, u.uLogo);
        gl.uniform1f(u.uAsp, asp);
        gl.uniform1f(u.uT, T.t);
        gl.uniform1f(u.uFrame, T.frame % 65536);
        gl.uniform1f(u.uDpr, canvas.width / Math.max(1, canvas.clientWidth || innerWidth));
        gl.uniform4f(u.uLook, 0.62 + 0.38 * H.level, (0.1 + 0.22 * mixC + 0.12 * space) * (ftype === 2 ? 0.5 + 0.5 * (1 - cutoff) : 1),
                     0.25 + 1.6 * T.flash + (ftype ? 0.8 * reso : 0), ftype === 1 ? 0.85 * (1 - cutoff) : 0);
        gl.uniform4f(u.uGrade, 0.095 + 0.2 * bgrain + (tape ? 0.03 : 0) + 0.03 * air, tape ? 0.3 + wob : 0, T.flash, sink);
        gl.uniform4f(u.uTitle, T.titleA * T.over, T.sharp, bgrain, 0);
        gl.uniform4f(u.uTape, tape ? sat : 0, tape && T.shockA < 1 ? T.shockS : 0, Math.min(1, T.shockA ?? 1), 0);
        gl.uniform4f(u.uLogoBox, lx, ly, LG.W * sc, LG.H * sc);
        gl.uniform4f(u.uLogoK, 0.96 * T.over, 0.6 + 1.2 * H.level, 0.16 + 0.45 * H.level, Math.max(0, Math.log2(1 / sc)));
        gl.uniform3f(u.uTint, 0.12 + 0.9 * heatMix, 0.42 - 0.12 * heatMix, 1 - 0.85 * heatMix);
        gl.uniform4fv(u.uRip, U.rip);
        gl.uniform1f(u.uFrost, T.fz);
      });
    }
    /* too slow for this computer (under ~45 a second for two seconds): a smaller field */
    function quality(dt) {
      Q.slowT += dt;
      if (dt > 1 / 45) Q.slow += dt;
      if (Q.slowT < 2) return;
      if (Q.slow > 1.2 && Q.px > 0.4e6) Q.px = Math.round(Q.px * 0.75);
      Q.slow = Q.slowT = 0;
    }
    const vis = { ok: true, draw, gesture, get heard() { return H.on ? H.level : -1; } };
    api.last = vis;
    return vis;
  }
  const api = { create, last: null };    /* last: the one made (the checks look at it) */
  return api;
})();
