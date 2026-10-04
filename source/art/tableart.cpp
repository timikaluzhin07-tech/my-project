#include "tableart.h"

#include <cstring>

#include "../core/parallel.h"
#include "../core/save.h"
#include "../core/ui.h"
#include "shade.h"
#include "svg.h"

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

void woodGrain(Image& img, float cx, float cy, Color dark, Color light, uint32_t seed, bool polar, float stretch) {
    const float S = TEX_SCALE;
    parallelFor(img.h, [&](int y0, int y1) {
        for (int py = y0; py < y1; py++)
            for (int px = 0; px < img.w; px++) {
                uint8_t* p = img.at(px, py);
                if (!p[3]) continue;
                float x = px / S, y = py / S;
                float u, v;
                if (polar) {
                    float dx = x - cx, dy = (y - cy) * stretch;
                    u = std::sqrt(dx * dx + dy * dy);
                    v = std::atan2(dy, dx) * 120.f;
                } else {
                    u = y;
                    v = x;
                }
                float warp = noise::fbm(v / 90.f, u / 30.f, 3, seed) * 6.f;
                float rings = u * 0.55f + warp;
                float t = rings - std::floor(rings);
                float band = std::pow(std::sin(t * PI), 6.f); // dark late-wood lines
                float fleck = noise::value(v / 6.f, u / 1.2f, seed + 3);
                float k = 0.62f + 0.38f * noise::fbm(v / 40.f, u / 8.f, 2, seed + 7) - band * 0.35f + (fleck - 0.5f) * 0.12f;
                Color c = lerpColor(dark, light, clamp01(k));
                p[0] = c.r; p[1] = c.g; p[2] = c.b;
            }
    });
}

void feltLight(Image& img, float lx, float ly, float rx, float ry, const std::function<float(float, float)>& edge, float ao,
               float strength) {
    const float S = TEX_SCALE;
    parallelFor(img.h, [&](int y0, int y1) {
        for (int py = y0; py < y1; py++)
            for (int px = 0; px < img.w; px++) {
                uint8_t* p = img.at(px, py);
                if (!p[3]) continue;
                float x = px / S, y = py / S;
                float e = edge(x, y);
                if (e < 0) continue;
                float dx = (x - lx) / rx, dy = (y - ly) / ry;
                float lamp = 0.5f + 0.75f * std::exp(-(dx * dx + dy * dy) * 2.f);
                float occ = 0.55f + 0.45f * clamp01(e / ao);
                float k = 1 + (lamp * occ - 1) * strength;
                p[0] = (uint8_t)std::clamp(p[0] * k, 0.f, 255.f);
                p[1] = (uint8_t)std::clamp(p[1] * k, 0.f, 255.f);
                p[2] = (uint8_t)std::clamp(p[2] * k, 0.f, 255.f);
            }
    });
}

Image leatherRail(const std::string& svgPath, float w, float h, Color base, float padding, const std::string& stitchSvg) {
    std::vector<shade::Layer> L;
    shade::Layer rail;
    rail.svg = svg::open(w, h) + svg::path(svgPath, base.css(), "fill-rule=\"evenodd\"") + svg::close();
    rail.mat = shade::mat::leather();
    rail.bevel = padding;
    rail.depth = 0.55f;
    rail.grain = 0.06f;
    rail.grainCell = 2.2f;
    L.push_back(rail);
    if (!stitchSvg.empty()) {
        shade::Layer st;
        st.svg = svg::open(w, h) + stitchSvg + svg::close();
        st.mat = shade::mat::leather();
        st.bevel = 0.6f;
        st.depth = -0.8f;
        L.push_back(st);
    }
    return shade::relief(w, h, L);
}

