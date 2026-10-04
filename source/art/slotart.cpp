#include "slotart.h"

#include "svg.h"
#include "../core/parallel.h"

namespace slotart {

using namespace slot;

namespace {

struct P { float x, y; };

std::string pts(const std::vector<P>& p) {
    std::string d;
    for (size_t i = 0; i < p.size(); i++) d += (i ? " L" : "M") + svg::num(p[i].x) + " " + svg::num(p[i].y);
    return d + " Z";
}

std::string goldDefs(const std::string& id) {
    return svg::linear(id, 0, 0, 1, 0.3f,
                       {{0, Color::hex(0x6a4608)}, {0.22f, Color::hex(0xf7dc8a)}, {0.45f, Color::hex(0xc8962e)},
                        {0.7f, Color::hex(0xf6d985)}, {1, Color::hex(0x6a4608)}});
}

// Faceted "brilliant" gem: girdle outline -> table, triangular crown facets shaded by light.
std::string brilliant(const std::vector<P>& g, P c, float table, Color base, Color light, Color dark) {
    int n = (int)g.size();
    std::vector<P> t(n);
    for (int i = 0; i < n; i++) {
        P m = {(g[i].x + g[(i + 1) % n].x) / 2, (g[i].y + g[(i + 1) % n].y) / 2};
        t[i] = {c.x + (m.x - c.x) * table, c.y + (m.y - c.y) * table};
    }
    std::string s;
    s += svg::path(pts(g), dark.css());
    const float lx = -0.6f, ly = -0.8f;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        int pi = (i + n - 1) % n;
        // facet between girdle i..j and table i
        P a = g[i], b = g[j], tt = t[i];
        float mx = (a.x + b.x + tt.x) / 3 - c.x, my = (a.y + b.y + tt.y) / 3 - c.y;
        float l = std::sqrt(mx * mx + my * my) + 1e-3f;
        float k = 0.5f + 0.5f * (mx / l * lx + my / l * ly);
        k = clamp01(k * 1.1f - (i % 2) * 0.12f);
        Color col = k > 0.5f ? lerpColor(base, light, (k - 0.5f) * 2) : lerpColor(dark, base, k * 2);
        s += svg::path(pts({a, b, tt}), col.css());
        // facet between table i-1, table i and girdle i
        P t0 = t[pi];
        mx = (t0.x + tt.x + a.x) / 3 - c.x;
        my = (t0.y + tt.y + a.y) / 3 - c.y;
        l = std::sqrt(mx * mx + my * my) + 1e-3f;
        k = clamp01(0.5f + 0.5f * (mx / l * lx + my / l * ly) + 0.08f);
        col = k > 0.5f ? lerpColor(base, light, (k - 0.5f) * 2) : lerpColor(dark, base, k * 2);
        s += svg::path(pts({t0, tt, a}), col.css());
    }
    s += "<defs>" + svg::linear("tbl", 0, 0, 1, 1, {{0, light}, {0.55f, base}, {1, base.scaled(0.8f)}}) + "</defs>";
    s += svg::path(pts(t), svg::url("tbl"), svg::stroke(light.css(), 0.6f, 0.6f));
    s += svg::path(pts(g), "none", svg::stroke(dark.scaled(0.6f).css(), 1.6f));
    return s;
}

std::string sparkle(float x, float y, float r) {
    return svg::path("M" + svg::num(x) + " " + svg::num(y - r) + " L" + svg::num(x + r * 0.18f) + " " + svg::num(y - r * 0.18f) +
                         " L" + svg::num(x + r) + " " + svg::num(y) + " L" + svg::num(x + r * 0.18f) + " " + svg::num(y + r * 0.18f) +
                         " L" + svg::num(x) + " " + svg::num(y + r) + " L" + svg::num(x - r * 0.18f) + " " + svg::num(y + r * 0.18f) +
                         " L" + svg::num(x - r) + " " + svg::num(y) + " L" + svg::num(x - r * 0.18f) + " " + svg::num(y - r * 0.18f) + " Z",
                     "#ffffff", "fill-opacity=\"0.9\"");
}

std::vector<P> ring(P c, float rx, float ry, int n, float rot) {
    std::vector<P> v;
    for (int i = 0; i < n; i++) {
        float a = rot + i * 2 * PI / n;
        v.push_back({c.x + std::cos(a) * rx, c.y + std::sin(a) * ry});
    }
    return v;
}

std::string gemSymbol(Sym s) {
    std::string out = svg::open(100, 100);
    P c = {50, 52};
    switch (s) {
        case BLUE:
            out += brilliant(ring(c, 34, 41, 12, -PI / 2), c, 0.55f, Color::hex(0x2f6fe0), Color::hex(0xa8d0ff), Color::hex(0x0a2470));
            break;
        case GREEN: {
            // emerald step cut: three nested octagons
            std::vector<P> o = {{30, 12}, {70, 12}, {85, 27}, {85, 77}, {70, 92}, {30, 92}, {15, 77}, {15, 27}};
            Color base = Color::hex(0x1ea862), light = Color::hex(0x9af5c4), dark = Color::hex(0x05401e);
            out += svg::path(pts(o), dark.css());
            for (int k = 0; k < 3; k++) {
                float sc0 = 1 - k * 0.17f, sc1 = 1 - (k + 1) * 0.17f;
                for (int i = 0; i < 8; i++) {
                    int j = (i + 1) % 8;
                    auto S = [&](P p, float sc) { return P{c.x + (p.x - c.x) * sc, c.y + (p.y - c.y) * sc}; };
                    P a = S(o[i], sc0), b = S(o[j], sc0), bb = S(o[j], sc1), aa = S(o[i], sc1);
                    float mx = (a.x + b.x) / 2 - c.x, my = (a.y + b.y) / 2 - c.y;
                    float l = std::sqrt(mx * mx + my * my);
                    float kk = clamp01(0.5f + 0.5f * (mx / l * -0.6f + my / l * -0.8f) - k * 0.06f);
                    Color col = kk > 0.5f ? lerpColor(base, light, (kk - 0.5f) * 2) : lerpColor(dark, base, kk * 2);
                    out += svg::path(pts({a, b, bb, aa}), col.css());
                }
            }
            std::vector<P> inner;
            for (auto& p : o) inner.push_back({c.x + (p.x - c.x) * 0.49f, c.y + (p.y - c.y) * 0.49f});
            out += "<defs>" + svg::linear("tb", 0, 0, 1, 1, {{0, light}, {1, base}}) + "</defs>";
            out += svg::path(pts(inner), svg::url("tb"));
            out += svg::path(pts(o), "none", svg::stroke("#032a14", 1.6f));
            break;
        }
        case YELLOW:
            out += brilliant(ring(c, 42, 42, 6, -PI / 2), c, 0.5f, Color::hex(0xf2b51e), Color::hex(0xfff2a6), Color::hex(0x8a5400));
            break;
        case PURPLE: {
            std::vector<P> tri;
            for (int i = 0; i < 3; i++) {
                // slightly rounded trillion: three arcs approximated with 4 points each
                float a0 = -PI / 2 + i * 2 * PI / 3;
                for (int k = -1; k <= 2; k++) {
                    float a = a0 + k * 0.22f;
                    float r = k == 0 || k == 1 ? 46.f : 40.f;
                    tri.push_back({c.x + std::cos(a) * r, c.y + 4 + std::sin(a) * r});
                }
            }
            out += brilliant(tri, {c.x, c.y + 4}, 0.5f, Color::hex(0x9b44e3), Color::hex(0xe6c2ff), Color::hex(0x380c66));
            break;
        }
        case RED: {
            std::vector<P> h;
            for (int i = 0; i < 24; i++) {
                float t = i * 2 * PI / 24;
                float x = 16 * std::pow(std::sin(t), 3.f);
                float y = 13 * std::cos(t) - 5 * std::cos(2 * t) - 2 * std::cos(3 * t) - std::cos(4 * t);
                h.push_back({50 + x * 2.55f, 50 - y * 2.55f});
            }
            out += brilliant(h, {50, 48}, 0.52f, Color::hex(0xe3203d), Color::hex(0xffa2b0), Color::hex(0x640012));
            break;
        }
        default: break;
    }
    out += sparkle(34, 30, 9);
    out += svg::close();
    return out;
}

std::string chalice() {
    std::string s = svg::open(100, 100) + "<defs>" + goldDefs("g") +
                    svg::radial("ruby", 0.35f, 0.3f, 0.7f, {{0, Color::hex(0xff9aa8)}, {0.5f, Color::hex(0xd01430)}, {1, Color::hex(0x5a0010)}}) +
                    svg::radial("wine", 0.5f, 0.5f, 0.6f, {{0, Color::hex(0x8a1028)}, {1, Color::hex(0x2a0008)}}) + "</defs>";
    s += svg::path("M30 88 Q50 76 70 88 L74 94 L26 94 Z", svg::url("g"), svg::stroke("#4a3004", 1));
    s += svg::ellipse(50, 94, 24, 4, svg::url("g"), svg::stroke("#4a3004", 1));
    s += svg::path("M45 58 L55 58 L53 80 L47 80 Z", svg::url("g"), svg::stroke("#4a3004", 1));
    s += svg::ellipse(50, 68, 8, 4, svg::url("g"), svg::stroke("#4a3004", 0.8f));
    s += svg::path("M16 14 C16 48 32 62 50 62 C68 62 84 48 84 14 Z", svg::url("g"), svg::stroke("#4a3004", 1.4f));
    s += svg::path("M22 30 C28 48 40 54 50 54 C60 54 72 48 78 30", "none", svg::stroke("#fff1b8", 1.2f, 0.6f));
    s += svg::ellipse(50, 14, 34, 7, svg::url("g"), svg::stroke("#4a3004", 1.2f));
    s += svg::ellipse(50, 14, 29, 5, svg::url("wine"));
    s += svg::circle(50, 38, 8, svg::url("ruby"), svg::stroke("#4a3004", 1.2f));
    for (float x : {32.f, 68.f}) s += svg::circle(x, 34, 4, "#2f6fe0", svg::stroke("#4a3004", 0.8f));
    s += sparkle(28, 22, 7);
    s += svg::close();
    return s;
}

std::string ringSym() {
    std::string s = svg::open(100, 100) + "<defs>" +
                    svg::linear("band", 0, 0, 0, 1,
                                {{0, Color::hex(0xf9e39a)}, {0.35f, Color::hex(0xc8962e)}, {0.6f, Color::hex(0x7a5410)}, {1, Color::hex(0xe6c06a)}}) +
                    goldDefs("g") + "</defs>";
    s += svg::path("M18 66 A32 24 0 1 0 82 66 A32 24 0 1 0 18 66 Z M28 66 A22 15 0 1 1 72 66 A22 15 0 1 1 28 66 Z",
                   svg::url("band"), "fill-rule=\"evenodd\" " + svg::stroke("#4a3004", 1.2f));
    // prongs
    s += svg::path("M36 46 L40 30 L44 46 Z", svg::url("g"));
    s += svg::path("M56 46 L60 30 L64 46 Z", svg::url("g"));
    s += svg::ellipse(50, 47, 16, 5, svg::url("g"), svg::stroke("#4a3004", 1));
    s += brilliant(ring({50, 30}, 22, 20, 10, -PI / 2), {50, 30}, 0.55f, Color::hex(0x1fb06a), Color::hex(0xb0ffd6),
                   Color::hex(0x04401e));
    s += sparkle(40, 22, 7);
    s += svg::close();
    return s;
}

std::string hourglass() {
    std::string s = svg::open(100, 100) + "<defs>" + goldDefs("g") +
                    svg::linear("sand", 0, 0, 0, 1, {{0, Color::hex(0xf8d37a)}, {1, Color::hex(0xa8701e)}}) + "</defs>";
    s += svg::path("M33 18 C33 40 46 44 46 50 C46 56 33 60 33 82 L67 82 C67 60 54 56 54 50 C54 44 67 40 67 18 Z",
                   "#cfe6ff", "fill-opacity=\"0.28\" " + svg::stroke("#eef7ff", 1, 0.8f));
    s += svg::path("M37 82 C40 70 46 66 50 63 C54 66 60 70 63 82 Z", svg::url("sand"));
    s += svg::path("M38 28 C42 38 47 44 50 47 C53 44 58 38 62 28 Z", svg::url("sand"));
    s += svg::line(50, 47, 50, 66, "#f8d37a", 1.2f);
    s += svg::path("M37 22 C37 34 42 40 45 44", "none", svg::stroke("#ffffff", 1.4f, 0.6f));
    s += svg::rect(22, 76, 56, 6, 1, "#4a3004", "fill-opacity=\"0.3\"");
    s += svg::rect(24, 18, 6, 64, 2, svg::url("g"), svg::stroke("#4a3004", 0.8f));
    s += svg::rect(70, 18, 6, 64, 2, svg::url("g"), svg::stroke("#4a3004", 0.8f));
    s += svg::rect(18, 8, 64, 12, 4, svg::url("g"), svg::stroke("#4a3004", 1.2f));
    s += svg::rect(18, 80, 64, 12, 4, svg::url("g"), svg::stroke("#4a3004", 1.2f));
    for (float x : {32.f, 50.f, 68.f}) {
        s += svg::circle(x, 14, 2.6f, "#d01430");
        s += svg::circle(x, 86, 2.6f, "#2f6fe0");
    }
    s += sparkle(28, 12, 6);
    s += svg::close();
    return s;
}

std::string crown() {
    std::string s = svg::open(100, 100) + "<defs>" + goldDefs("g") +
                    svg::linear("vel", 0, 0, 0, 1, {{0, Color::hex(0xb0122e)}, {1, Color::hex(0x4a0010)}}) +
                    svg::radial("pearl", 0.35f, 0.3f, 0.7f, {{0, Color::hex(0xffffff)}, {1, Color::hex(0xbcb4a4)}}) + "</defs>";
    s += svg::path("M24 52 Q50 14 76 52 Z", svg::url("vel"));
    s += svg::path("M12 78 L8 34 L28 54 L37 24 L50 46 L63 24 L72 54 L92 34 L88 78 Z", svg::url("g"),
                   svg::stroke("#4a3004", 1.4f));
    s += svg::path("M18 70 L82 70", "none", svg::stroke("#fff1b8", 1.2f, 0.6f));
    s += svg::rect(12, 72, 76, 16, 3, svg::url("g"), svg::stroke("#4a3004", 1.4f));
    s += svg::circle(30, 80, 5, "#d01430", svg::stroke("#4a3004", 0.8f));
    s += svg::circle(50, 80, 6, "#2f6fe0", svg::stroke("#4a3004", 0.8f));
    s += svg::circle(70, 80, 5, "#1ea862", svg::stroke("#4a3004", 0.8f));
    for (P p : {P{8, 34}, P{37, 24}, P{63, 24}, P{92, 34}}) s += svg::circle(p.x, p.y, 4.5f, svg::url("pearl"));
    s += svg::circle(50, 12, 6, svg::url("g"), svg::stroke("#4a3004", 1));
    s += svg::rect(48.5f, 2, 3, 8, 1, svg::url("g"));
    s += sparkle(26, 60, 7);
    s += svg::close();
    return s;
}

std::string scatter() {
    std::string s = svg::open(100, 100) + "<defs>" + goldDefs("g") +
                    svg::radial("sky", 0.5f, 0.45f, 0.6f, {{0, Color::hex(0x3a6aff)}, {0.6f, Color::hex(0x10248a)}, {1, Color::hex(0x040a2a)}}) + "</defs>";
    s += svg::circle(50, 50, 46, svg::url("sky"));
    for (int i = 0; i < 16; i++) {
        float a = i * PI / 8;
        s += svg::path(pts({{50 + std::cos(a - 0.06f) * 10, 50 + std::sin(a - 0.06f) * 10},
                            {50 + std::cos(a) * 44, 50 + std::sin(a) * 44},
                            {50 + std::cos(a + 0.06f) * 10, 50 + std::sin(a + 0.06f) * 10}}),
                       "#9ec0ff", "fill-opacity=\"0.22\"");
    }
    s += svg::circle(50, 50, 46, "none", svg::stroke("#f3d27a", 3.5f));
    s += svg::circle(50, 50, 41, "none", svg::stroke("#f3d27a", 1, 0.7f));
    s += svg::path("M60 8 L30 54 L47 54 L37 92 L72 40 L54 40 L68 8 Z", svg::url("g"), svg::stroke("#fffbe0", 1.6f));
    s += svg::close();
    return s;
}

Image symbolImage(Sym s) {
    std::string src;
    switch (s) {
        case CHALICE: src = chalice(); break;
        case RING: src = ringSym(); break;
        case HOURGLASS: src = hourglass(); break;
        case CROWN: src = crown(); break;
        case SCATTER: src = scatter(); break;
        default: src = gemSymbol(s); break;
    }
    // rasterize into the padded symbol box, with a soft drop shadow
    float inner = SYM * 0.86f;
    Image art = gfx::rasterSvg(src, TEX_SCALE * inner / 100.f);
    Image out((int)(SYM * TEX_SCALE), (int)(SYM * TEX_SCALE));
    int ox = (out.w - art.w) / 2, oy = (out.h - art.h) / 2;
    Image sh = art.shadow(4, 0.6f);
    out.draw(sh, ox - 8 + 3, oy - 8 + 5);
    out.draw(art, ox, oy);
    return out;
}

Image orbImage(int value) {
    Color c = orbColor(value);
    std::string s = svg::open(100, 100) + "<defs>" +
                    svg::radial("o", 0.4f, 0.34f, 0.72f, {{0, c.scaled(2.4f)}, {0.4f, c.scaled(1.45f)}, {0.82f, c.scaled(0.85f)}, {1, c.scaled(0.5f)}}) +
                    "</defs>";
    s += svg::circle(50, 50, 44, svg::url("o"), svg::stroke(c.scaled(1.6f).css(), 2.f, 0.9f));
    // crackling energy inside
    Rng r(value * 13 + 7);
    for (int k = 0; k < 5; k++) {
        float a = r.uniform(0, 2 * PI), len = r.uniform(18, 36);
        std::string d = "M50 50";
        float x = 50, y = 50;
        for (int j = 0; j < 4; j++) {
            x += std::cos(a) * len / 4 + r.uniform(-4, 4);
            y += std::sin(a) * len / 4 + r.uniform(-4, 4);
            d += " L" + svg::num(x) + " " + svg::num(y);
        }
        s += "<path d=\"" + d + "\" fill=\"none\" " + svg::stroke(c.scaled(2.2f).css(), 1.2f, 0.55f) + "/>";
    }
    s += svg::ellipse(38, 30, 16, 9, "#ffffff", "fill-opacity=\"0.45\"");
    s += svg::close();
    float inner = SYM * 0.9f;
    Image art = gfx::rasterSvg(s, TEX_SCALE * inner / 100.f);
    Image out((int)(SYM * TEX_SCALE), (int)(SYM * TEX_SCALE));
    int ox = (out.w - art.w) / 2, oy = (out.h - art.h) / 2;
    out.draw(art, ox, oy);
    std::string label = "x" + std::to_string(value);
    float size = value >= 100 ? 21 : 25;
    Image m = gfx::textMask(label, F_NUM, size, TEX_SCALE);
    if (!m.empty()) {
        Image outline = m.blurred(3);
        int tx = (out.w - m.w) / 2, ty = (out.h - m.h) / 2 + 2;
        for (int k = 0; k < 2; k++) out.drawTinted(outline, tx, ty, Color(0, 0, 0, 230));
        out.drawTinted(m, tx, ty, Color::hex(0xfffbea));
    }
    return out;
}

// ------------------------------------------------------------------ Zeus medallion
const float MED = 300; // logical size

std::string reliefSvg() {
    // Zeus in profile facing left, laurel wreath, curly hair and beard (white = raised)
    std::string s = svg::open(200, 200);
    std::string W = "#ffffff";
    for (auto c : std::vector<std::array<float, 3>>{{96, 52, 20}, {116, 45, 21}, {137, 52, 21}, {151, 70, 20}, {156, 92, 19},
                                                    {150, 113, 18}, {138, 128, 16}, {126, 138, 14}})
        s += svg::circle(c[0], c[1], c[2], W);
    s += svg::path("M94 38 C82 42 75 54 75 66 C75 72 72 76 69 80 C67 84 65 88 63 93 L54 107 C56 110 60 111 63 111 "
                   "C63 114 61 116 61 118 L120 124 L134 96 L132 58 Z",
                   W);
    s += svg::path("M61 114 C51 128 53 147 61 161 C69 175 84 183 99 181 C113 179 125 169 131 153 C135 141 135 127 129 116 Z", W);
    for (auto c : std::vector<std::array<float, 3>>{{63, 152, 7}, {71, 166, 8}, {85, 177, 8}, {100, 177, 8}, {114, 171, 8},
                                                    {125, 159, 7}, {131, 143, 7}})
        s += svg::circle(c[0], c[1], c[2], W);
    s += svg::path("M116 150 C122 166 128 180 146 196 L176 200 L170 176 C156 168 146 152 140 136 Z", W);
    s += svg::close();
    return s;
}

std::string wreathSvg() {
    std::string s = svg::open(200, 200);
    // leaves along an arc over the forehead towards the nape
    for (int i = 0; i < 13; i++) {
        float t = i / 12.f;
        float a = lerp(-2.25f, -0.15f, t);
        float cx = 120 + std::cos(a) * 46, cy = 92 + std::sin(a) * 50;
        float ang = (a + PI / 2) * 180 / PI;
        for (int side = -1; side <= 1; side += 2) {
            float ox = std::cos(a) * side * 6, oy = std::sin(a) * side * 6;
            s += "<ellipse cx=\"" + svg::num(cx + ox) + "\" cy=\"" + svg::num(cy + oy) + "\" rx=\"8\" ry=\"3.4\" fill=\"#fff\" transform=\"rotate(" +
                 svg::num(ang + side * 28) + " " + svg::num(cx + ox) + " " + svg::num(cy + oy) + ")\"/>";
        }
    }
    s += svg::close();
    return s;
}

std::string engravingSvg() {
    std::string s = svg::open(200, 200);
    std::string ink = "#3a2604";
    auto st = [&](const std::string& d, float w, float o = 0.85f) {
        s += "<path d=\"" + d + "\" fill=\"none\" " + svg::stroke(ink, w, o) + " stroke-linecap=\"round\"/>";
    };
    st("M72 77 C76 74 82 74 88 77", 2.2f);              // brow
    st("M75 85 C78 82 83 82 86 85 C83 87 78 87 75 85", 1.6f); // eye
    st("M63 112 C68 115 74 116 80 114", 1.8f);          // mustache
    st("M66 118 C72 124 78 124 84 120", 1.4f, 0.6f);
    for (int k = 0; k < 6; k++) {                        // beard strands
        float x = 70 + k * 10;
        st("M" + svg::num(x) + " 126 C" + svg::num(x - 6) + " 140 " + svg::num(x + 4) + " 152 " + svg::num(x - 2) + " 168", 1.3f, 0.6f);
    }
    for (auto c : std::vector<std::array<float, 2>>{{116, 45}, {137, 52}, {151, 70}, {156, 92}, {150, 113}, {138, 128}}) // hair curls
        st("M" + svg::num(c[0] - 7) + " " + svg::num(c[1]) + " A7 7 0 1 1 " + svg::num(c[0]) + " " + svg::num(c[1] + 7), 1.3f, 0.55f);
    st("M128 74 C126 84 128 92 134 98", 1.4f, 0.5f);       // ear line
    s += svg::close();
    return s;
}

Image buildMedallion() {
    float D = MED, c = MED / 2;
    std::string s = svg::open(D, D) + "<defs>" +
                    svg::radialU("rim", c, c, c, {{0.8f, Color::hex(0xf0cf72)}, {0.9f, Color::hex(0xa77a22)}, {0.96f, Color::hex(0xf7e2a0)}, {1, Color::hex(0x6a4608)}}) +
                    svg::radialU("field", c - 30, c - 40, c * 0.95f, {{0, Color::hex(0xe8c25e)}, {0.7f, Color::hex(0xb88a2e)}, {1, Color::hex(0x7a5410)}}) +
                    "</defs>";
    s += svg::circle(c, c, c - 1, svg::url("rim"));
    s += svg::circle(c, c, c * 0.84f, "#8a6418");
    s += svg::circle(c, c, c * 0.81f, svg::url("field"));
    // meander ring
    int units = 36;
    float r0 = c * 0.86f, r1 = c * 0.95f;
    for (int i = 0; i < units; i++) {
        float a0 = i * 2 * PI / units, a1 = (i + 1) * 2 * PI / units;
        auto P2 = [&](float t, float rr) {
            float a = lerp(a0, a1, t);
            float r = lerp(r0, r1, rr);
            return svg::num(c + std::cos(a) * r) + " " + svg::num(c + std::sin(a) * r);
        };
        std::string d = "M" + P2(0, 0) + " L" + P2(0, 1) + " L" + P2(0.8f, 1) + " L" + P2(0.8f, 0.3f) + " L" + P2(0.35f, 0.3f) +
                        " L" + P2(0.35f, 0.65f) + " L" + P2(0.55f, 0.65f);
        s += "<path d=\"" + d + "\" fill=\"none\" " + svg::stroke("#5a3c08", 1.6f, 0.9f) + "/>";
    }
    s += svg::circle(c, c, r0, "none", svg::stroke("#5a3c08", 1.2f));
    s += svg::circle(c, c, r1, "none", svg::stroke("#5a3c08", 1.2f));
    s += svg::close();
    Image coin = gfx::rasterSvg(s);
    // relief: raise the bust out of the field with emboss lighting
    float rs = TEX_SCALE * 1.18f;
    Image bust = gfx::rasterSvg(reliefSvg(), rs);
    Image wreath = gfx::rasterSvg(wreathSvg(), rs);
    Image height(coin.w, coin.h);
    int ox = (coin.w - bust.w) / 2 + (int)(4 * TEX_SCALE), oy = (coin.h - bust.h) / 2 + (int)(2 * TEX_SCALE);
    height.draw(bust, ox, oy);
    Image hb = height.blurred(7);
    Image wb = Image(coin.w, coin.h);
    wb.draw(wreath, ox, oy);
    Image wbb = wb.blurred(3);
    float fieldR = c * 0.81f * TEX_SCALE;
    for (int y = 1; y < coin.h - 1; y++)
        for (int x = 1; x < coin.w - 1; x++) {
            float dx = x - coin.w / 2.f, dy = y - coin.h / 2.f;
            if (dx * dx + dy * dy > fieldR * fieldR) continue;
            auto H = [&](int xx, int yy) {
                return hb.at(xx, yy)[3] / 255.f + 0.6f * wbb.at(xx, yy)[3] / 255.f;
            };
            float gx = H(x + 1, y) - H(x - 1, y), gy = H(x, y + 1) - H(x, y - 1);
            float shade = (gx * 0.7f + gy * 0.7f) * -1.f * 2.2f; // light from the top-left
            float raised = H(x, y);
            uint8_t* p = coin.at(x, y);
            float lift = 1 + 0.18f * std::min(raised, 1.f);
            for (int k = 0; k < 3; k++) p[k] = (uint8_t)std::clamp(p[k] * lift + shade * 255.f, 0.f, 255.f);
        }
    Image eng = gfx::rasterSvg(engravingSvg(), rs);
    coin.draw(eng, ox, oy, 0.9f);
    coin.grain(0.03f, 77);
    return coin;
}

// ------------------------------------------------------------------ background
Image buildBackground() {
    const float S = TEX_SCALE;
    int W = (int)(SCREEN_W * S), H = (int)(SCREEN_H * S);
    Image img(W, H);
    auto col = [](uint32_t h) { Color c = Color::hex(h); return std::array<float, 3>{(float)c.r, (float)c.g, (float)c.b}; };
    auto mixc = [](std::array<float, 3> a, std::array<float, 3> b, float t) {
        return std::array<float, 3>{lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t)};
    };
    auto top = col(0x070a1e), mid = col(0x1a1640), hor = col(0x47264a), cl0 = col(0x10142e), cl1 = col(0x5a5896);
    parallelFor(H, [&](int yBegin, int yEnd) {
    for (int py = yBegin; py < yEnd; py++) {
        float y = py / S;
        for (int px = 0; px < W; px++) {
            float x = px / S;
            float t = y / 560.f;
            auto c = t < 0.55f ? mixc(top, mid, t / 0.55f) : mixc(mid, hor, clamp01((t - 0.55f) / 0.4f));
            // clouds
            float n = noise::fbm(x / 230.f, y / 120.f, 5, 11);
            float n2 = noise::fbm(x / 90.f + 13, y / 60.f, 3, 12);
            float dens = clamp01((n - 0.42f) / 0.3f) * clamp01(1.2f - y / 520.f);
            auto cc = mixc(cl0, cl1, clamp01(n2 * 1.2f - 0.1f) * clamp01(1 - y / 600.f));
            c = mixc(c, cc, dens * 0.85f);
            // distant Olympus range
            float mY = 560 - 70 * noise::fbm(x / 160.f, 3.3f, 4, 5) - 150 * std::exp(-std::pow((x - 640) / 230.f, 2.f));
            if (y > mY) {
                float edge = clamp01((y - mY) / 4.f);
                auto m = mixc(col(0x3a3460), col(0x0b0918), edge);
                if (y > mY + 30 && y < mY + 34) m = mixc(m, col(0x6a6aa8), 0.15f);
                c = mixc(m, col(0x050409), clamp01((y - mY - 40) / 160.f));
            }
            uint8_t* p = img.at(px, py);
            p[0] = (uint8_t)std::clamp(c[0], 0.f, 255.f);
            p[1] = (uint8_t)std::clamp(c[1], 0.f, 255.f);
            p[2] = (uint8_t)std::clamp(c[2], 0.f, 255.f);
            p[3] = 255;
        }
    }
    });
    // marble temple columns at both edges and an architrave with a gold meander
    std::string s = svg::open(SCREEN_W, SCREEN_H) + "<defs>" +
                    svg::linear("col", 0, 0, 1, 0, {{0, Color::hex(0x3a3a4c)}, {0.3f, Color::hex(0xb4b0c4)}, {0.55f, Color::hex(0x8a879c)}, {1, Color::hex(0x2a2a36)}}) +
                    svg::linear("arch", 0, 0, 0, 1, {{0, Color::hex(0x55536a)}, {1, Color::hex(0x2a2838)}}) +
                    goldDefs("g") + "</defs>";
    for (float cx : {42.f, 1238.f}) {
        s += svg::rect(cx - 34, 40, 68, 680, 0, svg::url("col"));
        for (int k = -3; k <= 3; k++) s += svg::line(cx + k * 9, 70, cx + k * 9, 720, "#1a1a26", 1.2f, 0.45f);
        s += svg::rect(cx - 44, 40, 88, 22, 3, svg::url("g"));
        s += svg::rect(cx - 40, 60, 80, 8, 2, "#8a6a28");
    }
    s += svg::rect(0, 0, SCREEN_W, 40, 0, svg::url("arch"));
    s += svg::rect(0, 38, SCREEN_W, 3, 0, "#b8902e");
    for (float x = 0; x < SCREEN_W; x += 22) {
        std::string d = "M" + svg::num(x) + " 32 L" + svg::num(x) + " 10 L" + svg::num(x + 17) + " 10 L" + svg::num(x + 17) + " 26 L" +
                        svg::num(x + 7) + " 26 L" + svg::num(x + 7) + " 18 L" + svg::num(x + 11) + " 18";
        s += "<path d=\"" + d + "\" fill=\"none\" " + svg::stroke("#c9a24a", 2.f, 0.8f) + "/>";
        s += svg::line(x, 32, x + 22, 32, "#c9a24a", 2.f, 0.8f);
    }
    s += svg::close();
    img.draw(gfx::rasterSvg(s), 0, 0);
    img.vignette(0.35f, 0.5f);
    return img;
}

