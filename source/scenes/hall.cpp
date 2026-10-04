// Main menu: a dimly lit casino hall with four gaming stations.
#include "../art/art.h"
#include "../art/shade.h"
#include "../art/tableart.h"
#include "../art/svg.h"
#include "../core/app.h"
#include "../core/audio.h"
#include "../core/fx.h"
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/parallel.h"
#include "../core/platform.h"
#include "../core/save.h"
#include "../core/trophy.h"
#include "../core/ui.h"

namespace {

// Simple pinhole camera so the ceiling, back wall and carpet share one perspective.
constexpr float HZ = 300.f, FOC = 700.f, EYE = 1.6f, CEIL = 6.f, ZWALL = 30.f;
constexpr float BG_W = 1320.f;
const float WALL_TOP = HZ - (CEIL - EYE) * FOC / ZWALL;
const float WALL_BOT = HZ + EYE * FOC / ZWALL;

inline float frac(float x) { return x - std::floor(x); }

struct RGBf { float r, g, b; };
inline RGBf mix(RGBf a, RGBf b, float t) { return {lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t)}; }
inline RGBf rgb(uint32_t h) { return {(float)((h >> 16) & 255), (float)((h >> 8) & 255), (float)(h & 255)}; }

RGBf carpet(float u, float v) {
    const float tile = 1.15f;
    float fu = frac(u / tile) - 0.5f, fv = frac(v / tile) - 0.5f;
    RGBf base = rgb(0x3e0910), gold = rgb(0x7a5824), deep = rgb(0x1c0408), rose = rgb(0x561019);
    RGBf c = base;
    float d = std::fabs(fu) + std::fabs(fv);
    float r = std::sqrt(fu * fu + fv * fv);
    if (d < 0.33f) c = rose;
    if (std::fabs(d - 0.36f) < 0.028f) c = gold;
    if (std::fabs(r - 0.17f) < 0.02f) c = deep;
    if (r < 0.055f) c = gold;
    if (std::fabs(fu) > 0.45f || std::fabs(fv) > 0.45f) c = deep;
    // petals on the tile corners
    float cu = 0.5f - std::fabs(fu), cv = 0.5f - std::fabs(fv);
    if (std::sqrt(cu * cu + cv * cv) < 0.11f) c = gold;
    // border band every 6 tiles
    float band = frac(v / (tile * 6));
    if (band < 0.035f) c = gold;
    else if (band < 0.07f) c = deep;
    return c;
}

RGBf ceilingAt(float x, float y, float cx) {
    float z = (CEIL - EYE) * FOC / (HZ - y);
    float u = (x - cx) * z / FOC, v = z;
    const float P = 3.2f;
    float bu = frac(u / P + 0.5f), bv = frac(v / P);
    RGBf beam = rgb(0x2c1a10), goldE = rgb(0x8a6428), coffer = rgb(0x120a07), lamp = rgb(0xffd49a);
    RGBf c;
    float bw = 0.085f, ge = 0.016f;
    if (bu < bw || bv < bw) c = beam;
    else if (bu < bw + ge || bv < bw + ge) c = goldE;
    else {
        c = coffer;
        float du = bu - (0.5f + bw / 2), dv = bv - (0.5f + bw / 2);
        float l = std::exp(-(du * du + dv * dv) / 0.0035f);
        c = mix(c, lamp, l * 0.95f);
        float halo = std::exp(-(du * du + dv * dv) / 0.05f);
        c = {c.r + 40 * halo, c.g + 24 * halo, c.b + 10 * halo};
    }
    float far = clamp01((z - 9) / 20);
    c = mix(c, rgb(0x1a0f0a), far * 0.8f);
    float lightK = 0.35f + 0.65f * std::exp(-(z - 4.f) / 16.f);
    return {c.r * lightK, c.g * lightK, c.b * lightK};
}

RGBf wallAt(float x, float y, float cx) {
    float wx = (x - cx) * ZWALL / FOC;
    float wy = EYE + (HZ - y) * ZWALL / FOC; // height above floor, 0..CEIL
    RGBf c = mix(rgb(0x231610), rgb(0x150c08), clamp01(wy / CEIL));
    const float bay = 6.f;
    float bx = frac((wx + bay / 2) / bay) * bay - bay / 2; // -3..3 inside a bay, pillars at +-3
    float ax = std::fabs(bx);
    // arched openings into further rooms
    float archHalf = 1.9f, archSpring = 3.1f;
    bool inArch = false;
    if (ax < archHalf) {
        if (wy < archSpring) inArch = true;
        else {
            float dy = wy - archSpring;
            if (ax * ax + dy * dy * 1.6f < archHalf * archHalf) inArch = true;
        }
    }
    if (inArch) {
        c = mix(rgb(0x3b2516), rgb(0x1d120c), clamp01(wy / 4.4f));
        float glowv = std::exp(-((wy - 1.0f) * (wy - 1.0f)) / 0.9f);
        c = {c.r + 55 * glowv, c.g + 30 * glowv, c.b + 12 * glowv};
    } else {
        // arch moulding
        float dy = std::max(0.f, wy - archSpring);
        float rr = std::sqrt(ax * ax + dy * dy * 1.6f);
        if (rr < archHalf + 0.16f && rr >= archHalf) c = rgb(0x7d5a26);
        if (wy < 1.0f) c = mix(rgb(0x2a1a11), rgb(0x1a100b), wy); // wainscot
        if (std::fabs(wy - 1.0f) < 0.05f) c = rgb(0x8f6a2c);
    }
    // pillars
    float pd = 3.f - ax;
    if (pd < 0.45f) {
        float t = pd / 0.45f;
        c = mix(rgb(0x4a3526), rgb(0x2a1d15), std::fabs(t - 0.5f) * 2);
        if (std::fabs(wy - 4.9f) < 0.22f) c = rgb(0xb58a3a); // capital
        if (wy < 0.35f) c = rgb(0x6c4f25);                    // plinth
    }
    if (std::fabs(wy - 5.25f) < 0.07f) c = rgb(0xa47b33); // cornice
    // sconce light on each pillar
    float sx = ax - 3.f, sy = wy - 3.4f;
    float s = std::exp(-(sx * sx + sy * sy) / 0.35f);
    c = {c.r + 140 * s, c.g + 95 * s, c.b + 45 * s};
    return {c.r * 0.92f, c.g * 0.88f, c.b * 0.85f};
}

RGBf floorAt(float x, float y, float cx) {
    float z = EYE * FOC / (y - HZ);
    float u = (x - cx) * z / FOC, v = z;
    RGBf c = carpet(u, v);
    float far = clamp01((z - 5.f) / 14.f);
    c = mix(c, rgb(0x3a0a10), far);
    float n = noise::value(u * 9, v * 9, 77) * 0.12f + 0.94f;
    float k = (0.30f + 0.70f * std::exp(-(z - 2.5f) / 11.f)) * n;
    if (z < 3.2f) k *= 0.55f + 0.45f * clamp01((z - 2.2f) / 1.0f); // foreground falls into shadow
    return {c.r * k, c.g * k, c.b * k};
}

