#include "audio.h"

#include <SDL.h>

#include <atomic>
#include <thread>

namespace audio {

namespace {

constexpr int SR = 48000;
using Buf = std::vector<float>;

// ---------------------------------------------------------------------------
// DSP helpers
// ---------------------------------------------------------------------------
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    static Biquad make(int type, float f, float q) {
        Biquad b;
        f = std::clamp(f, 10.f, SR * 0.45f);
        float w = 2 * PI * f / SR, cs = std::cos(w), sn = std::sin(w), al = sn / (2 * q);
        float a0 = 1 + al;
        if (type == 0) { // low-pass
            b.b0 = (1 - cs) / 2; b.b1 = 1 - cs; b.b2 = (1 - cs) / 2;
        } else if (type == 1) { // high-pass
            b.b0 = (1 + cs) / 2; b.b1 = -(1 + cs); b.b2 = (1 + cs) / 2;
        } else { // band-pass (0 dB peak)
            b.b0 = al; b.b1 = 0; b.b2 = -al;
        }
        b.a1 = -2 * cs; b.a2 = 1 - al;
        b.b0 /= a0; b.b1 /= a0; b.b2 /= a0; b.a1 /= a0; b.a2 /= a0;
        return b;
    }
    static Biquad lp(float f, float q = 0.707f) { return make(0, f, q); }
    static Biquad hp(float f, float q = 0.707f) { return make(1, f, q); }
    static Biquad bp(float f, float q = 1.f) { return make(2, f, q); }
    float operator()(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

struct Noise {
    uint32_t s = 22222;
    float white() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (s & 0xffffff) / 8388607.5f - 1.f;
    }
    float b0 = 0, b1 = 0, b2 = 0;
    float pink() {
        float w = white();
        b0 = 0.99765f * b0 + w * 0.0990460f;
        b1 = 0.96300f * b1 + w * 0.2965164f;
        b2 = 0.57000f * b2 + w * 1.0526913f;
        return (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
    }
    float br = 0;
    float brown() {
        br = (br + white() * 0.02f) * 0.998f;
        return br * 3.5f;
    }
};

inline float envAD(float t, float a, float d) {
    if (t < 0) return 0;
    if (t < a) return t / a;
    return std::exp(-(t - a) / d);
}
inline float midiHz(float m) { return 440.f * std::pow(2.f, (m - 69) / 12.f); }
inline float polyblep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
}

Buf make(float sec) { return Buf((size_t)(sec * SR), 0.f); }

void normalize(Buf& b, float peak) {
    float m = 0;
    for (float v : b) m = std::max(m, std::fabs(v));
    if (m > 0) for (float& v : b) v *= peak / m;
}

// Sum of decaying sine partials — bells, chips, coins, glass.
void addModes(Buf& b, size_t at, const std::vector<std::pair<float, float>>& modes, float amp, float decayScale = 1) {
    for (auto& [f, d] : modes) {
        float ph = 0, inc = 2 * PI * f / SR;
        float dec = d * decayScale;
        for (size_t i = at; i < b.size(); i++) {
            float t = (i - at) / (float)SR;
            float e = std::exp(-t / dec);
            if (e < 0.0005f) break;
            b[i] += std::sin(ph) * e * amp;
            ph += inc;
        }
    }
}

void addClick(Buf& b, size_t at, float freq, float q, float len, float amp, uint32_t seed) {
    Noise n; n.s = seed | 1;
    Biquad f = Biquad::bp(freq, q);
    for (size_t i = at; i < b.size() && i < at + (size_t)(len * SR * 4); i++) {
        float t = (i - at) / (float)SR;
        b[i] += f(n.white()) * std::exp(-t / len) * amp;
    }
}

void chipClick(Buf& b, size_t at, float pitch, float amp, uint32_t seed) {
    Rng r(seed);
    std::vector<std::pair<float, float>> modes;
    float base[] = {2950, 4380, 6120, 7710, 9300};
    for (float f : base) modes.push_back({f * pitch * r.uniform(0.96f, 1.04f), r.uniform(0.012f, 0.03f)});
    addModes(b, at, modes, amp * 0.22f);
    addClick(b, at, 5200 * pitch, 1.2f, 0.004f, amp * 0.9f, seed);
}

// Small Schroeder reverb applied offline to a stereo buffer.
void reverb(Buf& L, Buf& R, float mix, float size) {
    const int combL[4] = {1557, 1617, 1491, 1422}, combR[4] = {1580, 1640, 1514, 1445};
    const int ap[2] = {225, 556};
    auto process = [&](Buf& ch, const int* combs) {
        std::vector<Buf> cb(4);
        std::vector<size_t> ci(4, 0);
        std::vector<float> filt(4, 0.f);
        for (int k = 0; k < 4; k++) cb[k].assign((size_t)(combs[k] * size * 1.4f), 0.f);
        Buf ab[2];
        size_t ai[2] = {0, 0};
        for (int k = 0; k < 2; k++) ab[k].assign(ap[k], 0.f);
        for (size_t i = 0; i < ch.size(); i++) {
            float in = ch[i] * 0.2f, out = 0;
            for (int k = 0; k < 4; k++) {
                float y = cb[k][ci[k]];
                filt[k] = y * 0.75f + filt[k] * 0.25f; // damping
                cb[k][ci[k]] = in + filt[k] * 0.82f;
                ci[k] = (ci[k] + 1) % cb[k].size();
                out += y;
            }
            for (int k = 0; k < 2; k++) {
                float bufout = ab[k][ai[k]];
                float y = -out + bufout;
                ab[k][ai[k]] = out + bufout * 0.5f;
                ai[k] = (ai[k] + 1) % ab[k].size();
                out = y;
            }
            ch[i] = ch[i] * (1 - mix * 0.3f) + out * mix;
        }
    };
    process(L, combL);
    process(R, combR);
}

// Crossfades the tail into the head so a buffer loops without a click.
void makeLoopable(Buf& b, float fadeSec) {
    size_t f = (size_t)(fadeSec * SR);
    if (b.size() < f * 2) return;
    size_t n = b.size() - f;
    for (size_t i = 0; i < f; i++) {
        float t = i / (float)f;
        b[i] = b[i] * t + b[n + i] * (1 - t);
    }
    b.resize(n);
}

// ---------------------------------------------------------------------------
// Sound effect synthesis
// ---------------------------------------------------------------------------
Buf sfxNav() {
    Buf b = make(0.06f);
    addModes(b, 0, {{2100, 0.012f}, {3400, 0.006f}}, 0.5f);
    addClick(b, 0, 3000, 2, 0.002f, 0.4f, 7);
    normalize(b, 0.32f);
    return b;
}

Buf sfxSelect() {
    Buf b = make(0.35f);
    addModes(b, 0, {{880, 0.12f}, {1760, 0.06f}, {2640, 0.03f}}, 0.4f);
    addModes(b, (size_t)(0.055f * SR), {{1318.5f, 0.15f}, {2637, 0.06f}}, 0.35f);
    addClick(b, 0, 2500, 1.5f, 0.003f, 0.5f, 11);
    normalize(b, 0.42f);
    return b;
}

Buf sfxBack() {
    Buf b = make(0.25f);
    addModes(b, 0, {{740, 0.07f}, {1480, 0.03f}}, 0.4f);
    addModes(b, (size_t)(0.05f * SR), {{554, 0.09f}, {1108, 0.03f}}, 0.4f);
    normalize(b, 0.34f);
    return b;
}

Buf sfxError() {
    Buf b = make(0.22f);
    Biquad lp = Biquad::lp(900);
    float ph = 0;
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        ph += 140.f / SR;
        float sq = std::fmod(ph, 1.f) < 0.5f ? 1.f : -1.f;
        b[i] = lp(sq) * envAD(t, 0.005f, 0.07f) * (t > 0.09f && t < 0.11f ? 0.2f : 1.f);
    }
    normalize(b, 0.3f);
    return b;
}

Buf sfxCardSlide() {
    Buf b = make(0.22f);
    Noise n;
    Biquad bp = Biquad::bp(3200, 0.7f), hp = Biquad::hp(900);
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        float e = t < 0.025f ? t / 0.025f : std::exp(-(t - 0.025f) / 0.05f);
        b[i] = hp(bp(n.white())) * e;
    }
    // soft landing tap
    size_t at = (size_t)(0.09f * SR);
    for (size_t i = at; i < b.size(); i++) {
        float t = (i - at) / (float)SR;
        b[i] += std::sin(2 * PI * 170 * t) * std::exp(-t / 0.012f) * 0.25f;
    }
    normalize(b, 0.55f);
    return b;
}