Image buildFrame() {
    // gold frame around the 6x5 grid (logical layout matches the scene)
    float gw = CELL * slot::COLS, gh = CELL * slot::ROWS;
    float b = 20;
    float W = gw + b * 2, H = gh + b * 2;
    std::string s = svg::open(W, H) + "<defs>" +
                    svg::linear("fr", 0, 0, 0, 1, {{0, Color::hex(0xf9e39a)}, {0.3f, Color::hex(0xc8962e)}, {0.55f, Color::hex(0x8a6418)}, {0.8f, Color::hex(0xe6c06a)}, {1, Color::hex(0x7a5410)}}) +
                    "</defs>";
    std::string outer = "M0 8 Q0 0 8 0 L" + svg::num(W - 8) + " 0 Q" + svg::num(W) + " 0 " + svg::num(W) + " 8 L" + svg::num(W) + " " +
                        svg::num(H - 8) + " Q" + svg::num(W) + " " + svg::num(H) + " " + svg::num(W - 8) + " " + svg::num(H) + " L8 " +
                        svg::num(H) + " Q0 " + svg::num(H) + " 0 " + svg::num(H - 8) + " Z";
    std::string inner = " M" + svg::num(b) + " " + svg::num(b) + " L" + svg::num(b) + " " + svg::num(b + gh) + " L" + svg::num(b + gw) + " " +
                        svg::num(b + gh) + " L" + svg::num(b + gw) + " " + svg::num(b) + " Z";
    s += svg::path(outer + inner, svg::url("fr"), "fill-rule=\"evenodd\"");
    s += svg::rect(3.5f, 3.5f, W - 7, H - 7, 6, "none", svg::stroke("#5a3c08", 1.2f));
    s += svg::rect(b - 3, b - 3, gw + 6, gh + 6, 2, "none", svg::stroke("#fff1b8", 1.2f, 0.7f));
    // meander along top and bottom bands
    for (float yb : {6.f, H - b + 6}) {
        for (float x = 14; x + 16 < W - 10; x += 16) {
            std::string d = "M" + svg::num(x) + " " + svg::num(yb + 9) + " L" + svg::num(x) + " " + svg::num(yb) + " L" + svg::num(x + 12) + " " +
                            svg::num(yb) + " L" + svg::num(x + 12) + " " + svg::num(yb + 6) + " L" + svg::num(x + 5) + " " + svg::num(yb + 6) +
                            " L" + svg::num(x + 5) + " " + svg::num(yb + 3) + " L" + svg::num(x + 8) + " " + svg::num(yb + 3);
            s += "<path d=\"" + d + "\" fill=\"none\" " + svg::stroke("#5a3c08", 1.4f, 0.85f) + "/>";
        }
    }
    for (P p : {P{b / 2, b / 2}, P{W - b / 2, b / 2}, P{b / 2, H - b / 2}, P{W - b / 2, H - b / 2}}) {
        s += svg::circle(p.x, p.y, 9, svg::url("fr"), svg::stroke("#5a3c08", 1.2f));
        s += svg::circle(p.x, p.y, 4, "#d01430");
    }
    s += svg::close();
    return gfx::rasterSvg(s);
}