Image buildBackground() {
    const float S = TEX_SCALE;
    int W = (int)(BG_W * S), H = (int)(SCREEN_H * S);
    Image img(W, H);
    float cx = BG_W / 2;
    const float offs[4][2] = {{0.25f, 0.25f}, {0.75f, 0.25f}, {0.25f, 0.75f}, {0.75f, 0.75f}};
    parallelFor(H, [&](int yBegin, int yEnd) {
    for (int py = yBegin; py < yEnd; py++) {
        for (int px = 0; px < W; px++) {
            RGBf acc = {0, 0, 0};
            int samples = 0;
            bool ss = true;
            float yc = (py + 0.5f) / S;
            if (yc >= WALL_TOP + 1 && yc < WALL_BOT - 1) ss = false;
            for (int k = 0; k < (ss ? 4 : 1); k++) {
                float x = (px + (ss ? offs[k][0] : 0.5f)) / S, y = (py + (ss ? offs[k][1] : 0.5f)) / S;
                RGBf c;
                if (y < WALL_TOP) c = ceilingAt(x, y, cx);
                else if (y < WALL_BOT) c = wallAt(x, y, cx);
                else c = floorAt(x, y, cx);
                acc.r += c.r; acc.g += c.g; acc.b += c.b;
                samples++;
            }
            uint8_t* p = img.at(px, py);
            p[0] = (uint8_t)std::clamp(acc.r / samples, 0.f, 255.f);
            p[1] = (uint8_t)std::clamp(acc.g / samples, 0.f, 255.f);
            p[2] = (uint8_t)std::clamp(acc.b / samples, 0.f, 255.f);
            p[3] = 255;
        }
    }
    });
    img.vignette(0.55f, 0.35f);
    return img;
}

// ---------------------------------------------------------------------------
// Station illustrations: layered SVG drawings shaded as lit solids (shade::relief).
// ---------------------------------------------------------------------------
struct Rel {
    float w, h;
    std::string defs;
    std::vector<shade::Layer> L;
    Rel(float w_, float h_, std::string d = "") : w(w_), h(h_), defs(std::move(d)) {}
    shade::Layer& add(const std::string& body, const shade::Material& m, float bevel, float depth = 1.f) {
        shade::Layer l;
        l.svg = svg::open(w, h) + (defs.empty() ? "" : "<defs>" + defs + "</defs>") + body + svg::close();
        l.mat = m;
        l.bevel = bevel;
        l.depth = depth;
        L.push_back(std::move(l));
        return L.back();
    }
    shade::Layer& ink(const std::string& body) {
        shade::Layer& l = add(body, shade::mat::paper(), 1);
        l.flat = true;
        return l;
    }
    Image render(float scale = TEX_SCALE) { return shade::relief(w, h, L, scale); }
};

std::string goldGrad(const std::string& id) {
    return svg::linear(id, 0, 0, 0, 1,
                       {{0, Color::hex(0xf7e19c)}, {0.45f, Color::hex(0xd1a646)}, {0.6f, Color::hex(0x9c7428)},
                        {1, Color::hex(0xdcb75c)}});
}

std::string softShadow(float cx, float cy, float rx, float ry, float opacity = 0.7f) {
    return "<defs>" + svg::radial("sh", 0.5f, 0.5f, 0.5f, {{0, Color(0, 0, 0, (uint8_t)(255 * opacity))}, {0.6f, Color(0, 0, 0, (uint8_t)(170 * opacity))}, {1, Color(0, 0, 0, 0)}}) +
           "</defs>" + svg::ellipse(cx, cy, rx, ry, svg::url("sh"));
}

// Chip stacks seen from table height; ellipses of one stack merge into a rounded column.
std::string miniChips(float x, float y, int n, uint32_t seed) {
    static const char* cols[] = {"#b3202e", "#1f5fae", "#232327", "#1f8a52", "#5b2c86", "#d9a62b"};
    std::string s;
    Rng r(seed);
    std::string col = cols[r.range(0, 5)];
    for (int k = 0; k < n; k++) {
        if (r.chance(0.3)) col = cols[r.range(0, 5)];
        float yy = y - k * 2.1f;
        s += svg::ellipse(x, yy, 7, 3.2f, col, svg::stroke("#f2eee4", 0.7f, 0.8f) + " stroke-dasharray=\"1.5 2\"");
    }
    return s;
}

std::string miniCard(float x, float y, float rot, bool red, bool back = false) {
    std::string g = "<g transform=\"translate(" + svg::num(x) + " " + svg::num(y) + ") rotate(" + svg::num(rot) +
                    ") scale(1 0.6)\">";
    if (back) return g + svg::rect(-5, -7, 10, 14, 1.2f, "#7a1028", svg::stroke("#e2b450", 0.5f)) + "</g>";
    g += svg::rect(-5, -7, 10, 14, 1.2f, "#f6f1e6");
    g += svg::circle(0, 0, 2.2f, red ? "#b0142c" : "#16151b");
    return g + "</g>";
}

void woodPaint(shade::Layer& l, Color dark, Color light, uint32_t seed, float cx = 0, float cy = 0, bool polar = false, float stretch = 1) {
    l.paint = [=](Image& im) { art::woodGrain(im, cx, cy, dark, light, seed, polar, stretch); };
    l.grain = 0;
}

