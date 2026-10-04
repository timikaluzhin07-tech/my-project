#include "slotart.h"

#include "art.h"
#include "shade.h"
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

// ------------------------------------------------------------------ symbols
using shade::Layer;
namespace M = shade::mat;

Layer layer(const std::string& body, const shade::Material& m, float bevel, float depth = 1.f, int shadow = 0) {
    Layer L;
    L.svg = svg::open(100, 100) + "<defs>" + goldDefs("g") + "</defs>" + body + svg::close();
    L.mat = m;
    L.bevel = bevel;
    L.depth = depth;
    L.shadow = shadow;
    L.shadowOpacity = 0.45f;
    return L;
}

const char* GOLD = "#efc25a";
const char* GOLD2 = "#d9a844";

std::vector<SDL_FPoint> ringPts(float cx, float cy, float rx, float ry, int n, float rot) {
    std::vector<SDL_FPoint> v;
    for (int i = 0; i < n; i++) {
        float a = rot + i * 2 * PI / n;
        v.push_back({cx + std::cos(a) * rx, cy + std::sin(a) * ry});
    }
    return v;
}

// A loose cut stone, 100x100 box.
Image gemArt(Sym s, float scale) {
    std::vector<SDL_FPoint> g;
    SDL_FPoint c = {50, 52};
    Color body;
    float table = 0.52f;
    switch (s) {
        case BLUE: g = ringPts(50, 52, 33, 41, 14, -PI / 2); body = Color::hex(0x1646c8); table = 0.56f; break;
        case GREEN:
            g = {{31, 11}, {69, 11}, {86, 28}, {86, 76}, {69, 93}, {31, 93}, {14, 76}, {14, 28}};
            body = Color::hex(0x0a8a46);
            table = 0.62f;
            break;
        case YELLOW: g = ringPts(50, 52, 43, 43, 6, -PI / 2); body = Color::hex(0xe39a0e); table = 0.5f; break;
        case PURPLE:
            for (int i = 0; i < 3; i++) {
                float a0 = -PI / 2 + i * 2 * PI / 3;
                for (int k = -1; k <= 2; k++) {
                    float a = a0 + k * 0.22f;
                    float r = (k == 0 || k == 1) ? 47.f : 41.f;
                    g.push_back({50 + std::cos(a) * r, 56 + std::sin(a) * r});
                }
            }
            c = {50, 56};
            body = Color::hex(0x7b2bd2);
            table = 0.48f;
            break;
        default: // ruby heart
            for (int i = 0; i < 26; i++) {
                float t = i * 2 * PI / 26;
                float x = 16 * std::pow(std::sin(t), 3.f);
                float y = 13 * std::cos(t) - 5 * std::cos(2 * t) - 2 * std::cos(3 * t) - std::cos(4 * t);
                g.push_back({50 + x * 2.6f, 48 - y * 2.6f});
            }
            c = {50, 47};
            body = Color::hex(0xc8102c);
            table = 0.5f;
            break;
    }
    Image img = shade::gem(100, 100, g, c, table, body, scale);
    shade::glints(img, 2, (uint32_t)s * 31 + 5, 7 * scale / TEX_SCALE * 1.4f);
    return img;
}

std::vector<Layer> chaliceLayers() {
    std::vector<Layer> L;
    L.push_back(layer(svg::path("M28 88 Q50 75 72 88 L76 95 L24 95 Z", GOLD2), M::gold(), 5, 1, 2));
    L.push_back(layer(svg::path("M45 58 L55 58 L53 82 L47 82 Z", GOLD), M::gold(), 4));
    L.push_back(layer(svg::ellipse(50, 69, 9, 4.5f, GOLD), M::gold(), 4));
    L.push_back(layer(svg::path("M14 14 C14 50 31 64 50 64 C69 64 86 50 86 14 Z", GOLD), M::gold(), 14, 0.9f, 2));
    L.push_back(layer(svg::path("M22 31 C28 48 40 55 50 55 C60 55 72 48 78 31", "none", svg::stroke("#8a5f12", 1.6f)), M::gold(), 1, -0.5f));
    L.push_back(layer(svg::ellipse(50, 14, 36, 7.5f, GOLD), M::gold(), 4));
    L.push_back(layer(svg::ellipse(50, 14, 30, 5, "#3a0610"), M::gloss(), 3, -0.6f));
    L.push_back(layer(svg::circle(50, 38, 8.5f, "#c8102c"), M::gloss(), 6, 1.4f, 1));
    L.push_back(layer(svg::circle(31, 33, 4.5f, "#1646c8") + svg::circle(69, 33, 4.5f, "#1646c8"), M::gloss(), 4, 1.3f));
    return L;
}