Buf sfxCardFlip() {
    Buf b = make(0.08f);
    Noise n;
    Biquad hp = Biquad::hp(2500);
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        b[i] = hp(n.white()) * std::exp(-t / 0.008f);
    }
    addModes(b, 0, {{1250, 0.01f}, {2300, 0.006f}}, 0.25f);
    normalize(b, 0.5f);
    return b;
}

Buf sfxChip() {
    Buf b = make(0.12f);
    chipClick(b, 0, 1.0f, 1.f, 3);
    chipClick(b, (size_t)(0.038f * SR), 1.06f, 0.55f, 5);
    normalize(b, 0.55f);
    return b;
}

Buf sfxChips() {
    Buf b = make(0.55f);
    Rng r(99);
    for (int k = 0; k < 11; k++) {
        float t = 0.012f + r.uniform(0.f, 0.38f) * (k / 11.f + 0.2f);
        chipClick(b, (size_t)(t * SR), r.uniform(0.9f, 1.12f), r.uniform(0.35f, 1.f), 100 + k);
    }
    normalize(b, 0.6f);
    return b;
}

Buf sfxShuffle() {
    Buf b = make(1.1f);
    Noise n;
    Biquad bp = Biquad::bp(4200, 0.6f);
    Rng r(5);
    // riffle: dense tiny flaps
    for (int k = 0; k < 70; k++) {
        float t = 0.05f + k * 0.0105f + r.uniform(-0.002f, 0.002f);
        addClick(b, (size_t)(t * SR), r.uniform(2500, 4500), 1.5f, 0.0025f, r.uniform(0.25f, 0.5f), 300 + k);
    }
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        float e = (t > 0.04f && t < 0.8f) ? 0.12f : 0.f;
        // closing "bridge" whoosh
        if (t > 0.82f) e = 0.35f * envAD(t - 0.82f, 0.03f, 0.08f);
        b[i] += bp(n.white()) * e;
    }
    normalize(b, 0.5f);
    return b;
}

// Bell-ish FM tone used for musical stingers.
void addBell(Buf& b, size_t at, float hz, float dur, float amp, float bright = 1.f) {
    for (size_t i = at; i < b.size(); i++) {
        float t = (i - at) / (float)SR;
        if (t > dur * 4) break;
        float idx = 2.2f * bright * std::exp(-t / (dur * 0.4f));
        float m = std::sin(2 * PI * hz * 3.5f * t) * idx;
        float e = envAD(t, 0.002f, dur);
        b[i] += std::sin(2 * PI * hz * t + m) * e * amp;
    }
}

Buf sfxWin() {
    Buf b = make(1.5f);
    float notes[] = {72, 76, 79, 84};
    for (int i = 0; i < 4; i++) addBell(b, (size_t)(i * 0.075f * SR), midiHz(notes[i]), 0.35f, 0.3f);
    addBell(b, (size_t)(0.3f * SR), midiHz(88), 0.6f, 0.22f, 0.6f);
    normalize(b, 0.5f);
    return b;
}

Buf sfxBigWin() {
    Buf b = make(3.2f);
    // brassy swell chord
    float chord[] = {48, 55, 60, 64, 67, 72};
    for (float m : chord) {
        float hz = midiHz(m);
        float ph = 0, ph2 = 0;
        Biquad lp = Biquad::lp(400);
        for (size_t i = 0; i < b.size(); i++) {
            float t = i / (float)SR;
            if (i % 64 == 0) lp = Biquad::lp(500 + 3000 * envAD(t, 0.25f, 1.0f), 0.8f);
            ph += hz / SR; ph2 += hz * 1.004f / SR;
            float s = (std::fmod(ph, 1.f) * 2 - 1) + (std::fmod(ph2, 1.f) * 2 - 1);
            b[i] += lp(s) * envAD(t, 0.18f, 1.1f) * 0.12f;
        }
    }
    float arp[] = {72, 76, 79, 84, 88, 91, 96};
    for (int i = 0; i < 7; i++) addBell(b, (size_t)((0.25f + i * 0.07f) * SR), midiHz(arp[i]), 0.5f, 0.18f);
    normalize(b, 0.6f);
    return b;
}

Buf sfxLose() {
    Buf b = make(0.6f);
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        float f = t < 0.14f ? 392.f : 311.1f;
        float tt = t < 0.14f ? t : t - 0.14f;
        b[i] = std::sin(2 * PI * f * tt) * envAD(tt, 0.01f, t < 0.14f ? 0.08f : 0.18f);
    }
    normalize(b, 0.22f);
    return b;
}

Buf sfxBallLoop() {
    Buf b = make(2.15f);
    Noise n;
    Biquad bp = Biquad::bp(1500, 0.9f), bp2 = Biquad::bp(4200, 2.f), lp = Biquad::lp(7000);
    Rng r(42);
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        float wob = 0.7f + 0.3f * std::sin(2 * PI * 6.f * t) * std::sin(2 * PI * 0.9f * t);
        b[i] = lp(bp(n.white()) * 0.8f + bp2(n.white()) * 0.25f) * wob;
    }
    // micro rattles of the ball on the track
    for (int k = 0; k < 40; k++)
        addClick(b, (size_t)(r.uniform(0.f, 2.0f) * SR), r.uniform(2500, 5000), 3, 0.0015f, r.uniform(0.1f, 0.3f), 900 + k);
    makeLoopable(b, 0.12f);
    normalize(b, 0.5f);
    return b;
}

Buf sfxBallClack() {
    Buf b = make(0.12f);
    addClick(b, 0, 3600, 2.5f, 0.004f, 1.f, 77);
    addModes(b, 0, {{2250, 0.02f}, {3900, 0.012f}, {5600, 0.008f}}, 0.3f);
    normalize(b, 0.55f);
    return b;
}

Buf sfxWhoosh() {
    Buf b = make(0.5f);
    Noise n;
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        static Biquad bp;
        if (i % 32 == 0) bp = Biquad::bp(500 + 2500 * std::sin(PI * t / 0.5f), 1.2f);
        b[i] = bp(n.white()) * std::sin(PI * t / 0.5f);
    }
    normalize(b, 0.35f);
    return b;
}

Buf sfxReelDrop() {
    Buf b = make(0.45f);
    Noise n;
    Biquad bp;
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        if (i % 32 == 0) bp = Biquad::bp(3500 - 2800 * (t / 0.45f), 1.f);
        b[i] = bp(n.white()) * envAD(t, 0.03f, 0.12f) * 0.8f;
    }
    normalize(b, 0.32f);
    return b;
}

Buf sfxLand() {
    Buf b = make(0.18f);
    float ph = 0;
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        float f = 150 * std::exp(-t / 0.05f) + 55;
        ph += 2 * PI * f / SR;
        b[i] = std::sin(ph) * envAD(t, 0.002f, 0.045f);
    }
    addClick(b, 0, 2000, 1.5f, 0.003f, 0.25f, 31);
    normalize(b, 0.5f);
    return b;
}