Image blackjackStation() {
    Rel r(300, 230, svg::radialU("felt", 150, 112, 150, {{0, Color::hex(0x2a9a66)}, {0.7f, Color::hex(0x16683f)}, {1, Color::hex(0x0b3a22)}}));
    r.ink(softShadow(150, 214, 92, 14));
    shade::Layer& ped = r.add(svg::rect(124, 160, 52, 58, 4, "#3a2216"), shade::mat::lacquer(), 14, 0.8f);
    woodPaint(ped, Color::hex(0x1a0c06), Color::hex(0x4a2a16), 3);
    r.add(svg::path("M10 120 A140 70 0 0 0 290 120 L290 140 A140 70 0 0 1 10 140 Z", "#2a170e"), shade::mat::leather(), 3, 0.8f);
    shade::Layer& rail = r.add(svg::path("M10 120 A140 70 0 0 0 290 120 L274 120 A124 59 0 0 1 26 120 Z", "#4a2c1c"), shade::mat::leather(), 7, 0.8f);
    rail.grain = 0.06f;
    shade::Layer& felt = r.add(svg::path("M26 120 A124 59 0 0 0 274 120 Z", svg::url("felt")), shade::mat::felt(), 1.5f, 0.5f);
    felt.grain = 0.07f;
    std::string pr = svg::path("M58 121 A92 42 0 0 0 242 121", "none", svg::stroke("#e4c56a", 1.1f, 0.6f));
    pr += svg::path("M72 121 A78 33 0 0 0 228 121", "none", svg::stroke("#e4c56a", 0.7f, 0.5f));
    for (int i = 0; i < 5; i++) {
        float a = (155 - i * 32.5f) * PI / 180;
        pr += svg::ellipse(150 + std::cos(a) * 104, 120 + std::sin(a) * 50, 10, 4.6f, "none", svg::stroke("#e4c56a", 1.f, 0.8f));
    }
    r.ink(pr);
    shade::Layer& edge = r.add(svg::rect(18, 112, 264, 8, 2.5f, "#5a3018"), shade::mat::lacquer(), 2.5f, 0.8f);
    woodPaint(edge, Color::hex(0x2a1208), Color::hex(0x8a4e24), 7);
    r.add(svg::rect(18, 119, 264, 1.4f, 0.5f, "#e2b450"), shade::mat::gold(), 0.6f);
    r.add(svg::rect(110, 101, 80, 15, 2.5f, "#150d09"), shade::mat::lacquer(), 2.5f).shadow = 2;
    static const char* tray[] = {"#b3202e", "#1f5fae", "#232327", "#1f8a52", "#5b2c86", "#d9a62b", "#b3202e", "#232327"};
    std::string tr;
    for (int i = 0; i < 8; i++) {
        tr += svg::rect(114.5f + i * 9, 103.5f, 7.6f, 10, 2, tray[i]);
        for (float y = 105; y < 113; y += 2.2f) tr += svg::rect(114.5f + i * 9, y, 7.6f, 0.5f, 0, "#000000", "fill-opacity=\"0.35\"");
    }
    r.add(tr, shade::mat::clay(), 3.8f, 0.9f);
    shade::Layer& shoe = r.add(svg::path("M222 101 L252 101 L257 118 L219 118 Z", "#141318"), shade::mat::gloss(), 3, 0.9f);
    shoe.shadow = 2;
    r.add(svg::rect(227, 104, 18, 9, 1.2f, "#7a1028", svg::stroke("#e2b450", 0.5f)), shade::mat::lacquer(), 1, -0.5f);
    r.add(svg::rect(219, 114, 38, 4, 1.5f, "#c9cad2"), shade::mat::silver(), 1.2f);
    std::string chips, cards;
    for (int i = 0; i < 5; i++) {
        float a = (155 - i * 32.5f) * PI / 180;
        float x = 150 + std::cos(a) * 104, y = 120 + std::sin(a) * 50;
        if (i != 2) chips += miniChips(x, y, 2 + i % 3, 40 + i);
        if (i % 2 == 0) {
            cards += miniCard(x - 5, y - 9, -8, i == 0);
            cards += miniCard(x + 2, y - 10, 6, false);
        }
    }
    shade::Layer& ch = r.add(chips, shade::mat::clay(), 2.5f, 0.8f);
    ch.shadow = 2;
    cards += miniCard(142, 109, 0, true) + miniCard(155, 109, 0, false, true);
    shade::Layer& cl = r.add(cards, shade::mat::paper(), 0.8f, 0.6f);
    cl.shadow = 1;
    return r.render();
}

Image pokerStation() {
    Rel r(300, 230, svg::radialU("felt", 150, 118, 125, {{0, Color::hex(0x2290aa)}, {0.7f, Color::hex(0x145a6e)}, {1, Color::hex(0x0a3240)}}));
    std::string chairsBack, chairsFront;
    for (float x : {82.f, 150.f, 218.f}) chairsBack += svg::rect(x - 17, 48, 34, 46, 10, "#6a1824");
    r.add(chairsBack, shade::mat::leather(), 9, 0.8f).grain = 0.05f;
    r.ink(softShadow(150, 214, 100, 15));
    shade::Layer& ped = r.add(svg::rect(126, 154, 48, 64, 4, "#3a2216"), shade::mat::lacquer(), 13, 0.8f);
    woodPaint(ped, Color::hex(0x1a0c06), Color::hex(0x4a2a16), 4);
    r.add(svg::path("M8 125 A142 62 0 0 0 292 125 L292 142 A142 62 0 0 1 8 142 Z", "#1e1612"), shade::mat::leather(), 3, 0.8f);
    shade::Layer& rail = r.add(svg::path("M8 125 A142 62 0 1 0 292 125 A142 62 0 1 0 8 125 Z M25 125 A125 53 0 1 1 275 125 A125 53 0 1 1 25 125 Z", "#2e2624",
                                         "fill-rule=\"evenodd\""),
                               shade::mat::leather(), 8, 0.8f);
    rail.grain = 0.06f;
    shade::Layer& wood = r.add(svg::path("M25 125 A125 53 0 1 0 275 125 A125 53 0 1 0 25 125 Z M33 125 A117 48 0 1 1 267 125 A117 48 0 1 1 33 125 Z", "#7a4c26",
                                         "fill-rule=\"evenodd\""),
                               shade::mat::lacquer(), 2.5f, 0.8f);
    woodPaint(wood, Color::hex(0x3a1a08), Color::hex(0x9a5a2a), 9, 150, 125, true, 125.f / 53.f);
    shade::Layer& felt = r.add(svg::ellipse(150, 125, 117, 48, svg::url("felt")), shade::mat::felt(), 1.5f, 0.5f);
    felt.grain = 0.07f;
    r.ink(svg::ellipse(150, 125, 88, 34, "none", svg::stroke("#ffffff", 0.6f, 0.22f)));
    std::string cards;
    for (int i = 0; i < 5; i++) cards += miniCard(126 + i * 12, 118, 0, i == 1 || i == 3);
    r.add(cards, shade::mat::paper(), 0.8f, 0.6f).shadow = 1;
    std::string chips = miniChips(150, 142, 5, 7) + miniChips(162, 140, 3, 9);
    for (int i = 0; i < 6; i++) {
        float a = (i * 60 + 30) * PI / 180;
        chips += miniChips(150 + std::cos(a) * 92, 125 + std::sin(a) * 38, 3 + (i * 7) % 4, 100 + i);
    }
    r.add(chips, shade::mat::clay(), 2.5f, 0.8f).shadow = 2;
    r.add(svg::ellipse(196, 132, 5, 2.4f, "#f4efe4"), shade::mat::ivory(), 1.5f).shadow = 1;
    for (float x : {40.f, 260.f}) chairsFront += svg::rect(x - 18, 150, 36, 48, 11, "#6a1824");
    shade::Layer& cf = r.add(chairsFront, shade::mat::leather(), 10, 0.8f);
    cf.grain = 0.05f;
    cf.shadow = 4;
    return r.render();
}