std::vector<Layer> ringLayers() {
    std::vector<Layer> L;
    L.push_back(layer(svg::path("M16 66 A34 25 0 1 0 84 66 A34 25 0 1 0 16 66 Z M27 66 A23 15 0 1 1 73 66 A23 15 0 1 1 27 66 Z",
                                GOLD, "fill-rule=\"evenodd\""),
                      M::gold(), 6, 1, 2));
    L.push_back(layer(svg::path("M34 47 L40 26 L46 47 Z", GOLD) + svg::path("M54 47 L60 26 L66 47 Z", GOLD), M::gold(), 2.5f));
    L.push_back(layer(svg::ellipse(50, 47, 17, 6, GOLD2), M::gold(), 4, 1, 1));
    return L;
}

std::vector<Layer> hourglassLayers() {
    std::vector<Layer> L;
    L.push_back(layer(svg::path("M33 18 C33 40 46 44 46 50 C46 56 33 60 33 82 L67 82 C67 60 54 56 54 50 C54 44 67 40 67 18 Z",
                                "#cfe6ff", "fill-opacity=\"0.32\""),
                      M::gloss(), 8, 1.2f));
    L.push_back(layer(svg::path("M37 82 C40 70 46 66 50 63 C54 66 60 70 63 82 Z", "#e8b45a"), M::clay(), 6, 0.8f));
    L.push_back(layer(svg::path("M38 28 C42 38 47 44 50 47 C53 44 58 38 62 28 Z", "#e8b45a"), M::clay(), 4, 0.8f));
    L.push_back(layer(svg::rect(49.2f, 47, 1.6f, 18, 0.8f, "#f2c66a"), M::clay(), 0.8f));
    L.push_back(layer(svg::rect(23, 17, 7, 66, 3, GOLD) + svg::rect(70, 17, 7, 66, 3, GOLD), M::gold(), 3.5f, 1, 1));
    L.push_back(layer(svg::rect(17, 7, 66, 13, 5, GOLD) + svg::rect(17, 80, 66, 13, 5, GOLD), M::gold(), 5, 1, 2));
    L.push_back(layer(svg::circle(32, 13.5f, 2.8f, "#c8102c") + svg::circle(50, 13.5f, 2.8f, "#1646c8") +
                          svg::circle(68, 13.5f, 2.8f, "#c8102c") + svg::circle(32, 86.5f, 2.8f, "#1646c8") +
                          svg::circle(50, 86.5f, 2.8f, "#c8102c") + svg::circle(68, 86.5f, 2.8f, "#1646c8"),
                      M::gloss(), 2.5f, 1.3f));
    return L;
}

std::vector<Layer> crownLayers() {
    std::vector<Layer> L;
    L.push_back(layer(svg::path("M22 56 Q50 10 78 56 Z", "#7a0c22"), M::leather(), 12, 1.1f));
    L.push_back(layer(svg::path("M12 80 L7 33 L28 55 L37 23 L50 46 L63 23 L72 55 L93 33 L88 80 Z", GOLD), M::gold(), 6, 1, 2));
    L.push_back(layer(svg::rect(11, 71, 78, 18, 4, GOLD2), M::gold(), 5, 1.1f, 1));
    L.push_back(layer(svg::circle(29, 80, 5.5f, "#c8102c") + svg::circle(71, 80, 5.5f, "#0a8a46"), M::gloss(), 5, 1.4f));
    L.push_back(layer(svg::circle(50, 80, 7, "#1646c8"), M::gloss(), 6, 1.4f));
    L.push_back(layer(svg::circle(7, 33, 5, "#f4efe4") + svg::circle(37, 23, 5, "#f4efe4") + svg::circle(63, 23, 5, "#f4efe4") +
                          svg::circle(93, 33, 5, "#f4efe4"),
                      M::ivory(), 5, 1.3f));
    L.push_back(layer(svg::circle(50, 11, 6.5f, GOLD) + svg::rect(48.4f, 1, 3.2f, 9, 1, GOLD), M::gold(), 3.5f, 1, 1));
    return L;
}