Buf sfxShatter() {
    Buf b = make(0.9f);
    Noise n;
    Biquad hp = Biquad::hp(3000);
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        b[i] = hp(n.white()) * envAD(t, 0.001f, 0.06f) * 0.6f;
    }
    Rng r(8);
    for (int k = 0; k < 24; k++) {
        size_t at = (size_t)(r.uniform(0.f, 0.25f) * SR);
        addModes(b, at, {{r.uniform(3000, 9000), r.uniform(0.03f, 0.15f)}}, r.uniform(0.05f, 0.15f));
    }
    // airy sparkle
    float notes[] = {84, 88, 91, 96};
    for (int i = 0; i < 4; i++) addBell(b, (size_t)((0.02f + i * 0.04f) * SR), midiHz(notes[i]), 0.2f, 0.08f, 0.5f);
    normalize(b, 0.5f);
    return b;
}

Buf sfxZap() {
    Buf b = make(0.7f);
    Noise n;
    Biquad bp = Biquad::bp(2500, 0.8f);
    float ph = 0;
    Rng r(4);
    float crackle = 1;
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        if (i % 240 == 0) crackle = r.chance(0.5) ? r.uniform(0.3f, 1.f) : 0.1f;
        float f = 900 * std::exp(-t / 0.15f) + 60;
        ph += f / SR;
        float saw = std::fmod(ph, 1.f) * 2 - 1;
        b[i] = (bp(n.white()) * crackle * 1.2f + saw * 0.35f) * envAD(t, 0.003f, 0.16f);
    }
    normalize(b, 0.5f);
    return b;
}

Buf sfxThunder() {
    Buf b = make(3.4f);
    Noise n;
    Biquad lp = Biquad::lp(180, 0.6f), lp2 = Biquad::lp(900), hp = Biquad::hp(1200);
    Rng r(12);
    float rollPh = 0;
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        rollPh += (3.5f + std::sin(t * 1.3f)) / SR;
        float roll = 0.6f + 0.4f * std::sin(2 * PI * rollPh) * std::sin(2 * PI * rollPh * 0.37f);
        float rumble = lp(n.brown()) * 2.2f * envAD(t, 0.08f, 1.1f) * roll;
        float crack = hp(n.white()) * envAD(t, 0.001f, 0.05f) * 0.9f;
        float mid = lp2(n.white()) * envAD(t - 0.03f, 0.02f, 0.35f) * 0.5f;
        b[i] = rumble + crack + mid;
    }
    normalize(b, 0.75f);
    return b;
}

Buf sfxCoin() {
    Buf b = make(0.4f);
    addModes(b, 0, {{3250, 0.12f}, {5150, 0.09f}, {7600, 0.06f}, {9800, 0.04f}}, 0.25f);
    addClick(b, 0, 6000, 2, 0.002f, 0.4f, 55);
    normalize(b, 0.4f);
    return b;
}

Buf sfxTick() {
    Buf b = make(0.03f);
    addModes(b, 0, {{4200, 0.004f}, {6300, 0.003f}}, 0.4f);
    normalize(b, 0.22f);
    return b;
}

Buf sfxOrb() {
    Buf b = make(1.2f);
    float notes[] = {79, 83, 86, 91, 95};
    for (int i = 0; i < 5; i++) addBell(b, (size_t)(i * 0.045f * SR), midiHz(notes[i]), 0.25f, 0.18f, 0.8f);
    Noise n;
    Biquad hp = Biquad::hp(6000);
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        b[i] += hp(n.white()) * envAD(t, 0.05f, 0.25f) * 0.12f;
    }
    normalize(b, 0.45f);
    return b;
}

Buf sfxBoom() {
    Buf b = make(2.2f);
    Noise n;
    Biquad lp = Biquad::lp(300);
    float ph = 0;
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        float f = 70 * std::exp(-t / 0.25f) + 32;
        ph += 2 * PI * f / SR;
        b[i] = std::sin(ph) * envAD(t, 0.003f, 0.6f) * 1.2f + lp(n.white()) * envAD(t, 0.001f, 0.25f) * 0.8f;
    }
    normalize(b, 0.8f);
    return b;
}

Buf sfxKnock() {
    // dealer taps the felt / table knock
    Buf b = make(0.15f);
    float ph = 0;
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        ph += 2 * PI * (220 * std::exp(-t / 0.02f) + 90) / SR;
        b[i] = std::sin(ph) * envAD(t, 0.001f, 0.025f);
    }
    addClick(b, 0, 1200, 1, 0.004f, 0.3f, 66);
    normalize(b, 0.45f);
    return b;
}

Buf sfxAmbience() {
    // distant crowd murmur with the odd chip clack and machine chime
    Buf b = make(12.5f);
    Noise n;
    Biquad bp1 = Biquad::bp(450, 0.6f), bp2 = Biquad::bp(1100, 0.8f), lp = Biquad::lp(2500);
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        float m1 = 0.6f + 0.4f * std::sin(2 * PI * 0.31f * t + std::sin(t * 1.7f));
        float m2 = 0.6f + 0.4f * std::sin(2 * PI * 0.47f * t + 1.3f + std::sin(t * 2.3f));
        b[i] = lp(bp1(n.pink()) * m1 * 1.4f + bp2(n.pink()) * m2 * 0.9f);
    }
    Rng r(2024);
    for (int k = 0; k < 9; k++) chipClick(b, (size_t)(r.uniform(0.5f, 11.f) * SR), r.uniform(0.9f, 1.1f), 0.05f, 700 + k);
    for (int k = 0; k < 3; k++) {
        size_t at = (size_t)(r.uniform(1.f, 10.f) * SR);
        float base = (float)r.range(76, 84);
        for (int j = 0; j < 3; j++) addBell(b, at + (size_t)(j * 0.09f * SR), midiHz(base + j * 4), 0.3f, 0.012f, 0.6f);
    }
    makeLoopable(b, 0.5f);
    normalize(b, 0.35f);
    return b;
}

// ---------------------------------------------------------------------------
// Crowd, drums and stingers
// ---------------------------------------------------------------------------
void addClap(Buf& b, size_t at, float amp, Rng& r) {
    Noise n;
    n.s = (uint32_t)r.next() | 1;
    Biquad f = Biquad::bp(r.uniform(900, 2400), r.uniform(1.2f, 2.2f));
    // a clap is two or three quick reflections of the same burst
    float d = r.uniform(0.006f, 0.012f);
    size_t len = (size_t)(0.09f * SR);
    for (size_t i = 0; i < len && at + i < b.size(); i++) {
        float t = i / (float)SR;
        float e = std::exp(-t / d) + 0.5f * envAD(t - 0.009f, 0.0005f, d) + 0.25f * envAD(t - 0.017f, 0.0005f, d * 1.5f);
        b[at + i] += f(n.white()) * e * amp;
    }
}

// A crowd voice: buzzy source through two vowel formants, gliding in pitch.
void addVoice(Buf& b, size_t at, float dur, float f0, float f1, float fa, float fb, float amp, Rng& r) {
    Biquad A = Biquad::bp(fa, 4.f), B = Biquad::bp(fb, 5.f), lp = Biquad::lp(3200);
    Noise n;
    n.s = (uint32_t)r.next() | 1;
    float ph = 0, vib = r.uniform(4.5f, 6.5f), vd = r.uniform(0.01f, 0.025f);
    size_t len = (size_t)(dur * SR);
    for (size_t i = 0; i < len && at + i < b.size(); i++) {
        float t = i / (float)SR, u = t / dur;
        float f = lerp(f0, f1, std::sqrt(u)) * (1 + vd * std::sin(2 * PI * vib * t));
        ph += f / SR;
        float saw = std::fmod(ph, 1.f) * 2 - 1 + n.white() * 0.25f;
        float e = clamp01(t / 0.12f) * clamp01((dur - t) / (dur * 0.45f));
        b[at + i] += lp(A(saw) * 1.2f + B(saw) * 0.7f) * e * amp;
    }
}