Image rouletteStation() {
    Rel r(300, 230);
    r.ink(softShadow(155, 212, 140, 16));
    r.add(svg::rect(36, 188, 14, 26, 3, "#24120a") + svg::rect(260, 188, 14, 26, 3, "#24120a"), shade::mat::lacquer(), 4);
    r.add(svg::path("M18 174 L292 174 L286 196 L24 196 Z", "#2a160c"), shade::mat::lacquer(), 3, 0.8f);
    shade::Layer& top = r.add(svg::path("M64 64 L268 64 L296 180 L14 180 Z", "#6a3a1a"), shade::mat::lacquer(), 5, 0.8f);
    woodPaint(top, Color::hex(0x2a1208), Color::hex(0x8a4e24), 12);
    shade::Layer& felt = r.add(svg::path("M74 72 L260 72 L284 171 L26 171 Z", "#1a7046"), shade::mat::felt(), 1.5f, -0.4f);
    felt.grain = 0.07f;
    // layout grid in perspective (bilinear across a quad)
    float qx[4] = {128, 252, 272, 140}, qy[4] = {82, 82, 160, 160};
    auto P = [&](float u, float v, float& x, float& y) {
        float tx = lerp(qx[0], qx[1], u), bx = lerp(qx[3], qx[2], u);
        x = lerp(tx, bx, v);
        y = lerp(qy[0], qy[3], v);
    };
    auto quadD = [&](float u0, float v0, float u1, float v1) {
        float x0, y0, x1, y1, x2, y2, x3, y3;
        P(u0, v0, x0, y0); P(u1, v0, x1, y1); P(u1, v1, x2, y2); P(u0, v1, x3, y3);
        return "M" + svg::num(x0) + " " + svg::num(y0) + " L" + svg::num(x1) + " " + svg::num(y1) + " L" + svg::num(x2) + " " +
               svg::num(y2) + " L" + svg::num(x3) + " " + svg::num(y3) + " Z";
    };
    static const int reds[] = {1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36};
    std::string grid;
    for (int col = 0; col < 12; col++)
        for (int row = 0; row < 3; row++) {
            int num = col * 3 + (3 - row);
            bool red = std::find(std::begin(reds), std::end(reds), num) != std::end(reds);
            grid += svg::path(quadD(col / 12.f, row / 4.f, (col + 1) / 12.f, (row + 1) / 4.f), red ? "#a3182a" : "#141216",
                              svg::stroke("#e9dcb6", 0.5f, 0.8f) + " fill-opacity=\"0.85\"");
        }
    for (int k = 0; k < 3; k++) grid += svg::path(quadD(k / 3.f, 0.75f, (k + 1) / 3.f, 1), "none", svg::stroke("#e9dcb6", 0.5f, 0.8f));
    {
        float x0, y0, x3, y3;
        P(0, 0, x0, y0); P(0, 0.75f, x3, y3);
        grid += svg::path("M" + svg::num(x0) + " " + svg::num(y0) + " L" + svg::num(x3) + " " + svg::num(y3) + " L" +
                              svg::num(x3 - 13) + " " + svg::num((y0 + y3) / 2) + " Z",
                          "#1d7a4c", svg::stroke("#e9dcb6", 0.5f, 0.8f));
    }
    r.ink(grid);
    r.add(miniChips(170, 110, 3, 21) + miniChips(214, 128, 2, 22) + miniChips(236, 100, 4, 23), shade::mat::clay(), 2.5f, 0.8f).shadow = 2;
    // wheel in perspective
    auto g = [](const std::string& b) { return "<g transform=\"translate(80 112) scale(1 0.5)\">" + b + "</g>"; };
    shade::Layer& bowl = r.add(g(svg::circle(0, 0, 55, "#6b3b1c")), shade::mat::lacquer(), 7, 0.6f);
    bowl.shadow = 3;
    woodPaint(bowl, Color::hex(0x2a1006), Color::hex(0x7e4620), 5, 80, 112, true, 2.f);
    r.add(g(svg::circle(0, 0, 45, "#a87038")), shade::mat::lacquer(), 4, -0.6f);
    std::string sec;
    for (int i = 0; i < 37; i++) {
        float a0 = i * 2 * PI / 37, a1 = (i + 1) * 2 * PI / 37;
        sec += svg::path(svg::sector(0, 0, 28, 38, a0, a1), i == 0 ? "#1d7a4c" : (i % 2 ? "#a3182a" : "#141216"));
    }
    r.add(g(sec), shade::mat::gloss(), 1.5f, 0.6f);
    shade::Layer& cone = r.add(g(svg::circle(0, 0, 28, "#8a5228")), shade::mat::lacquer(), 10, 0.5f);
    woodPaint(cone, Color::hex(0x3a1a08), Color::hex(0xa0602c), 13, 80, 112, true, 2.f);
    r.add(g(svg::rect(-2.2f, -21, 4.4f, 42, 2.2f, "#e2b450") + svg::rect(-21, -2.2f, 42, 4.4f, 2.2f, "#e2b450") + svg::circle(0, 0, 6.5f, "#e2b450")),
          shade::mat::gold(), 2.2f).shadow = 1;
    r.add(svg::circle(110, 99, 2.4f, "#fbfbf7"), shade::mat::ivory(), 2.4f).shadow = 1;
    return r.render();
}

Image slotMachine(bool lit, float scale) {
    Rel r(150, 300, goldGrad("g") + svg::linear("scr", 0, 0, 0, 1, {{0, Color::hex(0x24378a)}, {1, Color::hex(0x0b0f2c)}}) +
                        svg::linear("top", 0, 0, 0, 1, {{0, Color::hex(0x2a46b0)}, {1, Color::hex(0x0a1236)}}));
    r.add(svg::rect(16, 286, 118, 12, 3, "#0a080b"), shade::mat::gloss(), 3);
    r.add(svg::rect(20, 180, 110, 110, 6, "#221e28"), shade::mat::gloss(), 8, 0.8f);
    r.add(svg::path("M14 166 L136 166 L142 186 L8 186 Z", "#2a2630"), shade::mat::gloss(), 3);
    static const char* btn[] = {"#e04040", "#f0c040", "#40a8f0", "#40e080", "#f0f0f0"};
    std::string b;
    for (int i = 0; i < 5; i++) b += svg::rect(22 + i * 22, 171, 16, 8, 3, btn[i], lit ? "" : "fill-opacity=\"0.5\"");
    r.add(b, shade::mat::gloss(), 3, 1.f);
    // gold cabinet frame and the screen
    r.add(svg::path("M12 60 L12 32 Q75 -8 138 32 L138 60 Z", "#e2b450") + svg::rect(12, 56, 126, 114, 8, "#e2b450"), shade::mat::gold(), 5, 0.9f);
    r.add(svg::rect(19, 65, 112, 98, 4, svg::url("scr")), shade::mat::gloss(), 3, -0.8f);
    r.add(svg::path("M19 56 L19 36 Q75 2 131 36 L131 56 Z", svg::url("top")), shade::mat::gloss(), 3, -0.6f);
    std::string gems;
    static const char* gem[] = {"#ff4a64", "#b860ff", "#50e888", "#50a0ff", "#ffd050", "#ff4a64"};
    for (int c = 0; c < 6; c++)
        for (int rr = 0; rr < 5; rr++) {
            float x = 28 + c * 18.8f, y = 74 + rr * 19;
            const char* col = gem[(c * 7 + rr * 3) % 6];
            if ((c + rr * 2) % 7 == 3) gems += svg::circle(x, y, 7, "#8fdcff");
            else gems += svg::path("M" + svg::num(x) + " " + svg::num(y - 7) + " L" + svg::num(x + 6) + " " + svg::num(y) + " L" + svg::num(x) +
                                       " " + svg::num(y + 7) + " L" + svg::num(x - 6) + " " + svg::num(y) + " Z",
                                   col);
        }
    r.add(gems, shade::mat::gloss(), 2.5f, 1.f);
    std::string bolts;
    for (float bx : {36.f, 114.f})
        bolts += svg::path("M" + svg::num(bx + 4) + " 22 L" + svg::num(bx - 7) + " 38 L" + svg::num(bx) + " 38 L" + svg::num(bx - 5) + " 53 L" +
                               svg::num(bx + 9) + " 33 L" + svg::num(bx + 2) + " 33 L" + svg::num(bx + 8) + " 22 Z",
                           "#f0c860");
    r.add(bolts, shade::mat::gold(), 1.6f, 1.f);
    r.add(svg::rect(3, 30, 6, 264, 3, lit ? "#5ab8ff" : "#1a2a4a") + svg::rect(141, 30, 6, 264, 3, lit ? "#5ab8ff" : "#1a2a4a"), shade::mat::gloss(), 3);
    Image img = r.render(scale);
    if (lit) shade::glints(img, 3, 77, 7 * scale);
    return img;
}

