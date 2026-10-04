#include "tableart.h"

#include "../core/save.h"
#include "../core/ui.h"

namespace art {

void arcText(Image& img, const std::string& s, float cx, float cy, float rx, float ry, FontId f, float size, Color c,
             float tracking) {
    // split into UTF-8 code points
    std::vector<std::string> glyphs;
    for (size_t i = 0; i < s.size();) {
        unsigned char ch = (unsigned char)s[i];
        size_t n = ch < 0x80 ? 1 : (ch < 0xE0 ? 2 : (ch < 0xF0 ? 3 : 4));
        glyphs.push_back(s.substr(i, n));
        i += n;
    }
    std::vector<Image> masks;
    std::vector<float> adv;
    float total = 0;
    for (auto& g : glyphs) {
        masks.push_back(g == " " ? Image() : gfx::textMask(g, f, size, TEX_SCALE));
        float a = gfx::textWidth(g, f, size) * tracking;
        if (g == " ") a = size * 0.32f * tracking;
        adv.push_back(a);
        total += a;
    }
    auto localR = [&](float th) {
        float sx = rx * std::sin(th), sy = ry * std::cos(th);
        return std::sqrt(sx * sx + sy * sy);
    };
    // walk from the left end towards the right along the arc
    float th = PI / 2;
    {
        float half = total / 2, acc = 0;
        while (acc < half) { float d = 0.002f; acc += localR(th) * d; th += d; }
    }
    for (size_t i = 0; i < glyphs.size(); i++) {
        // centre of this glyph is half an advance further
        float half = adv[i] / 2, acc = 0, t2 = th;
        while (acc < half) { float d = 0.002f; acc += localR(t2) * d; t2 -= d; }
        if (!masks[i].empty()) {
            float x = cx + std::cos(t2) * rx, y = cy + std::sin(t2) * ry;
            // tangent direction gives the glyph rotation
            float tx = std::sin(t2) * rx, ty = -std::cos(t2) * ry;
            float ang = std::atan2(ty, tx);
            img.drawRotated(masks[i], x * TEX_SCALE, y * TEX_SCALE, ang, c);
        }
        acc = 0;
        while (acc < adv[i]) { float d = 0.002f; acc += localR(th) * d; th -= d; }
    }
}

void stampText(Image& img, const std::string& s, float cx, float cy, FontId f, float size, Color c, float angleDeg) {
    Image m = gfx::textMask(s, f, size, TEX_SCALE);
    if (m.empty()) return;
    img.drawRotated(m, cx * TEX_SCALE, cy * TEX_SCALE, angleDeg * PI / 180.f, c);
}

void drawChipRack(int selected, float cx, float y, i64 affordable, float alpha) {
    float sp = 54;
    float x0 = cx - sp * (CHIP_COUNT - 1) / 2.f;
    gfx::roundRect(x0 - 38, y - 30, sp * (CHIP_COUNT - 1) + 76, 60, 30, Color(8, 6, 8, 200).alpha(alpha));
    gfx::roundRectOutline(x0 - 38, y - 30, sp * (CHIP_COUNT - 1) + 76, 60, 30, 1, pal::gold.alpha(0.45f * alpha));
    for (int i = 0; i < CHIP_COUNT; i++) {
        float x = x0 + i * sp;
        bool s = i == selected;
        bool ok = CHIP_VALUES[i] <= affordable;
        float yy = y - (s ? 7.f : 0.f);
        if (s) gfx::glow(x, yy, 40, pal::gold, 0.45f * alpha);
        drawChip(i, x, yy, s ? 1.0f : 0.86f, (ok ? 1.f : 0.3f) * alpha);
        if (s) gfx::ring(x, yy, 24, 2, pal::goldLight.alpha(alpha));
    }
}

void Caption::render(float cx, float cy) const {
    if (t >= dur || text.empty()) return;
    float a = std::min(1.f, t * 5) * std::min(1.f, (dur - t) * 3);
    float w = gfx::textWidth(text, F_SANS_BOLD, 19) + 44;
    gfx::roundRect(cx - w / 2, cy - 17, w, 34, 17, Color(8, 6, 8, 200).alpha(a));
    gfx::roundRectOutline(cx - w / 2, cy - 17, w, 34, 17, 1, pal::gold.alpha(0.5f * a));
    gfx::text(text, cx, cy, F_SANS_BOLD, 19, pal::ivory, 0, a);
}

void seatPlate(int profile, float cx, float cy, bool active, float pulse, const std::string& line2, Color line2Color,
               float alpha, float width) {
    const Profile& p = save::player(profile);
    float w = width, h = 44, x = cx - w / 2, y = cy - h / 2;
    if (active) gfx::glowEllipse(cx, cy, w * 0.75f, h * 1.3f, pal::gold, (0.18f + 0.12f * pulse) * alpha);
    gfx::roundRect(x, y, w, h, 8, Color(8, 6, 9, 225).alpha(alpha));
    gfx::roundRectOutline(x, y, w, h, 8, active ? 1.8f : 1.f,
                          active ? pal::gold.alpha(alpha) : Color(255, 255, 255, 40).alpha(alpha));
    ui::chipDot(x + 20, cy, 9, playerColor(p.color).alpha(alpha));
    gfx::text(p.name, x + 36, cy - 9, F_SANS_BOLD, 16, pal::ivory, -1, alpha);
    std::string l2 = line2.empty() ? fmtMoney(p.balance) : line2;
    gfx::text(l2, x + 36, cy + 10, F_NUM, 14, line2.empty() ? pal::goldLight : line2Color, -1, alpha);
}

} // namespace art