Image regionRelief(float x, float y, float w, float h, std::vector<shade::Layer> layers, float margin) {
    const float S = TEX_SCALE;
    float W = w + 2 * margin, H = h + 2 * margin;
    std::string head = svg::open(W, H) + "<g transform=\"translate(" + svg::num(margin - x) + " " + svg::num(margin - y) + ")\">";
    for (auto& l : layers) l.svg = head + l.svg + "</g>" + svg::close();
    Image full = shade::relief(W, H, layers, S);
    Image out((int)std::ceil(w * S), (int)std::ceil(h * S));
    int ox = (int)std::lround(margin * S), oy = ox;
    for (int py = 0; py < out.h; py++) {
        int sy = py + oy;
        if (sy >= full.h) break;
        int n = std::min(out.w, full.w - ox);
        std::memcpy(out.at(0, py), full.at(ox, sy), (size_t)n * 4);
    }
    return out;
}

PortraitTraits portraitTraits(const std::string& name) {
    uint32_t hsh = 2166136261u;
    for (char ch : name) hsh = (hsh ^ (uint8_t)ch) * 16777619u;
    Rng r(hsh);
    PortraitTraits t;
    static const char* fem[] = {"Алиса", "Софи", "Ирина", "Ната", "Вера", "Кира"};
    for (auto f : fem) t.female |= name == f;
    static const uint32_t skins[] = {0xe0b090, 0xc89070, 0xa8704e, 0xf0c8a8, 0x8a5a3c};
    static const uint32_t hairs[] = {0x1a1210, 0x3a2414, 0x6a4020, 0xb08040, 0x2a2a2e, 0x8a1a10};
    static const uint32_t suits[] = {0x1a1c24, 0x2a0e14, 0x14241e, 0x24201a, 0x101014};
    t.skin = Color::hex(skins[r.range(0, 4)]);
    t.hair = Color::hex(hairs[r.range(0, 5)]);
    t.suit = Color::hex(suits[r.range(0, 4)]);
    t.hat = !t.female && r.chance(0.4);
    t.glasses = r.chance(0.45);
    t.beard = !t.female && r.chance(0.45);
    t.cigar = !t.female && r.chance(0.35);
    t.redTie = r.chance(0.5);
    t.earrings = t.female && r.chance(0.7);
    return t;
}