Image slotStation() {
    const float S = TEX_SCALE;
    Image canvas((int)(300 * S), (int)(310 * S));
    canvas.draw(gfx::rasterSvg(svg::open(300, 310) + softShadow(150, 300, 150, 12, 0.8f) + svg::close()), 0, 0);
    for (int side = 0; side < 2; side++) {
        Image back = slotMachine(false, S * 0.78f);
        for (size_t i = 0; i < back.px.size(); i += 4) {
            back.px[i] = (uint8_t)(back.px[i] * 0.45f);
            back.px[i + 1] = (uint8_t)(back.px[i + 1] * 0.45f);
            back.px[i + 2] = (uint8_t)(back.px[i + 2] * 0.5f);
        }
        int x = side == 0 ? (int)(10 * S) : (int)(173 * S);
        canvas.draw(back, x, (int)(52 * S));
    }
    Image main = slotMachine(true, S);
    canvas.draw(main.shadow((int)(6 * S), 0.6f), (int)(75 * S) - (int)(12 * S), (int)(8 * S) - (int)(10 * S));
    canvas.draw(main, (int)(75 * S), (int)(8 * S));
    // "ZEUS" lettering on the topper, gold foil
    Image t = gfx::textMask("ZEUS", F_TITLE, 16, S);
    if (!t.empty()) {
        Image foil(t.w, t.h, Color::hex(0xf3d77a));
        for (size_t i = 3; i < foil.px.size(); i += 4) foil.px[i] = t.px[i];
        shade::bevel(foil, 0.8f * S, 1.f, shade::mat::gold());
        canvas.draw(foil, (int)(150 * S - t.w / 2.f), (int)(46 * S - t.h / 2.f));
    }
    return canvas;
}

Image chandelier() {
    Rel r(240, 170, svg::radial("cr", 0.4f, 0.3f, 0.7f, {{0, Color::hex(0xffffff)}, {1, Color::hex(0xb9d4e8)}}));
    auto arms = [&](bool front) {
        std::string a, crystals, candles, flames;
        for (int i = 0; i < 8; i++) {
            float th = i * PI / 4 + PI / 8;
            if ((std::sin(th) >= 0) != front) continue;
            float ex = 120 + std::cos(th) * 100, ey = 96 + std::sin(th) * 24;
            float mx = 120 + std::cos(th) * 55, my = 122 + std::sin(th) * 16;
            a += "<path d=\"M120 102 Q" + svg::num(mx) + " " + svg::num(my) + " " + svg::num(ex) + " " + svg::num(ey + 6) + "\" fill=\"none\" " +
                 svg::stroke("#e2b450", 2.8f) + "/>";
            a += svg::ellipse(ex, ey + 6, 7, 2.8f, "#e2b450");
            candles += svg::rect(ex - 1.7f, ey - 6, 3.4f, 11, 1, "#f5efe2");
            flames += svg::ellipse(ex, ey - 8, 2.4f, 3.6f, "#fff3d0");
            for (int k = 0; k < 3; k++) crystals += svg::ellipse(mx, my + 6 + k * 6, 1.8f, 2.7f, "#dff0ff");
        }
        r.add(a, shade::mat::gold(), 1.4f, 1.f);
        r.add(candles, shade::mat::ivory(), 1.2f);
        r.ink(flames);
        r.add(crystals, shade::mat::gloss(), 1.6f, 1.2f);
    };
    r.add(svg::line(120, 0, 120, 34, "#e2b450", 2.6f), shade::mat::gold(), 1.3f);
    r.add(svg::ellipse(120, 34, 16, 5, "#e2b450"), shade::mat::gold(), 2.5f);
    arms(false);
    r.add(svg::path("M108 40 C 100 60, 104 80, 112 92 L128 92 C136 80 140 60 132 40 Z", "#e2b450") + svg::ellipse(120, 100, 30, 9, "#e2b450"),
          shade::mat::gold(), 6, 0.8f);
    std::string cr;
    for (int k = 0; k < 9; k++) {
        float a = PI * (0.15f + 0.7f * k / 8.f);
        cr += svg::ellipse(120 - std::cos(a) * 28, 108 + std::sin(a) * 14, 2, 3, "#dff0ff");
    }
    for (int k = 0; k < 4; k++) cr += svg::ellipse(120, 116 + k * 8, 2.2f, 3.4f, "#dff0ff");
    cr += svg::path("M116 146 L124 146 L120 158 Z", "#dff0ff");
    r.add(cr, shade::mat::gloss(), 1.8f, 1.2f);
    arms(true);
    Image img = r.render();
    shade::glints(img, 7, 5, 8);
    return img;
}

// Station art is costly to shade, so it is built once per session.
struct HallArt {
    Image stations[4];
    Image chand;
};
const HallArt& hallArt() {
    static HallArt a;
    static bool built = false;
    if (!built) {
        built = true;
        a.stations[0] = blackjackStation();
        a.stations[1] = pokerStation();
        a.stations[2] = rouletteStation();
        a.stations[3] = slotStation();
        a.chand = chandelier();
    }
    return a;
}

struct Station {
    SceneId scene;
    const char* title;
    const char* sub;
    float x, baseY;
    Tex tex;
    float anchorY; // texture y (logical) that sits on the floor
    float light = 0;
};

