#include "fx.h"

#include "../art/shade.h"
#include "../art/svg.h"
#include "input.h"
#include "save.h"

namespace fx {

namespace {

enum Kind { K_SPARK, K_COIN, K_CONFETTI };

struct Particle {
    float x, y, vx, vy, gravity, drag;
    float age = 0, life;
    float size, rot, vr, flip, vflip;
    Color c;
    Kind kind;
};

struct Wave { float x, y, radius, age, life; Color c; };

Tex g_grain, g_smoke, g_coin;
std::vector<Particle> g_parts;
std::vector<Wave> g_waves;
float g_trauma = 0, g_time = 0;
float g_flash = 0, g_flashDur = 1, g_flashA = 0;
Color g_flashC;
float g_ox = 0, g_oy = 0;
SDL_Rect g_vp;
bool g_vpSaved = false;
int g_grainFrame = 0;
Rng g_rng(0xF00D);

bool effectsOn() { return save::data().settings.effects; }

Image buildGrain() {
    Image img(256, 256);
    Rng r(77);
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            float n = r.uniform(-1, 1);
            n = n * std::fabs(n);
            uint8_t v = n > 0 ? 255 : 0;
            uint8_t* p = img.at(x, y);
            p[0] = p[1] = p[2] = v;
            p[3] = (uint8_t)(std::fabs(n) * 255);
        }
    return img;
}

Image buildSmoke() {
    const int N = 128;
    Image img(N, N);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float dx = (x + 0.5f) / N - 0.5f, dy = (y + 0.5f) / N - 0.5f;
            float r = std::sqrt(dx * dx + dy * dy) * 2;
            float n = noise::fbm(x / 22.f, y / 22.f, 4, 913);
            float a = clamp01(1 - r) * clamp01(1 - r) * clamp01(n * 1.6f - 0.35f);
            uint8_t* p = img.at(x, y);
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(clamp01(a * 1.4f) * 255);
        }
    return img;
}

// A gold casino token with an embossed star, lit like the other metal art.
Image buildCoin() {
    const float D = 36, c = D / 2;
    std::vector<shade::Layer> L(3);
    L[0].svg = svg::open(D, D) + svg::circle(c, c, c - 1, "#e6b850") + svg::close();
    L[0].mat = shade::mat::gold();
    L[0].bevel = 3.5f;
    L[1].svg = svg::open(D, D) + svg::circle(c, c, c - 5, "#d9a840") + svg::close();
    L[1].mat = shade::mat::gold();
    L[1].bevel = 1.2f;
    L[1].depth = -0.7f;
    std::string star = "M";
    for (int i = 0; i < 10; i++) {
        float a = -PI / 2 + i * PI / 5, rr = i % 2 ? 4.2f : 9.5f;
        star += svg::num(c + std::cos(a) * rr) + " " + svg::num(c + std::sin(a) * rr) + (i < 9 ? " L" : " Z");
    }
    L[2].svg = svg::open(D, D) + svg::path(star, "#f0c860") + svg::close();
    L[2].mat = shade::mat::gold();
    L[2].bevel = 1.6f;
    return shade::relief(D, D, L);
}

void drawSpark(const Particle& p, float a) {
    float len = std::min(0.035f, 0.01f + p.age * 0.02f);
    float tx = p.x - p.vx * len, ty = p.y - p.vy * len;
    gfx::line(tx, ty, p.x, p.y, p.size, p.c.alpha(a), true);
    gfx::glow(p.x, p.y, p.size * 3.5f, p.c, a * 0.5f);
}

} // namespace

void init() {
    g_grain = gfx::upload(buildGrain(), 1.f);
    g_smoke = gfx::upload(buildSmoke(), 1.f);
    g_coin = gfx::upload(buildCoin());
}

void shutdown() {
    g_grain = Tex();
    g_smoke = Tex();
    g_coin = Tex();
    clear();
}

void clear() {
    g_parts.clear();
    g_waves.clear();
    g_trauma = 0;
    g_flash = 0;
}

