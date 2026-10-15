#pragma once
/* Frame-rate independent easing helpers. dt in seconds. */
static inline float lerp_to(float cur, float target, float speed, float dt) {
    float k = speed * dt; if (k > 1) k = 1; return cur + (target - cur) * k;
}
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline float ease_out(float t) { t = clampf(t, 0, 1); return 1 - (1 - t) * (1 - t) * (1 - t); }