class HallScene : public Scene {
public:
    HallScene() {
        bg_ = gfx::upload(art::cached("hall.background", buildBackground));
        const HallArt& art = hallArt();
        chand_ = gfx::upload(art.chand);
        stations_[0] = {SC_BLACKJACK, "БЛЭКДЖЕК", "Ставки от 10 · до 5 игроков · 3 к 2", 205, 548, Tex(), 220};
        stations_[1] = {SC_POKER, "ТЕХАССКИЙ ХОЛДЕМ", "No-Limit · до 6 мест · боты и друзья", 495, 548, Tex(), 220};
        stations_[2] = {SC_ROULETTE, "РУЛЕТКА", "Европейская · один ноль · все ставки", 785, 548, Tex(), 214};
        stations_[3] = {SC_SLOTS, "ГНЕВ ЗЕВСА", "Слот 6×5 · каскады · множители до ×500", 1075, 548, Tex(), 306};
        for (int i = 0; i < 4; i++) stations_[i].tex = gfx::upload(art.stations[i]);
        Rng r(31337);
        for (int i = 0; i < 46; i++) {
            Bokeh b;
            b.x = r.uniform(0, BG_W);
            b.y = r.uniform(WALL_TOP + 25, WALL_BOT - 6);
            b.r = r.uniform(2.5f, 8.f);
            static const uint32_t cols[] = {0xffc875, 0xffb050, 0xff7a9a, 0x7fc8ff, 0xffe3a0, 0xc890ff};
            b.c = Color::hex(cols[r.range(0, 5)]);
            b.phase = r.uniform(0, 6.28f);
            b.speed = r.uniform(0.6f, 2.2f);
            bokeh_.push_back(b);
        }
        haze_.init(9, -100, 120, SCREEN_W + 200, 380, Color(255, 214, 170), 77);
        for (int i = 0; i < 40; i++) motes_.push_back({r.uniform(0, 1), r.uniform(0, 1), r.uniform(0.2f, 1.f)});
        sel_ = lastSel_;
        static bool tipShown = false;
        if (!tipShown) {
            tipShown = true;
            auto act = save::active();
            bool defaults = act.size() == 1 && save::player(act[0]).name == "Игрок 1";
            if (defaults) ui::toast("Играете компанией? Нажмите X — добавьте игроков и имена", pal::goldLight, 4.5f);
        }
        cam_ = (sel_ - 1.5f) * -18;
        for (int i = 0; i < 4; i++) stations_[i].light = i == sel_ ? 1.f : 0.f;
    }

    void update(float dt) override {
        t_ += dt;
        updateEvents(dt);
        if (dialog_.update(dt)) return;
        if (konami()) return;
        if (input::repeat(ANY_PAD, BTN_LEFT) && sel_ > 0) { sel_--; audio::play(audio::SFX_NAV, 0.9f, -0.4f); }
        if (input::repeat(ANY_PAD, BTN_RIGHT) && sel_ < 3) { sel_++; audio::play(audio::SFX_NAV, 0.9f, 0.4f); }
        if (input::pressed(ANY_PAD, BTN_A)) {
            audio::play(audio::SFX_SELECT);
            lastSel_ = sel_;
            app::go(stations_[sel_].scene);
        } else if (input::pressed(ANY_PAD, BTN_X)) {
            audio::play(audio::SFX_SELECT);
            lastSel_ = sel_;
            app::go(SC_PLAYERS);
        } else if (input::pressed(ANY_PAD, BTN_Y)) {
            audio::play(audio::SFX_SELECT);
            lastSel_ = sel_;
            app::go(SC_SETTINGS);
        } else if (input::pressed(ANY_PAD, BTN_MINUS)) {
            audio::play(audio::SFX_SELECT);
            lastSel_ = sel_;
            app::go(SC_TROPHIES);
        } else if (input::pressed(ANY_PAD, BTN_PLUS | BTN_B)) {
            audio::play(audio::SFX_BACK);
            dialog_.show("ПОКИНУТЬ КАЗИНО?", "Фишки и имена игроков сохранятся на карте памяти.", {"Остаться", "Выйти"},
                         [](int pick) {
                             if (pick == 1) { save::store(); app::quit(); }
                         });
        }
        cam_ = approach(cam_, (sel_ - 1.5f) * -18, 5, dt);
        for (int i = 0; i < 4; i++) stations_[i].light = approach(stations_[i].light, i == sel_ ? 1.f : 0.f, 9, dt);
        haze_.update(dt);
        for (auto& m : motes_) {
            m.y -= dt * 0.02f * m.s;
            m.x += std::sin(t_ * 0.5f + m.s * 10) * dt * 0.01f;
            if (m.y < 0) m.y += 1;
        }
    }

    void render() override {
        float bgx = (SCREEN_W - BG_W) / 2 + cam_ * 0.55f;
        gfx::drawScaled(bg_, bgx, 0, BG_W, SCREEN_H);
        // distant lights through the arches
        for (auto& b : bokeh_) {
            float a = 0.35f + 0.35f * std::sin(t_ * b.speed + b.phase);
            gfx::glow(bgx + b.x, b.y, b.r * 2.6f, b.c, a * 0.55f);
            gfx::circle(bgx + b.x, b.y, b.r * 0.35f, b.c.alpha(a * 0.8f));
        }
        // illuminated sign on the back wall
        float sx = bgx + BG_W / 2, sy = WALL_TOP + 34;
        float pulse = 0.85f + 0.15f * std::sin(t_ * 1.3f);
        gfx::glowEllipse(sx, sy, 250, 48, Color::hex(0xffc35a), 0.22f * pulse);
        gfx::textGold("GRAND CASINO", sx, sy, F_TITLE, 46, 0, 1.f, true);
        gfx::text("NX", sx + 196, sy - 14, F_TITLE, 16, pal::goldLight, 0, 0.85f);
        // chandeliers
        float chx = cam_ * 0.75f;
        drawChandelier(300 + chx, 18, 0.72f, 0.2f);
        drawChandelier(980 + chx, 18, 0.72f, 1.3f);
        drawChandelier(640 + chx, -6, 1.0f, 2.7f);
        drawJackpot(bgx + BG_W / 2, WALL_TOP + 84);
        drawEvents(bgx);
        // smoky air under the chandeliers
        haze_.render(1.f);
        // stations
        for (int i = 0; i < 4; i++) drawStation(i);
        // bottom bar
        gfx::rectGrad(0, 610, SCREEN_W, 110, Color(0, 0, 0, 0), Color(0, 0, 0, 240));
        drawPlayers();
        ui::hints({{BTN_A, "Играть"}, {BTN_X, "Игроки"}, {BTN_MINUS, "Трофеи"}, {BTN_Y, "Настройки"}, {BTN_PLUS, "Выход"}}, 1262, 700);
        dialog_.render();
    }

private:
    struct Bokeh { float x, y, r; Color c; float phase, speed; };