std::vector<Layer> scatterLayers() {
    std::vector<Layer> L;
    std::string d = "<defs>" + svg::radial("sky", 0.5f, 0.42f, 0.62f, {{0, Color::hex(0x2c5ae8)}, {0.6f, Color::hex(0x0d1f78)}, {1, Color::hex(0x040a2a)}}) + "</defs>";
    L.push_back(layer(d + svg::circle(50, 50, 46, svg::url("sky")), M::gloss(), 8, 0.9f, 2));
    std::string rays;
    for (int i = 0; i < 16; i++) {
        float a = i * PI / 8;
        rays += svg::path(pts({{50 + std::cos(a - 0.05f) * 12, 50 + std::sin(a - 0.05f) * 12}, {50 + std::cos(a) * 40, 50 + std::sin(a) * 40},
                               {50 + std::cos(a + 0.05f) * 12, 50 + std::sin(a + 0.05f) * 12}}),
                          "#a8c8ff", "fill-opacity=\"0.25\"");
    }
    Layer r = layer(rays, M::gloss(), 1);
    r.flat = true;
    L.push_back(r);
    L.push_back(layer(svg::path("M4 50 A46 46 0 1 0 96 50 A46 46 0 1 0 4 50 Z M10 50 A40 40 0 1 1 90 50 A40 40 0 1 1 10 50 Z", GOLD,
                                "fill-rule=\"evenodd\""),
                      M::gold(), 3.5f, 1.1f));
    L.push_back(layer(svg::path("M61 7 L30 54 L47 54 L37 93 L73 40 L55 40 L69 7 Z", GOLD), M::gold(), 4, 1.3f, 2));
    return L;
}

Image fitSymbol(const Image& art, float shadowAlpha = 0.6f) {
    Image out((int)(SYM * TEX_SCALE), (int)(SYM * TEX_SCALE));
    int ox = (out.w - art.w) / 2, oy = (out.h - art.h) / 2;
    Image sh = art.shadow((int)(3 * TEX_SCALE), shadowAlpha);
    int pad = (int)(3 * TEX_SCALE) * 2;
    out.draw(sh, ox - pad + (int)(2 * TEX_SCALE), oy - pad + (int)(4 * TEX_SCALE));
    out.draw(art, ox, oy);
    return out;
}

Image symbolImage(Sym s) {
    float inner = SYM * 0.88f;
    float sc = TEX_SCALE * inner / 100.f;
    Image art;
    switch (s) {
        case CHALICE: art = shade::relief(100, 100, chaliceLayers(), sc); break;
        case RING: {
            art = shade::relief(100, 100, ringLayers(), sc);
            Image stone = shade::gem(100, 100, ringPts(50, 30, 22, 19, 12, -PI / 2), {50, 30}, 0.55f, Color::hex(0x0a8a46), sc);
            Image sh = stone.shadow((int)(2 * sc), 0.5f);
            art.draw(sh, -(int)(4 * sc) + 1, -(int)(4 * sc) + 2);
            art.draw(stone, 0, 0);
            break;
        }
        case HOURGLASS: art = shade::relief(100, 100, hourglassLayers(), sc); break;
        case CROWN: art = shade::relief(100, 100, crownLayers(), sc); break;
        case SCATTER: art = shade::relief(100, 100, scatterLayers(), sc); break;
        default: art = gemArt(s, sc); break;
    }
    if (s == CROWN || s == CHALICE || s == HOURGLASS || s == RING || s == SCATTER) shade::glints(art, 2, (uint32_t)s * 7 + 1, 6.f);
    return fitSymbol(art);
}

