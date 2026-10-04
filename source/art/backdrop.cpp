#include "backdrop.h"

#include "../core/parallel.h"

namespace art {

Image velvetBackdrop(Color base, Color pattern, uint32_t seed) {
    const float S = TEX_SCALE;
    int W = (int)(SCREEN_W * S), H = (int)(SCREEN_H * S);
    Image img(W, H);
    parallelFor(H, [&](int yBegin, int yEnd) {
    for (int py = yBegin; py < yEnd; py++)
        for (int px = 0; px < W; px++) {
            float x = px / S, y = py / S;
            // damask: rotated lattice of soft lozenges with a central bloom
            float u = (x + y) / 64.f, v = (x - y) / 64.f;
            float fu = u - std::floor(u) - 0.5f, fv = v - std::floor(v) - 0.5f;
            float d = std::fabs(fu) + std::fabs(fv);
            float r = std::sqrt(fu * fu + fv * fv);
            float m = 0;
            m += std::exp(-std::pow((d - 0.42f) / 0.03f, 2.f)) * 0.8f;
            m += std::exp(-std::pow((r - 0.16f) / 0.025f, 2.f)) * 0.6f;
            m += std::exp(-r * r / 0.002f) * 0.7f;
            float n = noise::fbm(x / 140.f, y / 140.f, 3, seed);
            float folds = 0.85f + 0.3f * noise::value(x / 90.f, 0.5f, seed + 9); // vertical drape
            float k = (0.75f + 0.5f * n) * folds;
            uint8_t* p = img.at(px, py);
            p[0] = (uint8_t)std::clamp((base.r + (pattern.r - base.r) * m * 0.11f) * k, 0.f, 255.f);
            p[1] = (uint8_t)std::clamp((base.g + (pattern.g - base.g) * m * 0.11f) * k, 0.f, 255.f);
            p[2] = (uint8_t)std::clamp((base.b + (pattern.b - base.b) * m * 0.11f) * k, 0.f, 255.f);
            p[3] = 255;
        }
    });
    img.grain(0.04f, seed + 3);
    img.vignette(0.7f, 0.25f);
    return img;
}

Image feltImage(int w, int h, Color centre, Color edge, uint32_t seed, float scale) {
    int W = (int)(w * scale), H = (int)(h * scale);
    Image img(W, H);
    parallelFor(H, [&](int yBegin, int yEnd) {
    for (int py = yBegin; py < yEnd; py++)
        for (int px = 0; px < W; px++) {
            float nx = (px + 0.5f) / W - 0.5f, ny = (py + 0.5f) / H - 0.42f;
            float d = std::sqrt(nx * nx * 1.1f + ny * ny * 1.6f) * 1.6f;
            Color c = lerpColor(centre, edge, clamp01(d));
            uint8_t* p = img.at(px, py);
            p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = 255;
        }
    });
    img.grain(0.035f, seed);       // woven fibres
    img.grain(0.05f, seed + 1, 18); // soft blotches
    return img;
}

} // namespace art