Buf sfxApplause() {
    Buf b = make(3.2f);
    Rng r(808);
    for (int c = 0; c < 22; c++) {
        float t = r.uniform(0, 0.25f), rate = r.uniform(4.5f, 7.f), stop = r.uniform(1.6f, 2.9f);
        float amp = r.uniform(0.4f, 1.f);
        while (t < stop) {
            addClap(b, (size_t)(t * SR), amp * clamp01((stop - t) / 0.8f), r);
            t += 1 / rate * r.uniform(0.85f, 1.15f);
        }
    }
    // a couple of whistles from the back
    for (int w = 0; w < 2; w++) {
        float start = r.uniform(0.3f, 1.2f), dur = r.uniform(0.5f, 0.8f);
        float ph = 0;
        for (size_t i = (size_t)(start * SR); i < (size_t)((start + dur) * SR) && i < b.size(); i++) {
            float t = i / (float)SR - start, u = t / dur;
            float f = (u < 0.3f ? lerp(1700, 2700, u / 0.3f) : lerp(2700, 2300, (u - 0.3f) / 0.7f)) * (1 + 0.012f * std::sin(t * 40));
            ph += 2 * PI * f / SR;
            b[i] += std::sin(ph) * clamp01(t / 0.04f) * clamp01((dur - t) / 0.15f) * 0.12f;
        }
    }
    Noise n;
    Biquad lp = Biquad::lp(1200);
    for (size_t i = 0; i < b.size(); i++) b[i] += lp(n.pink()) * envAD(i / (float)SR, 0.2f, 1.2f) * 0.15f;
    normalize(b, 0.5f);
    return b;
}

Buf sfxCheer() {
    Buf b = sfxApplause();
    b.resize((size_t)(3.4f * SR), 0.f);
    Rng r(4242);
    for (int v = 0; v < 12; v++) {
        float f0 = r.uniform(170, 330), up = r.uniform(1.25f, 1.6f);
        bool ah = r.chance(0.5);
        addVoice(b, (size_t)(r.uniform(0, 0.35f) * SR), r.uniform(1.1f, 1.8f), f0, f0 * up, ah ? 800 : 450, ah ? 1250 : 850,
                 r.uniform(0.08f, 0.16f), r);
    }
    normalize(b, 0.6f);
    return b;
}

Buf sfxGroan() {
    Buf b = make(1.9f);
    Rng r(1313);
    for (int v = 0; v < 10; v++) {
        float f0 = r.uniform(190, 300);
        addVoice(b, (size_t)(r.uniform(0, 0.2f) * SR), r.uniform(1.2f, 1.6f), f0, f0 * r.uniform(0.62f, 0.75f), 500, 880,
                 r.uniform(0.1f, 0.18f), r);
    }
    normalize(b, 0.38f);
    return b;
}

Buf sfxKaching() {
    Buf b = make(1.3f);
    // drawer: a rattle sliding out, then the bell
    Noise n;
    Biquad bp = Biquad::bp(2600, 1.5f);
    for (size_t i = 0; i < (size_t)(0.12f * SR); i++) {
        float t = i / (float)SR;
        b[i] += bp(n.white()) * envAD(t, 0.005f, 0.04f) * (0.6f + 0.4f * std::sin(t * 900)) * 0.6f;
    }
    addClick(b, (size_t)(0.11f * SR), 1800, 1.5f, 0.004f, 0.8f, 91);
    size_t at = (size_t)(0.13f * SR);
    addModes(b, at, {{2637, 0.55f}, {3951, 0.4f}, {5274, 0.3f}, {6650, 0.2f}, {7920, 0.12f}}, 0.22f);
    addModes(b, at + (size_t)(0.07f * SR), {{3136, 0.5f}, {4700, 0.32f}, {6270, 0.22f}}, 0.16f);
    normalize(b, 0.5f);
    return b;
}

Buf sfxHeartbeat() {
    Buf b = make(0.95f);
    Noise n;
    auto thump = [&](float at, float amp) {
        float ph = 0;
        Biquad lp = Biquad::lp(160);
        for (size_t i = (size_t)(at * SR); i < b.size(); i++) {
            float t = i / (float)SR - at;
            if (t > 0.3f) break;
            float f = 62 * std::exp(-t / 0.06f) + 38;
            ph += 2 * PI * f / SR;
            b[i] += (std::sin(ph) + lp(n.white()) * 0.6f) * envAD(t, 0.004f, 0.06f) * amp;
        }
    };
    thump(0.0f, 1.f);
    thump(0.26f, 0.7f);
    normalize(b, 0.7f);
    return b;
}

void snare(Buf& b, size_t at, float amp, Noise& n) {
    Biquad hp = Biquad::hp(1800), bp = Biquad::bp(3800, 0.8f);
    float ph = 0;
    for (size_t i = at; i < b.size(); i++) {
        float t = (i - at) / (float)SR;
        if (t > 0.12f) break;
        ph += 2 * PI * (190 + 40 * std::exp(-t / 0.01f)) / SR;
        b[i] += (bp(hp(n.white())) * envAD(t, 0.0008f, 0.03f) + std::sin(ph) * envAD(t, 0.0008f, 0.018f) * 0.5f) * amp;
    }
}

Buf sfxDrumroll() {
    const float len = 1.0f;
    Buf b = make(len + 0.15f);
    Noise n;
    Rng r(66);
    const int strokes = 28;
    for (int k = 0; k < strokes; k++)
        snare(b, (size_t)(k * SR * len / strokes) + (size_t)r.uniform(0, 40), (k % 2 ? 0.75f : 1.f) * r.uniform(0.85f, 1.f), n);
    // the ringing of the last strokes wraps around to the head so the loop is seamless
    size_t L = (size_t)(len * SR);
    for (size_t i = L; i < b.size(); i++) b[i - L] += b[i];
    b.resize(L);
    normalize(b, 0.42f);
    return b;
}

Buf sfxCymbal() {
    Buf b = make(2.6f);
    Noise n;
    Biquad hp = Biquad::hp(5000), bp = Biquad::bp(7000, 0.6f);
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        b[i] = (hp(n.white()) * 0.8f + bp(n.white()) * 0.5f) * envAD(t, 0.002f, 0.7f);
    }
    Rng r(5);
    std::vector<std::pair<float, float>> modes;
    for (int k = 0; k < 14; k++) modes.push_back({r.uniform(3000, 11000), r.uniform(0.3f, 1.2f)});
    addModes(b, 0, modes, 0.04f);
    Noise n2;
    snare(b, 0, 0.8f, n2);
    normalize(b, 0.45f);
    return b;
}

Buf sfxGlass() {
    Buf b = make(1.6f);
    addClick(b, 0, 6500, 2, 0.002f, 0.5f, 17);
    addModes(b, 0, {{2350, 0.9f}, {2365, 0.85f}, {3820, 0.55f}, {5150, 0.4f}, {6650, 0.25f}, {8300, 0.15f}}, 0.18f);
    addModes(b, (size_t)(0.11f * SR), {{2480, 0.7f}, {4010, 0.45f}, {5400, 0.3f}}, 0.1f);
    normalize(b, 0.4f);
    return b;
}

Buf sfxRain() {
    Buf b = make(6.5f);
    Noise n;
    Biquad lp = Biquad::lp(3500), hp = Biquad::hp(400), lp2 = Biquad::lp(900);
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        float m = 0.75f + 0.25f * std::sin(t * 0.9f + std::sin(t * 0.37f) * 2);
        b[i] = hp(lp(n.white())) * 0.35f * m + lp2(n.pink()) * 0.4f;
    }
    Rng r(99);
    for (int k = 0; k < 900; k++) addClick(b, (size_t)(r.uniform(0, 6.4f) * SR), r.uniform(2500, 8000), 3, 0.0012f, r.uniform(0.05f, 0.25f), 3000 + k);
    makeLoopable(b, 0.4f);
    normalize(b, 0.4f);
    return b;
}