Image orbImage(int value) {
    Color c = orbColor(value);
    float inner = SYM * 0.92f;
    Image orb = shade::glassOrb(inner / 2, c);
    Image out = fitSymbol(orb, 0.45f);
    std::string label = "x" + std::to_string(value);
    float size = value >= 100 ? 21 : 25;
    Image m = gfx::textMask(label, F_NUM, size, TEX_SCALE);
    if (!m.empty()) {
        Image outline = m.blurred(3);
        int tx = (out.w - m.w) / 2, ty = (out.h - m.h) / 2 + 2;
        for (int k = 0; k < 2; k++) out.drawTinted(outline, tx, ty, Color(0, 0, 0, 230));
        // metallic numerals
        Image num = m;
        for (size_t i = 0; i < num.px.size(); i += 4) { num.px[i] = 255; num.px[i + 1] = 246; num.px[i + 2] = 220; }
        shade::bevel(num, 1.4f * TEX_SCALE, 0.8f, shade::mat::silver());
        out.draw(num, tx, ty);
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
    int N = (int)std::ceil(D * TEX_SCALE);
    // meander band mask
    std::string ms = svg::open(D, D);
    int units = 36;
    float r0 = c * 0.865f, r1 = c * 0.945f;
    for (int i = 0; i < units; i++) {
        float a0 = i * 2 * PI / units, a1 = (i + 1) * 2 * PI / units;
        auto P2 = [&](float t, float rr) {
            float a = lerp(a0, a1, t);
            float r = lerp(r0, r1, rr);
            return svg::num(c + std::cos(a) * r) + " " + svg::num(c + std::sin(a) * r);
        };
        std::string d = "M" + P2(0, 0) + " L" + P2(0, 1) + " L" + P2(0.8f, 1) + " L" + P2(0.8f, 0.3f) + " L" + P2(0.35f, 0.3f) +
                        " L" + P2(0.35f, 0.65f) + " L" + P2(0.55f, 0.65f);
        ms += "<path d=\"" + d + "\" fill=\"none\" " + svg::stroke("#ffffff", 2.f) + "/>";
    }
    ms += svg::close();
    Image meander = gfx::rasterSvg(ms).blurred(2);
    float rs = TEX_SCALE * 1.18f;
    Image bustS = gfx::rasterSvg(reliefSvg(), rs), wreathS = gfx::rasterSvg(wreathSvg(), rs), engS = gfx::rasterSvg(engravingSvg(), rs);
    int ox = (N - bustS.w) / 2 + (int)(4 * TEX_SCALE), oy = (N - bustS.h) / 2 + (int)(2 * TEX_SCALE);
    auto place = [&](const Image& src) { Image full(N, N); full.draw(src, ox, oy); return full; };
    Image bust = place(bustS), bustB = bust.blurred(9), bustFine = bust.blurred(3);
    Image wreath = place(wreathS).blurred(3);
    Image eng = place(engS).blurred(1);
    Image coin(N, N);
    std::vector<float> H((size_t)N * N, 0.f);
    float S = TEX_SCALE;
    parallelFor(N, [&](int y0, int y1) {
        for (int y = y0; y < y1; y++)
            for (int x = 0; x < N; x++) {
                float dx = (x + 0.5f) / S - c, dy = (y + 0.5f) / S - c;
                float r = std::sqrt(dx * dx + dy * dy);
                float cover = clamp01((c - 1 - r) * S + 0.5f);
                if (cover <= 0) continue;
                auto sm = [](float e0, float e1, float v) { float t = clamp01((v - e0) / (e1 - e0)); return t * t * (3 - 2 * t); };
                float edge = sm(c - 1, c - 9, r);                      // rolled outer edge
                float rim = sm(c * 0.83f, c * 0.85f, r) * (1 - sm(c * 0.955f, c * 0.975f, r));
                float hgt = 0.35f * edge + 0.55f * rim * edge;
                bool field = r < c * 0.83f;
                float bustH = bustB.at(x, y)[3] / 255.f * 0.9f + bustFine.at(x, y)[3] / 255.f * 0.45f;
                if (field) hgt += bustH + wreath.at(x, y)[3] / 255.f * 0.45f - eng.at(x, y)[3] / 255.f * 0.35f;
                hgt += meander.at(x, y)[3] / 255.f * 0.22f;
                H[(size_t)y * N + x] = hgt;
                uint8_t* p = coin.at(x, y);
                Color al = field ? (bustH > 0.3f ? Color::hex(0xf2c35e) : Color::hex(0xcf9c3e)) : Color::hex(0xe8b44c);
                p[0] = al.r; p[1] = al.g; p[2] = al.b;
                p[3] = (uint8_t)(cover * 255);
            }
    });
    coin.grain(0.035f, 77, 2);
    shade::Material gold = shade::mat::gold();
    gold.shininess = 45;
    shade::shadeHeight(coin, H, 7.5f * S, gold);
    // dark patina in the recesses of the engraving
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            uint8_t* p = coin.at(x, y);
            float e = eng.at(x, y)[3] / 255.f * 0.55f;
            for (int k = 0; k < 3; k++) p[k] = (uint8_t)(p[k] * (1 - e));
        }
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
    auto doc = [&](const std::string& body) { return svg::open(W, H) + body + svg::close(); };
    std::string outer = "M0 9 Q0 0 9 0 L" + svg::num(W - 9) + " 0 Q" + svg::num(W) + " 0 " + svg::num(W) + " 9 L" + svg::num(W) + " " +
                        svg::num(H - 9) + " Q" + svg::num(W) + " " + svg::num(H) + " " + svg::num(W - 9) + " " + svg::num(H) + " L9 " +
                        svg::num(H) + " Q0 " + svg::num(H) + " 0 " + svg::num(H - 9) + " Z";
    std::string inner = " M" + svg::num(b) + " " + svg::num(b) + " L" + svg::num(b) + " " + svg::num(b + gh) + " L" + svg::num(b + gw) + " " +
                        svg::num(b + gh) + " L" + svg::num(b + gw) + " " + svg::num(b) + " Z";
    std::vector<Layer> L;
    Layer body;
    body.svg = doc(svg::path(outer + inner, "#d9a844", "fill-rule=\"evenodd\""));
    body.mat = shade::mat::gold();
    body.bevel = 9;
    body.depth = 0.7f;
    body.shadow = 4;
    body.shadowOpacity = 0.7f;
    L.push_back(body);
    std::string key;
    for (float yb : {6.f, H - b + 6}) {
        for (float x = 16; x + 16 < W - 14; x += 16) {
            std::string d = "M" + svg::num(x) + " " + svg::num(yb + 9) + " L" + svg::num(x) + " " + svg::num(yb) + " L" + svg::num(x + 12) + " " +
                            svg::num(yb) + " L" + svg::num(x + 12) + " " + svg::num(yb + 6) + " L" + svg::num(x + 5) + " " + svg::num(yb + 6) +
                            " L" + svg::num(x + 5) + " " + svg::num(yb + 3) + " L" + svg::num(x + 8) + " " + svg::num(yb + 3);
            key += "<path d=\"" + d + "\" fill=\"none\" " + svg::stroke("#f3cf72", 1.6f) + "/>";
        }
    }
    Layer meander;
    meander.svg = doc(key);
    meander.mat = shade::mat::gold();
    meander.bevel = 0.8f;
    meander.depth = 1.2f;
    L.push_back(meander);
    std::string ros, gems;
    for (P p : {P{b / 2, b / 2}, P{W - b / 2, b / 2}, P{b / 2, H - b / 2}, P{W - b / 2, H - b / 2}}) {
        ros += svg::circle(p.x, p.y, 10, "#e8b44c");
        gems += svg::circle(p.x, p.y, 5, "#c8102c");
    }
    Layer rosL;
    rosL.svg = doc(ros);
    rosL.mat = shade::mat::gold();
    rosL.bevel = 5;
    rosL.shadow = 1;
    L.push_back(rosL);
    Layer gemL;
    gemL.svg = doc(gems);
    gemL.mat = shade::mat::gloss();
    gemL.bevel = 4;
    gemL.depth = 1.3f;
    L.push_back(gemL);
    Image img = shade::relief(W, H, L);
    shade::glints(img, 6, 99, 7);
    return img;
}