void update(float dt) {
    g_time += dt;
    g_trauma = std::max(0.f, g_trauma - dt * 1.5f);
    float k = g_trauma * g_trauma;
    float amp = effectsOn() ? 16.f * k : 0.f;
    g_ox = amp * (noise::value(g_time * 28.f, 0.5f, 11) * 2 - 1);
    g_oy = amp * (noise::value(0.5f, g_time * 28.f, 12) * 2 - 1);
    if (g_flash > 0) g_flash = std::max(0.f, g_flash - dt);
    for (auto& p : g_parts) {
        p.age += dt;
        p.vy += p.gravity * dt;
        float d = std::exp(-p.drag * dt);
        p.vx *= d;
        p.vy = p.kind == K_CONFETTI ? std::min(p.vy * d, 140.f) : p.vy * d;
        if (p.kind == K_CONFETTI) p.vx += std::sin(p.age * 3 + p.rot) * 60 * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.rot += p.vr * dt;
        p.flip += p.vflip * dt;
    }
    g_parts.erase(std::remove_if(g_parts.begin(), g_parts.end(), [](const Particle& p) { return p.age >= p.life || p.y > SCREEN_H + 60; }),
                  g_parts.end());
    for (auto& w : g_waves) w.age += dt;
    g_waves.erase(std::remove_if(g_waves.begin(), g_waves.end(), [](const Wave& w) { return w.age >= w.life; }), g_waves.end());
}

void beginFrame() {
    SDL_Renderer* R = gfx::renderer();
    SDL_RenderGetViewport(R, &g_vp);
    g_vpSaved = true;
    if (std::fabs(g_ox) > 0.05f || std::fabs(g_oy) > 0.05f) {
        SDL_Rect v = g_vp;
        v.x += (int)std::lround(g_ox);
        v.y += (int)std::lround(g_oy);
        SDL_RenderSetViewport(R, &v);
    }
}

void endFrame() {
    if (g_vpSaved) SDL_RenderSetViewport(gfx::renderer(), &g_vp);
    g_vpSaved = false;
}

void render() {
    for (const Wave& w : g_waves) {
        float t = w.age / w.life;
        float e = 1 - (1 - t) * (1 - t) * (1 - t);
        float r = w.radius * e;
        float a = (1 - t);
        gfx::glow(w.x, w.y, r * 0.9f, w.c, 0.35f * a * a);
        gfx::ring(w.x, w.y, r, 3 + 16 * (1 - t), w.c.alpha(0.55f * a));
        gfx::ring(w.x, w.y, r * 0.82f, 2 + 6 * (1 - t), Color(255, 255, 255).alpha(0.3f * a));
    }
    for (const Particle& p : g_parts) {
        float a = clamp01((p.life - p.age) / 0.4f) * clamp01(p.age / 0.04f);
        switch (p.kind) {
            case K_SPARK: drawSpark(p, a); break;
            case K_COIN: {
                float sx = std::cos(p.flip);
                float lit = 0.55f + 0.45f * std::fabs(sx);
                gfx::drawCenteredXY(g_coin, p.x, p.y, std::max(0.08f, std::fabs(sx)) * p.size, p.size, p.rot, a,
                                    pal::white.scaled(lit));
                if (std::fabs(sx) > 0.94f) gfx::glow(p.x - 4 * p.size, p.y - 5 * p.size, 10 * p.size, Color(255, 240, 200), 0.5f * a);
                break;
            }
            case K_CONFETTI: {
                float w = p.size * std::max(0.15f, std::fabs(std::cos(p.flip))), h = p.size * 0.55f;
                float ca = std::cos(p.rot), sa = std::sin(p.rot);
                auto P = [&](float u, float v) { return SDL_FPoint{p.x + u * ca - v * sa, p.y + u * sa + v * ca}; };
                Color c = p.c.scaled(0.7f + 0.3f * std::fabs(std::cos(p.flip))).alpha(a);
                gfx::quad(P(-w / 2, -h / 2), P(w / 2, -h / 2), P(w / 2, h / 2), P(-w / 2, h / 2), c, c, c, c);
                break;
            }
        }
    }
    if (g_flash > 0) {
        float t = g_flash / g_flashDur;
        gfx::dim(g_flashA * t * t, g_flashC);
    }
}

