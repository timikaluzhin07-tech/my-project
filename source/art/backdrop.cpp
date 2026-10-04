#include "backdrop.h"

#include "../core/parallel.h"
#include "shade.h"

namespace art {

Image velvetBackdrop(Color base, Color pattern, uint32_t seed) {
    const float S = TEX_SCALE;
    int W = (int)(SCREEN_W * S), H = (int)(SCREEN_H * S);
    Image img(W, H);
    std::vector<float> height((size_t)W * H);
    parallelFor(H, [&](int yBegin, int yEnd) {
    for (int py = yBegin; py < yEnd; py++)
        for (int px = 0; px < W; px++) {
            float x = px / S, y = py / S;
            // damask: staggered medallions (petals around a bloom) inside a lattice of S-scrolls
            float cw = 120, chh = 150;
            float row = std::floor(y / chh);
            float xs = x + (std::fmod(row, 2.f) != 0 ? cw / 2 : 0);
            float fu = (xs - std::floor(xs / cw) * cw) / cw - 0.5f, fv = (y - row * chh) / chh - 0.5f;
            float r = std::sqrt(fu * fu + fv * fv * 0.64f);
            float th = std::atan2(fv * 0.8f, fu);
            float m = 0;
            m += std::exp(-std::pow((r - 0.2f - 0.07f * std::cos(6 * th)) / 0.028f, 2.f)) * 0.9f;  // petals
            m += std::exp(-std::pow((r - 0.09f - 0.03f * std::cos(4 * th + 0.8f)) / 0.02f, 2.f)) * 0.7f;
            m += std::exp(-r * r / 0.0012f) * 0.8f;                                              // bloom
            float sc = std::fabs(std::fabs(fu) - 0.5f) + std::fabs(std::fabs(fv) - 0.5f);         // scroll lattice
            m += std::exp(-std::pow((sc - 0.22f - 0.05f * std::sin(fv * 18.f)) / 0.02f, 2.f)) * 0.6f;
            m = std::min(m, 1.f);
            float n = noise::fbm(x / 140.f, y / 140.f, 3, seed);
            float folds = 0.9f + 0.2f * noise::value(x / 90.f, 0.5f, seed + 9);
            float fibre = noise::value(x * 1.3f, y * 0.25f, seed + 5);
            size_t i = (size_t)py * W + px;
            height[i] = m * 0.7f + fibre * 0.06f;
            float k = (0.8f + 0.4f * n) * folds;
            Color c = lerpColor(base, pattern, m * 0.16f);
            uint8_t* p = img.at(px, py);
            p[0] = (uint8_t)std::clamp(c.r * k, 0.f, 255.f);
            p[1] = (uint8_t)std::clamp(c.g * k, 0.f, 255.f);
            p[2] = (uint8_t)std::clamp(c.b * k, 0.f, 255.f);
            p[3] = 255;
        }
    });
    shade::Material satin;
    satin.ambient = 0.5f;
    satin.diffuse = 0.6f;
    satin.spec = 0.35f;
    satin.shininess = 18;
    satin.reflect = 0.04f;
    satin.exposure = 1.25f;
    shade::shadeHeight(img, height, 2.2f * S, satin);
    // two wall sconces wash the wallpaper from above
    parallelFor(H, [&](int yBegin, int yEnd) {
        for (int py = yBegin; py < yEnd; py++)
            for (int px = 0; px < W; px++) {
                float x = px / S, y = py / S;
                float l = 0.55f;
                for (float lx : {250.f, 1030.f}) {
                    float dx = (x - lx) / 330.f, dy = (y + 40) / 520.f;
                    l += 0.75f * std::exp(-(dx * dx + dy * dy) * 2.2f);
                }
                uint8_t* p = img.at(px, py);
                for (int k = 0; k < 3; k++) p[k] = (uint8_t)std::min(255.f, p[k] * l);
            }
    });
    img.grain(0.035f, seed + 3);
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