Buf sfxHit() {
    Buf b = make(2.4f);
    // brass stab
    float chord[] = {41, 48, 53, 57, 60, 65, 69};
    for (float m : chord) {
        float hz = midiHz(m), ph = 0, ph2 = 0;
        Biquad lp = Biquad::lp(3000);
        for (size_t i = 0; i < b.size(); i++) {
            float t = i / (float)SR;
            if (i % 64 == 0) lp = Biquad::lp(600 + 4000 * envAD(t, 0.01f, 0.25f), 0.9f);
            ph += hz / SR;
            ph2 += hz * 1.006f / SR;
            float s = (std::fmod(ph, 1.f) * 2 - 1) + (std::fmod(ph2, 1.f) * 2 - 1);
            b[i] += lp(s) * envAD(t, 0.008f, 0.45f) * 0.1f;
        }
    }
    // timpani
    float ph = 0;
    Noise n;
    Biquad lp = Biquad::lp(400);
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR;
        ph += 2 * PI * (88 + 20 * std::exp(-t / 0.05f)) / SR;
        b[i] += (std::sin(ph) * 0.9f + lp(n.white()) * 0.5f) * envAD(t, 0.002f, 0.55f);
    }
    Buf cym = sfxCymbal();
    for (size_t i = 0; i < b.size() && i < cym.size(); i++) b[i] += cym[i] * 0.5f;
    normalize(b, 0.75f);
    return b;
}

Buf sfxRiser() {
    Buf b = make(1.7f);
    Noise n;
    Biquad bp;
    float ph = 0;
    for (size_t i = 0; i < b.size(); i++) {
        float t = i / (float)SR, u = t / 1.7f;
        if (i % 32 == 0) bp = Biquad::bp(300 * std::pow(20.f, u), 2.f);
        ph += 2 * PI * (220 * std::pow(4.f, u)) / SR;
        float e = u * u * clamp01((1.7f - t) / 0.05f);
        b[i] = (bp(n.white()) * 1.4f + std::sin(ph) * 0.25f) * e;
    }
    normalize(b, 0.45f);
    return b;
}

Buf sfxAchieve() {
    Buf b = make(1.8f);
    float notes[] = {76, 79, 83, 88};
    for (int i = 0; i < 4; i++) addBell(b, (size_t)(i * 0.09f * SR), midiHz(notes[i]), 0.5f, 0.25f, 0.9f);
    addBell(b, (size_t)(0.38f * SR), midiHz(91), 0.9f, 0.2f, 0.5f);
    addBell(b, (size_t)(0.38f * SR), midiHz(95), 0.9f, 0.14f, 0.5f);
    Rng r(3);
    for (int k = 0; k < 14; k++) addModes(b, (size_t)(r.uniform(0.35f, 1.1f) * SR), {{r.uniform(5000, 9000), 0.08f}}, 0.04f);
    normalize(b, 0.5f);
    return b;
}

Buf sfxSparkle() {
    Buf b = make(1.1f);
    Rng r(21);
    for (int k = 0; k < 16; k++) {
        float t = k * 0.045f + r.uniform(0, 0.02f);
        addBell(b, (size_t)(t * SR), midiHz((float)r.range(84, 100)), 0.15f, 0.12f * (1 - k / 20.f), 0.4f);
    }
    normalize(b, 0.35f);
    return b;
}

// ---------------------------------------------------------------------------
// Music
// ---------------------------------------------------------------------------
struct Track {
    std::vector<int16_t> pcm; // interleaved stereo
    std::atomic<bool> ready{false};
};

struct Stereo {
    Buf L, R;
    explicit Stereo(float sec) : L(make(sec)), R(make(sec)) {}
    // cheap pan law (music is rendered offline, millions of calls)
    void add(size_t i, float v, float pan) {
        if (i >= L.size()) return;
        float r = 0.5f + 0.5f * pan;
        L[i] += v * (1.2f - r) * 0.83f;
        R[i] += v * (0.2f + r) * 0.83f;
    }
};

// Sine via a rotating phasor: one multiply-add per sample instead of sin().
struct Phasor {
    float c, s, x = 1, y = 0;
    explicit Phasor(float hz) : c(std::cos(2 * PI * hz / SR)), s(std::sin(2 * PI * hz / SR)) {}
    float next() {
        float nx = x * c - y * s, ny = x * s + y * c;
        x = nx; y = ny;
        return y;
    }
};

void rhodes(Stereo& st, float start, float dur, float midi, float vel, float pan) {
    float hz = midiHz(midi);
    size_t a = (size_t)(start * SR), n = (size_t)((dur + 1.2f) * SR);
    Phasor mod(hz), tine(hz * 14.f), trem(4.2f);
    float env = 0, idx = 1.4f, tineEnv = 0.08f;
    const float atk = 1.f / (0.004f * SR);
    const float decay = std::exp(-1.f / (1.4f * SR)), idxDecay = std::exp(-1.f / (0.35f * SR));
    const float tineDecay = std::exp(-1.f / (0.02f * SR)), rel = std::exp(-1.f / (0.25f * SR));
    float ph = 0, inc = 2 * PI * hz / SR;
    float sustain = 1;
    size_t durS = (size_t)(dur * SR);
    for (size_t k = 0; k < n; k++) {
        if (k * atk < 1) env = k * atk;
        else env *= decay;
        if (k > durS) sustain *= rel;
        float e = env * sustain;
        if (k > durS && e < 0.0005f) break;
        float m = mod.next() * (idx + 0.25f);
        idx *= idxDecay;
        float tn = tineEnv > 1e-4f ? tine.next() * tineEnv : 0.f;
        tineEnv *= tineDecay;
        float tr = trem.next();
        ph += inc;
        if (ph > 2 * PI) ph -= 2 * PI;
        float v = (std::sin(ph + m) + tn) * e * vel * (1 + 0.12f * tr);
        st.add(a + k, v, pan + 0.12f * tr);
    }
}

void bass(Stereo& s, float start, float dur, float midi, float vel) {
    float hz = midiHz(midi);
    size_t a = (size_t)(start * SR), n = (size_t)((dur + 0.2f) * SR);
    Biquad lp = Biquad::lp(600, 0.9f);
    for (size_t k = 0; k < n; k++) {
        float t = k / (float)SR;
        float rel = t > dur ? std::exp(-(t - dur) / 0.05f) : 1.f;
        float e = envAD(t, 0.006f, 0.55f) * rel;
        float v = std::sin(2 * PI * hz * t) + 0.35f * std::sin(4 * PI * hz * t) + 0.12f * std::sin(6 * PI * hz * t);
        v += (k < 300 ? 0.3f * std::sin(2 * PI * hz * 8 * t) * (1 - k / 300.f) : 0); // finger pluck
        s.add(a + k, lp(v) * e * vel, -0.05f);
    }
}

void ride(Stereo& s, float start, float vel, Noise& nz) {
    static const float fr[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f};
    size_t a = (size_t)(start * SR), n = (size_t)(0.9f * SR);
    Biquad hp = Biquad::hp(6000), bp = Biquad::bp(9000, 0.8f);
    float ph[6] = {}, inc[6];
    for (int j = 0; j < 6; j++) inc[j] = fr[j] * 1.7f / SR;
    float env = 1, dec = std::exp(-1.f / (0.28f * SR));
    for (size_t k = 0; k < n; k++) {
        float m = 0;
        for (int j = 0; j < 6; j++) {
            ph[j] += inc[j];
            if (ph[j] >= 1) ph[j] -= 1;
            m += ph[j] < 0.5f ? 1.f : -1.f;
        }
        float v = bp(hp(m * 0.15f + nz.white() * 0.3f)) * env;
        env *= dec;
        s.add(a + k, v * vel, 0.35f);
    }
}

void brush(Stereo& s, float start, float vel, Noise& nz) {
    size_t a = (size_t)(start * SR), n = (size_t)(0.35f * SR);
    Biquad bp = Biquad::bp(3500, 0.5f);
    for (size_t k = 0; k < n; k++) {
        float t = k / (float)SR;
        s.add(a + k, bp(nz.white()) * envAD(t, 0.025f, 0.09f) * vel, -0.25f);
    }
}

