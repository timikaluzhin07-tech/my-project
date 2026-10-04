// Grand Casino NX — shared basics: types, colors, easing, RNG, formatting.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using i64 = int64_t;
using u32 = uint32_t;
using u64 = uint64_t;

constexpr int SCREEN_W = 1280;
constexpr int SCREEN_H = 720;
// Every procedural texture is rasterized at this multiple of its logical size,
// so it stays crisp on a 1080p TV while all layout math happens in 1280x720.
constexpr float TEX_SCALE = 1.5f;
constexpr float PI = 3.14159265358979f;

struct Color {
    uint8_t r = 255, g = 255, b = 255, a = 255;
    constexpr Color() = default;
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
    static constexpr Color hex(uint32_t rgb, uint8_t a = 255) {
        return Color((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, a);
    }
    constexpr Color alpha(float f) const { return Color(r, g, b, (uint8_t)std::clamp(a * f, 0.f, 255.f)); }
    constexpr Color withA(uint8_t na) const { return Color(r, g, b, na); }
    Color scaled(float f) const {
        return Color((uint8_t)std::clamp(r * f, 0.f, 255.f), (uint8_t)std::clamp(g * f, 0.f, 255.f),
                     (uint8_t)std::clamp(b * f, 0.f, 255.f), a);
    }
    std::string css() const {
        char buf[16];
        snprintf(buf, sizeof buf, "#%02x%02x%02x", r, g, b);
        return buf;
    }
};

inline Color lerpColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return Color((uint8_t)(a.r + (b.r - a.r) * t), (uint8_t)(a.g + (b.g - a.g) * t),
                 (uint8_t)(a.b + (b.b - a.b) * t), (uint8_t)(a.a + (b.a - a.a) * t));
}

namespace pal {
constexpr Color white(255, 255, 255);
constexpr Color black(0, 0, 0);
constexpr Color ink = Color::hex(0x0b0a0d);
constexpr Color gold = Color::hex(0xd4af37);
constexpr Color goldLight = Color::hex(0xf6e3a1);
constexpr Color goldDark = Color::hex(0x8a6a1f);
constexpr Color ivory = Color::hex(0xf3ecdc);
constexpr Color cream = Color::hex(0xe9dcc0);
constexpr Color muted = Color::hex(0xa79f8f);
constexpr Color dim = Color::hex(0x6d665c);
constexpr Color red = Color::hex(0xc0283a);
constexpr Color redBright = Color::hex(0xe5484d);
constexpr Color green = Color::hex(0x2fa36b);
constexpr Color greenBright = Color::hex(0x4ad48a);
constexpr Color blue = Color::hex(0x3b82c4);
constexpr Color panel = Color::hex(0x0d0b10, 225);
} // namespace pal

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float clamp01(float t) { return std::clamp(t, 0.f, 1.f); }
// Exponential approach that is frame-rate independent.
inline float approach(float cur, float target, float speed, float dt) {
    return target + (cur - target) * std::exp(-speed * dt);
}

namespace ease {
inline float outCubic(float t) { t = clamp01(t); float u = 1 - t; return 1 - u * u * u; }
inline float inCubic(float t) { t = clamp01(t); return t * t * t; }
inline float inOutCubic(float t) {
    t = clamp01(t);
    return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.f) / 2;
}
inline float outQuad(float t) { t = clamp01(t); return 1 - (1 - t) * (1 - t); }
inline float inQuad(float t) { t = clamp01(t); return t * t; }
inline float outBack(float t, float s = 1.70158f) {
    t = clamp01(t);
    float u = t - 1;
    return 1 + (s + 1) * u * u * u + s * u * u;
}
inline float outBounce(float t) {
    t = clamp01(t);
    const float n = 7.5625f, d = 2.75f;
    if (t < 1 / d) return n * t * t;
    if (t < 2 / d) { t -= 1.5f / d; return n * t * t + 0.75f; }
    if (t < 2.5f / d) { t -= 2.25f / d; return n * t * t + 0.9375f; }
    t -= 2.625f / d;
    return n * t * t + 0.984375f;
}
inline float outElastic(float t) {
    t = clamp01(t);
    if (t == 0 || t == 1) return t;
    return std::pow(2.f, -10 * t) * std::sin((t * 10 - 0.75f) * (2 * PI / 3)) + 1;
}
} // namespace ease

// xoshiro256** — fast, good quality, deterministic when seeded (used by tests).
class Rng {
public:
    explicit Rng(u64 seed = 0x9E3779B97F4A7C15ull) { reseed(seed); }
    void reseed(u64 seed) {
        for (auto& v : s_) {
            seed += 0x9E3779B97F4A7C15ull;
            u64 z = seed;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            v = z ^ (z >> 31);
        }
    }
    u64 next() {
        const u64 result = rotl(s_[1] * 5, 7) * 9;
        const u64 t = s_[1] << 17;
        s_[2] ^= s_[0]; s_[3] ^= s_[1]; s_[1] ^= s_[2]; s_[0] ^= s_[3];
        s_[2] ^= t; s_[3] = rotl(s_[3], 45);
        return result;
    }
    // Uniform integer in [lo, hi] inclusive.
    int range(int lo, int hi) {
        if (hi <= lo) return lo;
        u64 span = (u64)(hi - lo) + 1;
        return lo + (int)(next() % span);
    }
    double uniform() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    float uniform(float a, float b) { return a + (b - a) * (float)uniform(); }
    bool chance(double p) { return uniform() < p; }

private:
    static u64 rotl(u64 x, int k) { return (x << k) | (x >> (64 - k)); }
    u64 s_[4];
};

Rng& rng(); // global gameplay RNG (seeded from time at startup)

std::string strf(const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

// "12 500" style thousands grouping with a thin no-break space.
std::string fmtMoney(i64 v);
// Compact chip label: 500, 1K, 25K, 1M.
std::string fmtChip(i64 v);