void overlay() {
    if (!effectsOn() || !g_grain) return;
    // fine moving film grain, re-seeded at ~24 fps
    if (g_time * 24 > g_grainFrame) g_grainFrame++;
    Rng r((uint32_t)g_grainFrame * 2654435761u);
    float ox = -r.uniform(0, 256), oy = -r.uniform(0, 256);
    for (float y = oy; y < SCREEN_H; y += 256)
        for (float x = ox; x < SCREEN_W; x += 256) gfx::draw(g_grain, x, y, 0.045f);
}

void shake(float trauma, bool rumble) {
    g_trauma = std::min(1.f, g_trauma + trauma);
    if (rumble) input::rumble(ANY_PAD, std::min(1.f, 0.25f + trauma), 0.12f + trauma * 0.4f);
}

void flash(Color c, float alpha, float seconds) {
    if (!effectsOn()) alpha *= 0.4f;
    g_flashC = c;
    g_flashA = alpha;
    g_flash = g_flashDur = seconds;
}

void shockwave(float x, float y, Color c, float radius, float seconds) {
    g_waves.push_back({x, y, radius, 0, seconds, c});
}

void burst(float x, float y, int count, Color c, float speed, float gravity) {
    for (int i = 0; i < count; i++) {
        Particle p{};
        float a = g_rng.uniform(0, 2 * PI), s = speed * g_rng.uniform(0.35f, 1.f);
        p.x = x;
        p.y = y;
        p.vx = std::cos(a) * s;
        p.vy = std::sin(a) * s - speed * 0.25f;
        p.gravity = gravity;
        p.drag = 1.6f;
        p.life = g_rng.uniform(0.5f, 1.1f);
        p.size = g_rng.uniform(1.4f, 2.8f);
        p.c = lerpColor(c, Color(255, 255, 255), g_rng.uniform(0, 0.5f));
        p.kind = K_SPARK;
        g_parts.push_back(p);
    }
}

void coins(int count, float spread) {
    for (int i = 0; i < count; i++) {
        Particle p{};
        p.x = SCREEN_W / 2.f + g_rng.uniform(-1, 1) * SCREEN_W * 0.48f * spread;
        p.y = -30 - g_rng.uniform(0, 520);
        p.vx = g_rng.uniform(-60, 60);
        p.vy = g_rng.uniform(80, 260);
        p.gravity = 820;
        p.drag = 0.4f;
        p.life = 4.f;
        p.size = g_rng.uniform(0.75f, 1.15f);
        p.rot = g_rng.uniform(-20, 20);
        p.vr = g_rng.uniform(-90, 90);
        p.flip = g_rng.uniform(0, 6.28f);
        p.vflip = g_rng.uniform(6, 14);
        p.kind = K_COIN;
        g_parts.push_back(p);
    }
}

void confetti(int count) {
    static const uint32_t cols[] = {0xf3d77a, 0xe8434f, 0x3fa9ff, 0x4fd18b, 0xb86cff, 0xffffff, 0xff9a3c};
    for (int i = 0; i < count; i++) {
        Particle p{};
        p.x = g_rng.uniform(0, SCREEN_W);
        p.y = -20 - g_rng.uniform(0, 400);
        p.vx = g_rng.uniform(-40, 40);
        p.vy = g_rng.uniform(40, 140);
        p.gravity = 260;
        p.drag = 1.2f;
        p.life = 5.5f;
        p.size = g_rng.uniform(8, 14);
        p.rot = g_rng.uniform(0, 6.28f);
        p.vr = g_rng.uniform(-6, 6);
        p.flip = g_rng.uniform(0, 6.28f);
        p.vflip = g_rng.uniform(4, 11);
        p.c = Color::hex(cols[g_rng.range(0, 6)]);
        p.kind = K_CONFETTI;
        g_parts.push_back(p);
    }
}