void softKick(Stereo& s, float start, float vel) {
    size_t a = (size_t)(start * SR), n = (size_t)(0.3f * SR);
    float ph = 0;
    for (size_t k = 0; k < n; k++) {
        float t = k / (float)SR;
        ph += 2 * PI * (60 * std::exp(-t / 0.04f) + 48) / SR;
        s.add(a + k, std::sin(ph) * envAD(t, 0.003f, 0.12f) * vel, 0);
    }
}

void vibes(Stereo& s, float start, float dur, float midi, float vel, float pan) {
    float hz = midiHz(midi);
    size_t a = (size_t)(start * SR), n = (size_t)((dur + 1.2f) * SR);
    for (size_t k = 0; k < n; k++) {
        float t = k / (float)SR;
        float e = envAD(t, 0.003f, 0.9f) * (t > dur ? std::exp(-(t - dur) / 0.3f) : 1.f);
        float trem = 1 + 0.25f * std::sin(2 * PI * 5.5f * t);
        float v = (std::sin(2 * PI * hz * t) + 0.18f * std::sin(2 * PI * hz * 4 * t) * std::exp(-t / 0.15f)) * e * trem;
        s.add(a + k, v * vel, pan);
    }
}

std::vector<int16_t> toPcm(Stereo& s, float gain) {
    float m = 0;
    for (size_t i = 0; i < s.L.size(); i++) m = std::max(m, std::max(std::fabs(s.L[i]), std::fabs(s.R[i])));
    float g = m > 0 ? gain / m : 1.f;
    std::vector<int16_t> out(s.L.size() * 2);
    for (size_t i = 0; i < s.L.size(); i++) {
        out[i * 2] = (int16_t)std::clamp(s.L[i] * g * 32767.f, -32767.f, 32767.f);
        out[i * 2 + 1] = (int16_t)std::clamp(s.R[i] * g * 32767.f, -32767.f, 32767.f);
    }
    return out;
}

// Wraps reverb/release tails that run past the loop end back onto the start.
void foldTail(Stereo& s, size_t loopLen) {
    for (size_t i = loopLen; i < s.L.size(); i++) {
        s.L[i - loopLen] += s.L[i];
        s.R[i - loopLen] += s.R[i];
    }
    s.L.resize(loopLen);
    s.R.resize(loopLen);
}

std::vector<int16_t> genLounge() {
    const float bpm = 86, beat = 60.f / bpm, bar = beat * 4;
    const int bars = 16;
    float total = bars * bar;
    Stereo s(total + 3.f);
    Noise nz;
    Rng r(1987);
    // F major lounge changes, two 8-bar phrases
    struct Chord { int root; std::vector<int> voicing; };
    std::vector<Chord> prog = {
        {43, {53, 57, 58, 62}}, {36, {52, 57, 58, 62}}, {41, {52, 55, 57, 60}}, {38, {53, 57, 60, 64}},
        {43, {53, 57, 58, 62}}, {36, {52, 55, 58, 61}}, {45, {55, 60, 64, 67}}, {43, {53, 58, 62, 65}},
        {43, {53, 57, 58, 62}}, {36, {52, 57, 58, 62}}, {41, {52, 55, 57, 60}}, {38, {53, 57, 60, 64}},
        {46, {53, 57, 58, 62}}, {36, {52, 55, 58, 61}}, {41, {52, 55, 57, 60}}, {36, {52, 57, 58, 62}},
    };
    auto swing = [&](float b) { // b in beats; swing the off-beat eighths
        float whole = std::floor(b), frac = b - whole;
        if (std::fabs(frac - 0.5f) < 0.01f) frac = 0.64f;
        return (whole + frac) * beat;
    };
    for (int i = 0; i < bars; i++) {
        float t0 = i * bar;
        const Chord& c = prog[i];
        const Chord& nx = prog[(i + 1) % bars];
        // comping
        static const float patterns[4][3] = {{0, 1.5f, 3.f}, {0.5f, 2.f, 3.5f}, {0, 2.5f, -1}, {1.5f, 3.f, -1}};
        const float* pat = patterns[r.range(0, 3)];
        for (int k = 0; k < 3; k++) {
            if (pat[k] < 0) continue;
            float st = t0 + swing(pat[k]);
            float len = k == 2 ? beat * 0.9f : beat * 0.6f;
            float vel = r.uniform(0.09f, 0.13f);
            for (size_t v = 0; v < c.voicing.size(); v++)
                rhodes(s, st + v * 0.006f, len, (float)c.voicing[v], vel, -0.2f + v * 0.12f);
        }
        // walking bass
        int root = c.root, next = nx.root;
        int line[4] = {root, root + (r.chance(0.5) ? 7 : 4), root + (r.chance(0.5) ? 12 : 7),
                       next + (r.chance(0.5) ? 1 : -1)};
        if (line[1] > root + 7) line[1] = root + 7;
        for (int k = 0; k < 4; k++) bass(s, t0 + k * beat, beat * 0.85f, (float)line[k], 0.55f);
        // drums: ride swing pattern, brushes on 2 & 4, feathered kick
        float ridePat[] = {0, 1, 1.5f, 2, 3, 3.5f};
        for (float rp : ridePat) ride(s, t0 + swing(rp), (rp == std::floor(rp) ? 0.18f : 0.11f), nz);
        brush(s, t0 + beat, 0.22f, nz);
        brush(s, t0 + 3 * beat, 0.22f, nz);
        softKick(s, t0, 0.35f);
        softKick(s, t0 + 2 * beat, 0.25f);
        // sparse vibraphone melody in the second phrase
        if (i >= 8 && i != 15) {
            int count = r.range(1, 3);
            float pos = (float)r.range(0, 1) * 0.5f;
            for (int k = 0; k < count && pos < 3.5f; k++) {
                int tone = c.voicing[r.range(0, 3)] + 12;
                float len = r.uniform(0.5f, 1.5f) * beat;
                vibes(s, t0 + swing(pos), len, (float)tone, 0.12f, 0.3f);
                pos += r.chance(0.5) ? 1.5f : 1.f;
            }
        }
    }
    reverb(s.L, s.R, 0.28f, 1.0f);
    foldTail(s, (size_t)(total * SR));
    return toPcm(s, 0.72f);
}

void stringsPad(Stereo& s, float start, float dur, const std::vector<int>& notes, float vel) {
    for (size_t ni = 0; ni < notes.size(); ni++) {
        float hz = midiHz((float)notes[ni]);
        size_t a = (size_t)(start * SR), n = (size_t)((dur + 0.8f) * SR);
        Biquad lp = Biquad::lp(1600, 0.7f);
        float ph[3] = {0.1f, 0.4f, 0.7f};
        float det[3] = {0.996f, 1.f, 1.004f};
        float pan = -0.4f + 0.8f * ni / std::max<size_t>(1, notes.size() - 1);
        for (size_t k = 0; k < n; k++) {
            float t = k / (float)SR;
            float e = std::min(1.f, t / 0.45f) * (t > dur ? std::exp(-(t - dur) / 0.35f) : 1.f);
            float v = 0;
            for (int j = 0; j < 3; j++) {
                float dt = hz * det[j] / SR;
                ph[j] += dt;
                if (ph[j] >= 1) ph[j] -= 1;
                v += (ph[j] * 2 - 1) - polyblep(ph[j], dt);
            }
            s.add(a + k, lp(v) * e * vel, pan);
        }
    }
}

