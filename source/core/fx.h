// Screen effects shared by every scene: camera shake (with controller rumble),
// light flashes, shockwaves, spark bursts, gold-coin showers, confetti, drifting
// smoke and a fine film grain over the whole picture.
#pragma once

#include "gfx.h"

namespace fx {

void init();
void shutdown();
void update(float dt);
// Drops everything in flight (scene changes).
void clear();

// Main loop hooks: the scene is drawn between beginFrame/endFrame (shaken),
// render() draws particles on top of it, overlay() the film grain.
void beginFrame();
void render();
void endFrame();
void overlay();

// Adds camera trauma 0..1 (shake grows with its square) and rumbles the controllers.
void shake(float trauma, bool rumble = true);
void flash(Color c, float alpha = 0.6f, float seconds = 0.35f);
void shockwave(float x, float y, Color c, float radius = 240.f, float seconds = 0.65f);
void burst(float x, float y, int count, Color c, float speed = 420.f, float gravity = 700.f);
void coins(int count, float spread = 1.f);
void confetti(int count);
// Diagonal light sweep across a rectangle, t in 0..1 (call from render()).
void shine(float x, float y, float w, float h, float t, float alpha = 0.55f);

// Slow smoke drifting through a region (cigar haze over a table, hall atmosphere).
struct Haze {
    struct Puff { float x, y, vx, vy, r, a, spin, ang, life, age; };
    std::vector<Puff> puffs;
    float x = 0, y = 0, w = SCREEN_W, h = SCREEN_H;
    Color tint = Color(220, 210, 200);
    void init(int count, float x, float y, float w, float h, Color tint, uint32_t seed = 1);
    void update(float dt);
    void render(float alpha = 1.f) const;
};

// A wisp of smoke rising from a point (cigar, candle).
struct Wisp {
    struct Bit { float x, y, vx, r, age, life; };
    std::vector<Bit> bits;
    float timer = 0;
    void update(float dt, float x, float y, float rate = 6.f);
    void render(float alpha = 1.f) const;
};

} // namespace fx