Image botPortrait(const std::string& name, float size) {
    PortraitTraits t = portraitTraits(name);
    bool female = t.female, hat = t.hat, glasses = t.glasses, beard = t.beard, cigar = t.cigar;
    Color skin = t.skin, hair = t.hair, suit = t.suit;
    std::vector<shade::Layer> L;
    auto doc = [](const std::string& b) { return svg::open(100, 100) + b + svg::close(); };
    auto add = [&](const std::string& b, shade::Material m, float bev, float depth = 1.f) {
        shade::Layer l;
        l.svg = doc(b);
        l.mat = m;
        l.bevel = bev;
        l.depth = depth;
        L.push_back(l);
    };
    // smoky backdrop
    shade::Layer bg;
    bg.svg = doc("<defs>" + svg::radial("bg", 0.62f, 0.3f, 0.8f, {{0, Color::hex(0x5a4430)}, {0.6f, Color::hex(0x1e1612)}, {1, Color::hex(0x0a0807)}}) +
                 "</defs>" + svg::circle(50, 50, 49, svg::url("bg")));
    bg.flat = true;
    L.push_back(bg);
    if (female) add(svg::path("M24 60 C18 30 30 14 50 14 C70 14 82 30 76 60 C80 76 70 86 50 86 C30 86 20 76 24 60 Z", hair.css()), shade::mat::leather(), 10);
    add(svg::path("M14 100 C16 78 32 70 50 70 C68 70 84 78 86 100 Z", suit.css()), shade::mat::leather(), 7);
    add(svg::path("M42 62 L58 62 L60 76 L40 76 Z", skin.scaled(0.85f).css()), shade::mat::clay(), 3);
    if (!female) add(svg::path("M38 78 L50 92 L62 78 L58 74 L50 84 L42 74 Z", "#e8e4dc"), shade::mat::paper(), 2);
    if (!female) add(svg::path("M44 80 L50 84 L56 80 L56 88 L50 85 L44 88 Z", t.redTie ? "#8a1020" : "#101014"), shade::mat::gloss(), 1.5f);
    add(svg::ellipse(50, 44, 17, 22, skin.css()), shade::mat::clay(), 12, 0.8f);
    if (!female && !hat) add(svg::path("M33 40 C33 22 44 18 52 18 C62 18 69 24 67 40 C64 30 56 28 50 28 C42 28 36 32 33 40 Z", hair.css()), shade::mat::leather(), 5);
    if (female) add(svg::path("M33 42 C34 24 44 20 52 20 C62 20 68 28 67 42 C62 32 54 28 46 30 C40 32 36 36 33 42 Z", hair.css()), shade::mat::leather(), 5);
    if (beard) add(svg::path("M34 46 C35 62 42 68 50 68 C58 68 65 62 66 46 C62 54 58 58 50 58 C42 58 38 54 34 46 Z", hair.css()), shade::mat::leather(), 4);
    if (glasses) add(svg::rect(36, 39, 12, 7, 3, "#0a0a0c") + svg::rect(52, 39, 12, 7, 3, "#0a0a0c") + svg::rect(47, 41, 6, 1.5f, 0.5f, "#2a2a2e"), shade::mat::gloss(), 2.5f);
    else add(svg::ellipse(43, 43, 2.2f, 1.6f, "#1a1010") + svg::ellipse(57, 43, 2.2f, 1.6f, "#1a1010"), shade::mat::gloss(), 1);
    add(svg::path("M45 55 Q50 57.5 55 55", "none", svg::stroke(skin.scaled(0.6f).css(), 1.4f)), shade::mat::clay(), 0.5f, -0.5f);
    if (hat) {
        add(svg::ellipse(50, 28, 30, 6, "#16141a"), shade::mat::leather(), 4);
        add(svg::path("M34 28 C34 12 40 8 50 8 C60 8 66 12 66 28 Z", "#1c1a20"), shade::mat::leather(), 6);
        add(svg::rect(34, 22, 32, 5, 1, "#5a1018"), shade::mat::leather(), 1.5f);
    }
    if (cigar) {
        add(svg::path("M56 57 L76 61 L76 64 L56 60 Z", "#6a4020"), shade::mat::leather(), 1.5f);
        add(svg::circle(76.5f, 62.5f, 2.2f, "#ff7a2a"), shade::mat::gloss(), 1);
    }
    if (t.earrings) add(svg::circle(33, 52, 2.4f, "#f0c45a") + svg::circle(67, 52, 2.4f, "#f0c45a"), shade::mat::gold(), 2);
    // gold bezel
    add(svg::path("M2 50 A48 48 0 1 0 98 50 A48 48 0 1 0 2 50 Z M7 50 A43 43 0 1 1 93 50 A43 43 0 1 1 7 50 Z", "#e2b450") + "", shade::mat::gold(), 2.2f);
    // clip to the circle (the relief layers overflow the frame)
    Image img = shade::relief(100, 100, L, TEX_SCALE * size / 100.f);
    float c = img.w / 2.f, rr = img.w / 2.f - 1;
    for (int y = 0; y < img.h; y++)
        for (int x = 0; x < img.w; x++) {
            float d = std::hypot(x + 0.5f - c, y + 0.5f - c);
            uint8_t* p = img.at(x, y);
            p[3] = (uint8_t)(p[3] * clamp01(rr - d + 0.5f));
        }
    return img;
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
    ui::plate(x, y, w, h, active, alpha);
    ui::chipDot(x + 20, cy, 9, playerColor(p.color).alpha(alpha));
    gfx::text(p.name, x + 36, cy - 9, F_SANS_BOLD, 16, pal::ivory, -1, alpha);
    std::string l2 = line2.empty() ? fmtMoney(p.balance) : line2;
    gfx::text(l2, x + 36, cy + 10, F_NUM, 14, line2.empty() ? pal::goldLight : line2Color, -1, alpha);
}

} // namespace art