void choir(Stereo& s, float start, float dur, float midi, float vel) {
    float hz = midiHz(midi);
    size_t a = (size_t)(start * SR), n = (size_t)((dur + 0.8f) * SR);
    Biquad f1 = Biquad::bp(700, 4), f2 = Biquad::bp(1150, 5), f3 = Biquad::bp(2600, 6);
    float ph = 0, ph2 = 0.5f;
    for (size_t k = 0; k < n; k++) {
        float t = k / (float)SR;
        float vib = 1 + 0.005f * std::sin(2 * PI * 5.f * t);
        ph += hz * vib / SR; ph2 += hz * 1.003f / SR;
        if (ph >= 1) ph -= 1;
        if (ph2 >= 1) ph2 -= 1;
        float src = (ph * 2 - 1) + (ph2 * 2 - 1);
        float v = f1(src) * 1.0f + f2(src) * 0.6f + f3(src) * 0.2f;
        float e = std::min(1.f, t / 0.6f) * (t > dur ? std::exp(-(t - dur) / 0.4f) : 1.f);
        s.add(a + k, v * e * vel, 0.1f);
    }
}

void taiko(Stereo& s, float start, float vel, Noise& nz) {
    size_t a = (size_t)(start * SR), n = (size_t)(0.9f * SR);
    float ph = 0;
    Biquad lp = Biquad::lp(400);
    for (size_t k = 0; k < n; k++) {
        float t = k / (float)SR;
        ph += 2 * PI * (95 * std::exp(-t / 0.08f) + 52) / SR;
        float v = std::sin(ph) * envAD(t, 0.002f, 0.32f) + lp(nz.white()) * envAD(t, 0.001f, 0.05f) * 0.6f;
        s.add(a + k, v * vel, 0);
    }
}

void ostinato(Stereo& s, float start, float len, float midi, float vel) {
    float hz = midiHz(midi);
    size_t a = (size_t)(start * SR), n = (size_t)((len + 0.15f) * SR);
    Biquad lp = Biquad::lp(900, 1.0f);
    float ph = 0, ph2 = 0.3f;
    for (size_t k = 0; k < n; k++) {
        float t = k / (float)SR;
        float e = envAD(t, 0.008f, 0.12f) * (t > len ? std::exp(-(t - len) / 0.04f) : 1.f);
        ph += hz / SR; ph2 += hz * 2.001f / SR;
        if (ph >= 1) ph -= 1;
        if (ph2 >= 1) ph2 -= 1;
        float v = (ph * 2 - 1) + 0.4f * (ph2 * 2 - 1);
        s.add(a + k, lp(v) * e * vel, -0.1f);
    }
}

std::vector<int16_t> genOlympus() {
    const float bpm = 92, beat = 60.f / bpm, bar = beat * 4;
    const int bars = 16;
    float total = bars * bar;
    Stereo s(total + 3.f);
    Noise nz;
    // D minor epic: i - VI - iv - V - i - III - VII - V
    struct C { int bassNote; std::vector<int> pad; int top; };
    std::vector<C> prog = {
        {38, {50, 57, 62, 65}, 74}, {34, {50, 53, 58, 62}, 74}, {31, {50, 55, 58, 62}, 70}, {33, {49, 52, 57, 61}, 73},
        {38, {50, 57, 62, 65}, 77}, {41, {53, 57, 60, 65}, 77}, {36, {48, 52, 55, 60}, 76}, {33, {49, 52, 57, 64}, 73},
    };
    for (int i = 0; i < bars; i++) {
        float t0 = i * bar;
        const C& c = prog[i % 8];
        bool second = i >= 8;
        stringsPad(s, t0, bar * 0.98f, c.pad, second ? 0.05f : 0.04f);
        if (second || i % 2 == 0) choir(s, t0, bar * 0.95f, (float)c.top, second ? 0.05f : 0.03f);
        for (int k = 0; k < 8; k++) {
            float vel = (k % 4 == 0) ? 0.16f : (k % 2 == 0 ? 0.11f : 0.08f);
            ostinato(s, t0 + k * beat * 0.5f, beat * 0.32f, (float)(c.bassNote + 12 + (k == 6 ? 7 : 0)), vel);
        }
        taiko(s, t0, 0.55f, nz);
        taiko(s, t0 + beat * 1.5f, 0.3f, nz);
        taiko(s, t0 + beat * 2, 0.45f, nz);
        if (second || i % 2 == 1) {
            taiko(s, t0 + beat * 3, 0.35f, nz);
            taiko(s, t0 + beat * 3.5f, 0.3f, nz);
        }
    }
    reverb(s.L, s.R, 0.38f, 1.35f);
    foldTail(s, (size_t)(total * SR));
    return toPcm(s, 0.75f);
}

// ---------------------------------------------------------------------------
// Mixer
// ---------------------------------------------------------------------------
struct Voice {
    const Buf* buf = nullptr;
    double pos = 0;
    float step = 1;
    float gl = 0, gr = 0;
    float vol = 1, pan = 0;
    float targetVol = 1, fadeRate = 0;
    bool loop = false;
    bool active = false;
    int handle = 0;
    bool isSfx = true;
};

constexpr int MAX_VOICES = 40;
SDL_AudioDeviceID g_dev = 0;
Buf g_sfx[SFX_COUNT];
Voice g_voices[MAX_VOICES];
int g_nextHandle = 1;
Track g_tracks[MUS_COUNT];
std::thread g_musicThread;
std::atomic<bool> g_quit{false};
struct MusicChan { size_t pos = 0; float gain = 0; float target = 0; float rate = 1; };
MusicChan g_mus[MUS_COUNT];
float g_master = 0.9f, g_musicVol = 0.6f, g_sfxVol = 0.9f;

void updateGains(Voice& v) {
    float p = std::clamp(v.pan, -1.f, 1.f);
    v.gl = v.vol * std::sqrt(0.5f * (1 - p));
    v.gr = v.vol * std::sqrt(0.5f * (1 + p));
}

void callback(void*, Uint8* stream, int len) {
    int frames = len / (int)(sizeof(int16_t) * 2);
    int16_t* out = (int16_t*)stream;
    static std::vector<float> mix;
    mix.assign((size_t)frames * 2, 0.f);
    const float dtFrame = 1.f / SR;
    for (auto& v : g_voices) {
        if (!v.active || !v.buf || v.buf->empty()) continue;
        const Buf& b = *v.buf;
        double n = (double)b.size();
        for (int i = 0; i < frames; i++) {
            if (v.fadeRate != 0) {
                v.vol += v.fadeRate * dtFrame;
                if ((v.fadeRate < 0 && v.vol <= v.targetVol) || (v.fadeRate > 0 && v.vol >= v.targetVol)) {
                    v.vol = v.targetVol;
                    v.fadeRate = 0;
                    if (v.vol <= 0.0001f) { v.active = false; break; }
                }
                updateGains(v);
            }
            if (v.pos >= n) {
                if (v.loop) v.pos = std::fmod(v.pos, n);
                else { v.active = false; break; }
            }
            size_t i0 = (size_t)v.pos;
            size_t i1 = i0 + 1 < b.size() ? i0 + 1 : (v.loop ? 0 : i0);
            float fr = (float)(v.pos - i0);
            float s = b[i0] + (b[i1] - b[i0]) * fr;
            mix[i * 2] += s * v.gl;
            mix[i * 2 + 1] += s * v.gr;
            v.pos += v.step;
        }
    }
    for (int m = 1; m < MUS_COUNT; m++) {
        MusicChan& c = g_mus[m];
        if (!g_tracks[m].ready.load()) continue;
        if (c.gain <= 0.0001f && c.target <= 0.0001f) continue;
        const auto& pcm = g_tracks[m].pcm;
        size_t nfr = pcm.size() / 2;
        for (int i = 0; i < frames; i++) {
            if (c.gain < c.target) c.gain = std::min(c.target, c.gain + c.rate * dtFrame);
            else if (c.gain > c.target) c.gain = std::max(c.target, c.gain - c.rate * dtFrame);
            float g = c.gain * g_musicVol / 32767.f;
            mix[i * 2] += pcm[c.pos * 2] * g;
            mix[i * 2 + 1] += pcm[c.pos * 2 + 1] * g;
            if (++c.pos >= nfr) c.pos = 0;
        }
    }
    for (int i = 0; i < frames * 2; i++) {
        float x = mix[i] * g_master;
        x = x / (1 + std::fabs(x) * 0.35f) * 1.2f; // gentle soft clip
        out[i] = (int16_t)std::clamp(x * 32767.f, -32767.f, 32767.f);
    }
}