    // ---------------------------------------------------------------- jackpot board
    void drawJackpot(float cx, float cy) {
        float w = 258, h = 50;
        float pulse = 0.5f + 0.5f * std::sin(t_ * 2.4f);
        gfx::glowEllipse(cx, cy, w * 0.7f, h * 1.2f, Color::hex(0xff5a2a), 0.12f + 0.06f * pulse);
        gfx::roundRect(cx - w / 2, cy - h / 2, w, h, 8, Color(10, 6, 6, 235));
        gfx::roundRectOutline(cx - w / 2, cy - h / 2, w, h, 8, 1.4f, pal::gold.alpha(0.9f));
        // marquee bulbs chasing around the frame
        int bulbs = 26;
        for (int k = 0; k < bulbs; k++) {
            float u = k / (float)bulbs;
            float px, py;
            float per = 2 * (w + h), d = u * per;
            if (d < w) { px = cx - w / 2 + d; py = cy - h / 2; }
            else if (d < w + h) { px = cx + w / 2; py = cy - h / 2 + (d - w); }
            else if (d < 2 * w + h) { px = cx + w / 2 - (d - w - h); py = cy + h / 2; }
            else { px = cx - w / 2; py = cy + h / 2 - (d - 2 * w - h); }
            bool on = ((k + (int)(t_ * 10)) % 3) == 0;
            gfx::circle(px, py, 1.8f, on ? Color::hex(0xffe9a8) : Color::hex(0x5a4020));
            if (on) gfx::glow(px, py, 7, Color::hex(0xffc860), 0.6f);
        }
        jpShown_ = jpShown_ <= 0 ? (float)jackpot::value() : approach(jpShown_, (float)jackpot::value(), 3, 1 / 60.f);
        gfx::text("ПРОГРЕССИВНЫЙ ДЖЕКПОТ", cx, cy - 13, F_SANS_BOLD, 11, Color::hex(0xffb070), 0, 0.9f);
        std::string v = fmtMoney((i64)jpShown_);
        gfx::textGlow(v, cx, cy + 8, F_NUM, 26, Color::hex(0xff7a30), 0.5f + 0.2f * pulse, 0);
        gfx::text(v, cx, cy + 8, F_NUM, 26, Color::hex(0xffd890), 0);
    }

    // ---------------------------------------------------------------- living hall
    struct Waiter { float x = -100, dir = 1, t = 0; bool active = false, clinked = false; };

    void updateEvents(float dt) {
        // the pool creeps up as if other guests were playing
        jpTick_ -= dt;
        if (jpTick_ <= 0) {
            jpTick_ = rng().uniform(0.4f, 1.2f);
            save::data().jackpot = jackpot::value() + rng().range(1, 9);
        }
        waiterTimer_ -= dt;
        if (!waiter_.active && waiterTimer_ <= 0) {
            waiter_.active = true;
            waiter_.dir = rng().chance(0.5) ? 1.f : -1.f;
            waiter_.x = waiter_.dir > 0 ? -60.f : BG_W + 60.f;
            waiter_.t = 0;
            waiter_.clinked = false;
        }
        if (waiter_.active) {
            waiter_.t += dt;
            waiter_.x += waiter_.dir * 62 * dt;
            if (!waiter_.clinked && std::fabs(waiter_.x - BG_W / 2) < 40) {
                waiter_.clinked = true;
                audio::play(audio::SFX_GLASS, 0.22f, (waiter_.x - BG_W / 2) / 700.f);
            }
            if (waiter_.x < -80 || waiter_.x > BG_W + 80) {
                waiter_.active = false;
                waiterTimer_ = rng().uniform(22, 40);
            }
        }
        distantTimer_ -= dt;
        if (distantTimer_ <= 0) {
            distantTimer_ = rng().uniform(45, 80);
            distantT_ = 0;
            distantX_ = rng().chance(0.5) ? rng().uniform(80, 360) : rng().uniform(960, 1240);
            float pan = (distantX_ - BG_W / 2) / 700.f;
            audio::play(audio::SFX_SPARKLE, 0.18f, pan);
            audio::play(audio::SFX_CHEER, 0.12f, pan);
            static const char* lines[] = {"Где-то в зале сорвали куш!", "За дальним столом аплодируют победителю",
                                          "Кому-то сегодня везёт — может, и вам?"};
            if (rng().chance(0.6)) ui::toast(lines[rng().range(0, 2)], pal::cream, 2.6f);
        }
        if (distantT_ < 3) distantT_ += dt;
    }

    void drawEvents(float bgx) {
        // light celebrating a distant win through the arches
        if (distantT_ < 3) {
            float a = std::sin(distantT_ / 3 * PI) * (0.6f + 0.4f * std::sin(distantT_ * 18));
            gfx::glowEllipse(bgx + distantX_, WALL_TOP + 90, 90, 60, Color::hex(0xffd070), 0.35f * a);
        }
        if (!waiter_.active) return;
        // a waiter in black tie carrying champagne across the back of the hall
        float x = bgx + waiter_.x, y = WALL_BOT + 18;
        float step = waiter_.t * 7.5f;
        float bob = std::fabs(std::sin(step)) * 1.6f;
        Color suit(14, 12, 16), shirt(205, 200, 190), skin(70, 52, 44);
        float sw = std::sin(step) * 7;
        gfx::glowEllipse(x, y - 30, 26, 44, Color::hex(0xffc890), 0.08f);
        gfx::ellipse(x, y + 1, 14, 3, Color(0, 0, 0, 120));
        gfx::line(x - 2, y - 30 - bob, x - 2 + sw, y, 5, suit);
        gfx::line(x + 2, y - 30 - bob, x + 2 - sw, y, 5, suit);
        float sy = y - 58 - bob, hy = y - 30 - bob;
        gfx::quad({x - 9, sy}, {x + 9, sy}, {x + 7, hy}, {x - 7, hy}, suit, suit, suit, suit);
        gfx::triangle({x - 3.5f, sy}, {x + 3.5f, sy}, {x, sy + 12}, shirt, shirt, shirt);
        gfx::rect(x - 3, sy + 1, 6, 2, Color(10, 8, 10));
        // free arm swinging, tray arm raised
        float d = waiter_.dir;
        gfx::line(x - d * 8, sy + 3, x - d * 9 + std::sin(step + PI) * 4, hy + 2, 4, suit);
        gfx::line(x + d * 8, sy + 3, x + d * 16, sy - 4, 4, suit);
        gfx::ellipse(x + d * 17, sy - 6, 12, 2.2f, Color(190, 190, 200));
        for (int k = -1; k <= 1; k++) {
            float gx = x + d * 17 + k * 6;
            gfx::line(gx, sy - 7, gx, sy - 11, 1, Color(230, 230, 240, 200));
            gfx::quad({gx - 2, sy - 18}, {gx + 2, sy - 18}, {gx + 1, sy - 11}, {gx - 1, sy - 11}, Color(255, 220, 140, 220),
                      Color(255, 220, 140, 220), Color(240, 190, 90, 220), Color(240, 190, 90, 220));
        }
        gfx::circle(x, sy - 8, 6, skin);
        gfx::ellipse(x - d * 0.5f, sy - 12, 6, 3, Color(20, 16, 14));
    }

    // ---------------------------------------------------------------- secret
    // Up, up, down, down, left, right, left, right, B, A.
    bool konami() {
        static const u32 seq[] = {BTN_UP, BTN_UP, BTN_DOWN, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_LEFT, BTN_RIGHT, BTN_B, BTN_A};
        u32 p = 0;
        for (u32 b : {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_B, BTN_A})
            if (input::pressed(ANY_PAD, b)) p = b;
        if (!p) return false;
        if (p == seq[konami_]) {
            konami_++;
            if (konami_ == 10) {
                konami_ = 0;
                secret();
                return true;
            }
            return konami_ == 9; // swallow the B so it does not open the exit dialog
        }
        konami_ = p == BTN_UP ? 1 : 0;
        return false;
    }