Image buildCoin() {
    std::string s = svg::open(40, 40) + "<defs>" + goldDefs("g") + "</defs>" + svg::circle(20, 20, 18, svg::url("g"), svg::stroke("#5a3c08", 1.4f)) +
                    svg::circle(20, 20, 13, "none", svg::stroke("#fff1b8", 1.2f, 0.7f)) +
                    svg::path("M23 9 L14 22 L19 22 L16 31 L26 18 L21 18 L24 9 Z", "#7a5410") + svg::close();
    return gfx::rasterSvg(s);
}

} // namespace

Color symbolColor(Sym s) {
    switch (s) {
        case BLUE: return Color::hex(0x5a9aff);
        case GREEN: return Color::hex(0x3ad88a);
        case YELLOW: return Color::hex(0xffd04a);
        case PURPLE: return Color::hex(0xc070ff);
        case RED: return Color::hex(0xff4a62);
        case SCATTER: return Color::hex(0x7aa8ff);
        default: return Color::hex(0xffd36a);
    }
}

Color orbColor(int v) {
    if (v >= 100) return Color::hex(0xd8501a);
    if (v >= 20) return Color::hex(0x8a2ad0);
    if (v >= 6) return Color::hex(0x1f62d0);
    return Color::hex(0x149a6a);
}

void build(Assets& a) {
    for (int i = 0; i < SYM_COUNT; i++) {
        if (i == ORB) continue;
        Image img = symbolImage((Sym)i);
        a.sym[i] = gfx::upload(img);
        Image g = img.blurred(10);
        for (size_t k = 0; k < g.px.size(); k += 4) g.px[k] = g.px[k + 1] = g.px[k + 2] = 255;
        a.symGlow[i] = gfx::upload(g);
    }
    static const int vals[] = {2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 50, 100, 250, 500};
    for (int v : vals) a.orbs[v] = gfx::upload(orbImage(v));
    a.medallion = gfx::upload(buildMedallion());
    a.background = gfx::upload(buildBackground());
    a.frame = gfx::upload(buildFrame());
    a.coin = gfx::upload(buildCoin());
}

} // namespace slotart