Voice* allocVoice() {
    for (auto& v : g_voices)
        if (!v.active) return &v;
    // steal the quietest one-shot
    Voice* best = nullptr;
    for (auto& v : g_voices)
        if (!v.loop && (!best || v.vol < best->vol)) best = &v;
    return best;
}

} // namespace

void init() {
    g_sfx[SFX_NAV] = sfxNav();
    g_sfx[SFX_SELECT] = sfxSelect();
    g_sfx[SFX_BACK] = sfxBack();
    g_sfx[SFX_ERROR] = sfxError();
    g_sfx[SFX_CARD_SLIDE] = sfxCardSlide();
    g_sfx[SFX_CARD_FLIP] = sfxCardFlip();
    g_sfx[SFX_CHIP] = sfxChip();
    g_sfx[SFX_CHIPS] = sfxChips();
    g_sfx[SFX_SHUFFLE] = sfxShuffle();
    g_sfx[SFX_WIN] = sfxWin();
    g_sfx[SFX_BIGWIN] = sfxBigWin();
    g_sfx[SFX_LOSE] = sfxLose();
    g_sfx[SFX_BALL_LOOP] = sfxBallLoop();
    g_sfx[SFX_BALL_CLACK] = sfxBallClack();
    g_sfx[SFX_WHOOSH] = sfxWhoosh();
    g_sfx[SFX_REEL_DROP] = sfxReelDrop();
    g_sfx[SFX_LAND] = sfxLand();
    g_sfx[SFX_SHATTER] = sfxShatter();
    g_sfx[SFX_ZAP] = sfxZap();
    g_sfx[SFX_THUNDER] = sfxThunder();
    g_sfx[SFX_COIN] = sfxCoin();
    g_sfx[SFX_TICK] = sfxTick();
    g_sfx[SFX_ORB] = sfxOrb();
    g_sfx[SFX_BOOM] = sfxBoom();
    g_sfx[SFX_AMBIENCE] = sfxAmbience();
    g_sfx[SFX_KNOCK] = sfxKnock();
    g_sfx[SFX_KACHING] = sfxKaching();
    g_sfx[SFX_HEARTBEAT] = sfxHeartbeat();
    g_sfx[SFX_GLASS] = sfxGlass();
    g_sfx[SFX_RISER] = sfxRiser();
    g_sfx[SFX_ACHIEVE] = sfxAchieve();
    g_sfx[SFX_SPARKLE] = sfxSparkle();
    g_sfx[SFX_DRUMROLL] = sfxDrumroll();
    g_sfx[SFX_CYMBAL] = sfxCymbal();

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_Log("audio init failed: %s", SDL_GetError());
        return;
    }
    SDL_AudioSpec want{}, have{};
    want.freq = SR;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = callback;
    g_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!g_dev) {
        SDL_Log("open audio failed: %s", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(g_dev, 0);
    // Music takes a moment to synthesize, so it renders in the background.
    g_musicThread = std::thread([] {
        Uint64 t0 = SDL_GetPerformanceCounter();
        {
            Buf applause = sfxApplause(), cheer = sfxCheer(), groan = sfxGroan(), rain = sfxRain(), hit = sfxHit();
            SDL_LockAudioDevice(g_dev);
            g_sfx[SFX_APPLAUSE] = std::move(applause);
            g_sfx[SFX_CHEER] = std::move(cheer);
            g_sfx[SFX_GROAN] = std::move(groan);
            g_sfx[SFX_RAIN] = std::move(rain);
            g_sfx[SFX_HIT] = std::move(hit);
            SDL_UnlockAudioDevice(g_dev);
            SDL_Log("crowd sounds synthesized in %.0f ms", (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency());
            if (g_quit) return;
        }
        t0 = SDL_GetPerformanceCounter();
        auto lounge = genLounge();
        SDL_Log("lounge music synthesized in %.0f ms", (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency());
        if (g_quit) return;
        SDL_LockAudioDevice(g_dev);
        g_tracks[MUS_LOUNGE].pcm = std::move(lounge);
        g_tracks[MUS_LOUNGE].ready = true;
        SDL_UnlockAudioDevice(g_dev);
        t0 = SDL_GetPerformanceCounter();
        auto olympus = genOlympus();
        SDL_Log("olympus music synthesized in %.0f ms", (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency());
        if (g_quit) return;
        SDL_LockAudioDevice(g_dev);
        g_tracks[MUS_OLYMPUS].pcm = std::move(olympus);
        g_tracks[MUS_OLYMPUS].ready = true;
        SDL_UnlockAudioDevice(g_dev);
    });
}

void shutdown() {
    g_quit = true;
    if (g_musicThread.joinable()) g_musicThread.join();
    if (g_dev) SDL_CloseAudioDevice(g_dev);
    g_dev = 0;
}

void play(Sfx s, float vol, float pan, float pitch) {
    if (!g_dev || s < 0 || s >= SFX_COUNT) return;
    SDL_LockAudioDevice(g_dev);
    if (Voice* v = allocVoice()) {
        *v = Voice();
        v->buf = &g_sfx[s];
        v->vol = vol * g_sfxVol;
        v->pan = pan;
        v->step = pitch;
        v->active = true;
        updateGains(*v);
    }
    SDL_UnlockAudioDevice(g_dev);
}

int loop(Sfx s, float vol, float pitch) {
    if (!g_dev) return 0;
    int h = 0;
    SDL_LockAudioDevice(g_dev);
    if (Voice* v = allocVoice()) {
        *v = Voice();
        v->buf = &g_sfx[s];
        v->vol = vol * g_sfxVol;
        v->step = pitch;
        v->loop = true;
        v->active = true;
        v->handle = h = g_nextHandle++;
        updateGains(*v);
    }
    SDL_UnlockAudioDevice(g_dev);
    return h;
}

void setLoop(int handle, float vol, float pitch) {
    if (!g_dev || !handle) return;
    SDL_LockAudioDevice(g_dev);
    for (auto& v : g_voices)
        if (v.active && v.handle == handle && v.fadeRate == 0) {
            v.vol = vol * g_sfxVol;
            v.step = std::max(0.05f, pitch);
            updateGains(v);
        }
    SDL_UnlockAudioDevice(g_dev);
}

void stopLoop(int handle, float fade) {
    if (!g_dev || !handle) return;
    SDL_LockAudioDevice(g_dev);
    for (auto& v : g_voices)
        if (v.active && v.handle == handle) {
            v.targetVol = 0;
            v.fadeRate = -std::max(0.01f, v.vol) / std::max(0.01f, fade);
            v.handle = 0;
        }
    SDL_UnlockAudioDevice(g_dev);
}

void music(Music m, float fade) {
    if (!g_dev) return;
    SDL_LockAudioDevice(g_dev);
    for (int i = 1; i < MUS_COUNT; i++) {
        float target = (i == m) ? 1.f : 0.f;
        if (i == m && g_mus[i].target < 0.5f && g_mus[i].gain < 0.01f) g_mus[i].pos = 0;
        g_mus[i].target = target;
        g_mus[i].rate = 1.f / std::max(0.05f, fade);
    }
    SDL_UnlockAudioDevice(g_dev);
}

void setVolumes(float master, float musicVol, float sfx) {
    if (g_dev) SDL_LockAudioDevice(g_dev);
    g_master = master;
    g_musicVol = musicVol;
    g_sfxVol = sfx;
    if (g_dev) SDL_UnlockAudioDevice(g_dev);
}

bool musicReady() { return g_tracks[MUS_LOUNGE].ready.load(); }

} // namespace audio