Image buildCoin() {
    std::vector<Layer> L;
    Layer c;
    c.svg = svg::open(40, 40) + svg::circle(20, 20, 18, "#e8b44c") + svg::close();
    c.mat = shade::mat::gold();
    c.bevel = 4;
    L.push_back(c);
    Layer bolt;
    bolt.svg = svg::open(40, 40) + svg::path("M23 8 L13 22 L19 22 L15 32 L27 17 L21 17 L25 8 Z", "#f2c35e") + svg::close();
    bolt.mat = shade::mat::gold();
    bolt.bevel = 1.5f;
    bolt.depth = 1.2f;
    L.push_back(bolt);
    return shade::relief(40, 40, L);
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
        const Image* img = &art::cached("slot.sym" + std::to_string(i), [i] { return symbolImage((Sym)i); });
        a.sym[i] = gfx::upload(*img);
        a.symGlow[i] = gfx::upload(art::cached("slot.glow" + std::to_string(i), [img] {
            Image g = img->blurred(10);
            for (size_t k = 0; k < g.px.size(); k += 4) g.px[k] = g.px[k + 1] = g.px[k + 2] = 255;
            return g;
        }));
    }
    static const int vals[] = {2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 50, 100, 250, 500};
    for (int v : vals) a.orbs[v] = gfx::upload(art::cached("slot.orb" + std::to_string(v), [v] { return orbImage(v); }));
    a.medallion = gfx::upload(art::cached("slot.medallion", buildMedallion));
    a.background = gfx::upload(art::cached("slot.background", buildBackground));
    a.frame = gfx::upload(art::cached("slot.frame", buildFrame));
    a.coin = gfx::upload(art::cached("slot.coin", buildCoin));
}

} // namespace slotart