    void secret() {
        static bool claimed = false;
        audio::play(audio::SFX_SPARKLE, 0.8f);
        if (claimed) {
            ui::toast("Казино вас помнит: бонус уже выдан сегодня", pal::goldLight, 3);
            fx::burst(640, 300, 40, Color::hex(0xffd060));
            return;
        }
        claimed = true;
        audio::play(audio::SFX_HIT, 0.8f);
        audio::play(audio::SFX_CHEER, 0.6f);
        fx::coins(110);
        fx::confetti(140);
        fx::shake(0.5f);
        fx::flash(Color::hex(0xfff0c0), 0.5f, 0.5f);
        for (int idx : save::active()) {
            save::player(idx).balance += 7777;
            trophy::unlock(idx, trophy::SECRET);
            trophy::checkBalance(idx);
        }
        save::store();
        ui::toast("Секретный бонус заведения: +7 777 фишек каждому!", pal::goldLight, 4);
    }
    struct Mote { float x, y, s; };

    void drawChandelier(float x, float y, float scale, float phase) {
        float cx = x, w = chand_.w * scale, h = chand_.h * scale;
        gfx::glow(cx, y + h * 0.55f, 210 * scale, Color::hex(0xffb860), 0.32f);
        gfx::drawScaled(chand_, cx - w / 2, y, w, h);
        for (int i = 0; i < 8; i++) {
            float th = i * PI / 4 + PI / 8;
            float ex = cx + std::cos(th) * 100 * scale, ey = y + (96 + std::sin(th) * 24 - 8) * scale;
            float fl = 0.8f + 0.2f * std::sin(t_ * 9 + i * 1.7f + phase);
            gfx::glow(ex, ey, 26 * scale, Color::hex(0xffd08a), 0.55f * fl);
        }
        // crystal glints
        for (int k = 0; k < 3; k++) {
            float tt = t_ * 0.9f + phase + k * 2.1f;
            float a = std::max(0.f, std::sin(tt * 2.3f)) * 0.9f;
            float gx = cx + std::sin(tt * 0.7f + k) * 30 * scale, gy = y + (110 + k * 12) * scale;
            gfx::glow(gx, gy, 9 * scale, pal::white, a);
        }
    }

    void drawStation(int i) {
        Station& st = stations_[i];
        float L = st.light;
        float x = st.x + cam_;
        float scale = 1.0f + 0.04f * L;
        float w = st.tex.w * scale, h = st.tex.h * scale;
        float top = st.baseY - st.anchorY * scale;
        bool slot = st.scene == SC_SLOTS;
        // pool of light on the carpet
        gfx::glowEllipse(x, st.baseY - 8, 190, 46, Color::hex(0xffcf8a), 0.10f + 0.22f * L);
        if (!slot) {
            // pendant lamp and its cone of light
            float ly = top + 10;
            Color warm = Color::hex(0xffd6a0);
            gfx::line(x, 0, x, ly - 24, 1.2f, Color(30, 22, 18));
            gfx::quad({x - 70, st.baseY - 70}, {x + 70, st.baseY - 70}, {x + 16, ly - 10}, {x - 16, ly - 10},
                      warm.alpha(0.0f), warm.alpha(0.0f), warm.alpha(0.10f + 0.12f * L), warm.alpha(0.10f + 0.12f * L), true);
            gfx::quad({x - 20, ly - 24}, {x + 20, ly - 24}, {x + 34, ly - 6}, {x - 34, ly - 6}, Color::hex(0x1d3a2a),
                      Color::hex(0x1d3a2a), Color::hex(0x0b1a12), Color::hex(0x0b1a12));
            gfx::rect(x - 34, ly - 7, 68, 2, Color::hex(0xb08a3a));
            gfx::glowEllipse(x, ly - 2, 46, 12, warm, 0.6f + 0.3f * L);
            // dust in the light
            for (size_t k = 0; k < motes_.size(); k += 4) {
                const Mote& m = motes_[(k + i) % motes_.size()];
                float my = lerp(ly, st.baseY - 80, m.y);
                float spread = lerp(16, 70, m.y);
                float mx = x + (m.x - 0.5f) * 2 * spread;
                gfx::circle(mx, my, 0.9f, warm.alpha(0.25f * L * m.s));
            }
        } else {
            gfx::glowEllipse(x, top + h * 0.45f, 150, 170, Color::hex(0x3f8fff), 0.12f + 0.25f * L);
        }
        float b = 0.5f + 0.5f * L;
        gfx::drawScaled(st.tex, x - w / 2, top, w, h, 1.f, pal::white.scaled(b));
        if (slot) {
            float flick = 0.6f + 0.4f * std::sin(t_ * 6.f);
            gfx::glowEllipse(x - 64 * scale, top + 160 * scale, 10, 130 * scale, Color::hex(0x3fa9ff), (0.25f + 0.45f * L) * flick);
            gfx::glowEllipse(x + 64 * scale, top + 160 * scale, 10, 130 * scale, Color::hex(0x3fa9ff), (0.25f + 0.45f * L) * flick);
            gfx::glowEllipse(x, top + 120 * scale, 70, 60, Color::hex(0x6f7bff), 0.18f * L);
        }
        // name plate
        float ny = st.baseY + 22;
        float a = 0.55f + 0.45f * L;
        if (L > 0.02f) {
            float pw = std::max(gfx::textWidth(st.title, F_TITLE, 24), 230.f) + 40;
            gfx::roundRect(x - pw / 2, ny - 20, pw, 64, 8, Color(8, 6, 8, (uint8_t)(200 * L)));
            gfx::roundRectOutline(x - pw / 2, ny - 20, pw, 64, 8, 1.2f, pal::gold.alpha(0.8f * L));
        }
        gfx::textGold(st.title, x, ny, F_TITLE, 20 + 4 * L, 0, a);
        gfx::text(st.sub, x, ny + 25, F_SANS, 15, pal::cream, 0, L);
    }

    void drawPlayers() {
        auto act = save::active();
        float x = 20, y = 652;
        gfx::text("ЗА СТОЛАМИ", x, y - 9, F_SANS_BOLD, 13, pal::muted, -1);
        for (size_t k = 0; k < act.size(); k++) {
            float px = x + k * 152;
            if (px > 700) break;
            ui::playerTag(act[k], px, y, 144, false, 1.f);
            if (input::connectedCount() > 1) gfx::text(strf("%d", (int)k + 1), px + 134, y + 10, F_NUM, 12, pal::muted, 1);
        }
    }

    Tex bg_, chand_;
    fx::Haze haze_;
    Waiter waiter_;
    float waiterTimer_ = 6, distantTimer_ = 25, distantT_ = 9, distantX_ = 0, jpTick_ = 1, jpShown_ = 0;
    int konami_ = 0;
    Station stations_[4];
    std::vector<Bokeh> bokeh_;
    std::vector<Mote> motes_;
    int sel_ = 0;
    float cam_ = 0, t_ = 0;
    ui::Dialog dialog_;
    static int lastSel_;
};

int HallScene::lastSel_ = 0;

} // namespace

std::unique_ptr<Scene> makeHallScene() { return std::make_unique<HallScene>(); }