void shine(float x, float y, float w, float h, float t, float alpha) {
    if (t <= 0 || t >= 1) return;
    float bw = std::max(26.f, w * 0.12f), sk = h * 0.45f;
    float cx = lerp(x - bw - sk, x + w + bw + sk, t);
    float peak = alpha * std::sin(t * PI);
    // soft parallelogram: transparent at its four edges, brightest along the middle
    const float vs[4] = {0, 0.3f, 0.7f, 1}, va[4] = {0, 1, 1, 0};
    const float us[3] = {-1, 0, 1}, ua[3] = {0, 1, 0};
    auto P = [&](int ui, int vi) { return SDL_FPoint{cx + us[ui] * bw + sk * (1 - 2 * vs[vi]), y + vs[vi] * h}; };
    auto C = [&](int ui, int vi) { return Color(255, 248, 225).alpha(peak * ua[ui] * va[vi]); };
    for (int vi = 0; vi < 3; vi++)
        for (int ui = 0; ui < 2; ui++)
            gfx::quad(P(ui, vi), P(ui + 1, vi), P(ui + 1, vi + 1), P(ui, vi + 1), C(ui, vi), C(ui + 1, vi), C(ui + 1, vi + 1), C(ui, vi + 1), true);
}

// ---------------------------------------------------------------------------
void Haze::init(int count, float x_, float y_, float w_, float h_, Color t, uint32_t seed) {
    x = x_; y = y_; w = w_; h = h_; tint = t;
    Rng r(seed);
    puffs.clear();
    for (int i = 0; i < count; i++) {
        Puff p;
        p.x = x + r.uniform(0, w);
        p.y = y + r.uniform(0, h);
        p.vx = r.uniform(4, 14) * (r.chance(0.5) ? 1 : -1);
        p.vy = r.uniform(-4, 2);
        p.r = r.uniform(110, 260);
        p.a = r.uniform(0.05f, 0.12f);
        p.spin = r.uniform(-0.05f, 0.05f);
        p.ang = r.uniform(0, 360);
        p.life = r.uniform(14, 30);
        p.age = r.uniform(0, p.life);
        puffs.push_back(p);
    }
}

void Haze::update(float dt) {
    for (auto& p : puffs) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.ang += p.spin * dt * 57.3f;
        p.age += dt;
        if (p.age > p.life) p.age -= p.life;
        if (p.x < x - p.r) p.x += w + 2 * p.r;
        if (p.x > x + w + p.r) p.x -= w + 2 * p.r;
        if (p.y < y - p.r) p.y += h + 2 * p.r;
        if (p.y > y + h + p.r) p.y -= h + 2 * p.r;
    }
}

void Haze::render(float alpha) const {
    if (!g_smoke) return;
    for (const auto& p : puffs) {
        float life = std::sin(p.age / p.life * PI);
        gfx::drawCentered(g_smoke, p.x, p.y, p.r * 2 / g_smoke.w, p.ang, p.a * life * alpha, tint);
    }
}

void Wisp::update(float dt, float x, float y, float rate) {
    timer += dt * rate;
    while (timer > 1) {
        timer -= 1;
        bits.push_back({x, y, (float)(std::rand() % 100 - 50) / 10.f, 5.f, 0.f, 2.6f + (std::rand() % 100) / 100.f});
    }
    for (auto& b : bits) {
        b.age += dt;
        b.y -= dt * 26;
        b.x += b.vx * dt + std::sin(b.age * 2.2f + b.life * 7) * dt * 10;
        b.r += dt * 9;
    }
    bits.erase(std::remove_if(bits.begin(), bits.end(), [](const Bit& b) { return b.age >= b.life; }), bits.end());
}

void Wisp::render(float alpha) const {
    if (!g_smoke) return;
    for (const auto& b : bits) {
        float t = b.age / b.life;
        float a = std::sin(t * PI) * 0.3f * alpha;
        gfx::drawCentered(g_smoke, b.x, b.y, b.r * 2 / g_smoke.w, b.age * 40, a, Color(230, 225, 220));
    }
}

} // namespace fx
