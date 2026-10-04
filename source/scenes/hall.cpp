// Main menu: a dimly lit casino hall with four gaming stations.
#include "../art/art.h"
#include "../art/svg.h"
#include "../core/app.h"
#include "../core/audio.h"
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/parallel.h"
#include "../core/platform.h"
#include "../core/save.h"
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
// Station illustrations
// ---------------------------------------------------------------------------
std::string goldGrad(const std::string& id) {
    return svg::linear(id, 0, 0, 0, 1,
                       {{0, Color::hex(0xf7e19c)}, {0.45f, Color::hex(0xd1a646)}, {0.6f, Color::hex(0x9c7428)},
                        {1, Color::hex(0xdcb75c)}});
}

std::string miniChips(float x, float y, int n, uint32_t seed) {
    static const char* cols[] = {"#b3202e", "#1f5fae", "#232327", "#1f8a52", "#5b2c86", "#d9a62b"};
    std::string s;
    Rng r(seed);
    std::string col = cols[r.range(0, 5)];
    for (int k = 0; k < n; k++) {
        if (r.chance(0.3)) col = cols[r.range(0, 5)];
        float yy = y - k * 1.9f;
        s += svg::ellipse(x, yy + 1, 7, 3.2f, "#000", "fill-opacity=\"0.4\"");
        s += svg::ellipse(x, yy, 7, 3.2f, col, svg::stroke("#ffffff", 0.6f, 0.55f) + " stroke-dasharray=\"1.5 2\"");
    }
    return s;
}

std::string miniCard(float x, float y, float rot, bool red) {
    std::string g = "<g transform=\"translate(" + svg::num(x) + " " + svg::num(y) + ") rotate(" + svg::num(rot) +
                    ") scale(1 0.6)\">";
    g += svg::rect(-5, -7, 10, 14, 1.2f, "#f6f1e6", svg::stroke("#9c927e", 0.4f));
    g += svg::circle(0, 0, 2.2f, red ? "#b0142c" : "#16151b");
    return g + "</g>";
}

std::string blackjackStation() {
    std::string s = svg::open(300, 230) + "<defs>" + goldGrad("g") +
                    svg::radialU("felt", 150, 130, 140, {{0, Color::hex(0x23845a)}, {1, Color::hex(0x0c3f27)}}) +
                    svg::linear("leather", 0, 0, 0, 1, {{0, Color::hex(0x3a2418)}, {1, Color::hex(0x120906)}}) +
                    svg::linear("skirt", 0, 0, 0, 1, {{0, Color::hex(0x2a1810)}, {1, Color::hex(0x0d0705)}}) +
                    svg::linear("ped", 0, 0, 1, 0, {{0, Color::hex(0x0e0807)}, {0.5f, Color::hex(0x2a1912)}, {1, Color::hex(0x0e0807)}}) +
                    "</defs>";
    s += svg::ellipse(150, 218, 60, 9, "#000", "fill-opacity=\"0.6\"");
    s += svg::rect(126, 170, 48, 48, 3, svg::url("ped"));
    s += svg::path("M10 120 A140 70 0 0 0 290 120 L290 138 A140 70 0 0 1 10 138 Z", svg::url("skirt"));
    s += svg::path("M10 120 A140 70 0 0 0 290 120 L274 120 A124 59 0 0 1 26 120 Z", svg::url("leather"),
                   svg::stroke("#6a4630", 0.8f, 0.8f));
    s += svg::path("M26 120 A124 59 0 0 0 274 120 Z", svg::url("felt"));
    // dealer edge, tray, shoe
    s += svg::rect(20, 114, 260, 7, 2, "#2a1810", svg::stroke("#7a5a28", 0.6f));
    s += svg::rect(112, 104, 76, 13, 2, "#120b08", svg::stroke("#8a6a30", 0.7f));
    static const char* tray[] = {"#b3202e", "#1f5fae", "#232327", "#1f8a52", "#5b2c86", "#d9a62b", "#b3202e", "#232327"};
    for (int i = 0; i < 8; i++) s += svg::rect(116 + i * 8.8f, 106, 7, 9, 1.5f, tray[i], svg::stroke("#fff", 0.4f, 0.4f));
    s += svg::path("M224 104 L250 104 L254 118 L222 118 Z", "#1a1a1e", svg::stroke("#6a6a70", 0.5f));
    s += svg::rect(228, 106, 14, 9, 1, "#f2ecdf");
    // printed arcs
    s += svg::path("M58 121 A92 42 0 0 0 242 121", "none", svg::stroke("#e4c56a", 1.1f, 0.55f));
    s += svg::path("M72 121 A78 33 0 0 0 228 121", "none", svg::stroke("#e4c56a", 0.7f, 0.45f));
    for (int i = 0; i < 5; i++) {
        float a = (155 - i * 32.5f) * PI / 180;
        float x = 150 + std::cos(a) * 104, y = 120 + std::sin(a) * 50;
        s += svg::ellipse(x, y, 10, 4.6f, "none", svg::stroke("#e4c56a", 1.f, 0.75f));
        if (i != 2) s += miniChips(x, y, 2 + i % 3, 40 + i);
        if (i % 2 == 0) {
            s += miniCard(x - 5, y - 9, -8, i == 0);
            s += miniCard(x + 2, y - 10, 6, false);
        }
    }
    s += miniCard(142, 111, 0, true);
    s += miniCard(155, 111, 0, false);
    s += svg::close();
    return s;
}

std::string pokerStation() {
    std::string s = svg::open(300, 230) + "<defs>" + goldGrad("g") +
                    svg::radialU("felt", 150, 122, 120, {{0, Color::hex(0x1b6f86)}, {1, Color::hex(0x0a3240)}}) +
                    svg::linear("leather", 0, 0, 0, 1, {{0, Color::hex(0x2b2422)}, {1, Color::hex(0x080606)}}) +
                    svg::linear("wood", 0, 0, 0, 1, {{0, Color::hex(0x7a4c26)}, {1, Color::hex(0x3a210f)}}) +
                    svg::linear("chair", 0, 0, 0, 1, {{0, Color::hex(0x5a1520)}, {1, Color::hex(0x2a070d)}}) +
                    svg::linear("skirt", 0, 0, 0, 1, {{0, Color::hex(0x221410)}, {1, Color::hex(0x0b0605)}}) +
                    svg::linear("ped", 0, 0, 1, 0, {{0, Color::hex(0x0e0807)}, {0.5f, Color::hex(0x2a1912)}, {1, Color::hex(0x0e0807)}}) +
                    "</defs>";
    // chairs behind the table
    for (float x : {82.f, 150.f, 218.f}) {
        s += svg::rect(x - 17, 50, 34, 44, 9, svg::url("chair"), svg::stroke("#8a3a40", 0.6f, 0.6f));
        s += svg::rect(x - 12, 55, 24, 6, 3, "#ffffff", "fill-opacity=\"0.06\"");
    }
    s += svg::ellipse(150, 218, 64, 10, "#000", "fill-opacity=\"0.6\"");
    s += svg::rect(128, 160, 44, 58, 3, svg::url("ped"));
    s += svg::path("M8 125 A142 62 0 0 0 292 125 L292 140 A142 62 0 0 1 8 140 Z", svg::url("skirt"));
    s += svg::ellipse(150, 125, 142, 62, svg::url("leather"), svg::stroke("#5a4a44", 0.8f, 0.7f));
    s += svg::ellipse(150, 125, 125, 53, svg::url("wood"));
    s += svg::ellipse(150, 125, 117, 48, svg::url("felt"));
    s += svg::ellipse(150, 125, 88, 34, "none", svg::stroke("#ffffff", 0.6f, 0.22f));
    // community cards
    for (int i = 0; i < 5; i++) s += miniCard(126 + i * 12, 120, 0, i == 1 || i == 3);
    s += miniChips(150, 142, 5, 7);
    s += miniChips(160, 140, 3, 9);
    // stacks in front of seats
    for (int i = 0; i < 6; i++) {
        float a = (i * 60 + 30) * PI / 180;
        float x = 150 + std::cos(a) * 92, y = 125 + std::sin(a) * 38;
        s += miniChips(x, y, 3 + (i * 7) % 4, 100 + i);
    }
    s += svg::ellipse(196, 132, 5, 2.4f, "#f4efe4", svg::stroke("#333", 0.4f));
    // chairs in front corners
    for (float x : {40.f, 260.f}) {
        s += svg::rect(x - 18, 152, 36, 46, 10, svg::url("chair"), svg::stroke("#8a3a40", 0.6f, 0.6f));
        s += svg::rect(x - 13, 157, 26, 6, 3, "#ffffff", "fill-opacity=\"0.07\"");
    }
    s += svg::close();
    return s;
}

std::string rouletteStation() {
    std::string s = svg::open(300, 230) + "<defs>" + goldGrad("g") +
                    svg::linear("felt", 0, 0, 0, 1, {{0, Color::hex(0x1a6d45)}, {1, Color::hex(0x0d4429)}}) +
                    svg::linear("wood", 0, 0, 0, 1, {{0, Color::hex(0x5e3a1d)}, {1, Color::hex(0x2a170b)}}) +
                    svg::radialU("bowl", 0, 0, 52, {{0.6f, Color::hex(0x6b3b1c)}, {1, Color::hex(0x2a140a)}}) +
                    svg::radialU("cone", 0, 0, 28, {{0, Color::hex(0xa06a35)}, {1, Color::hex(0x4a2810)}}) +
                    "</defs>";
    s += svg::ellipse(155, 214, 120, 14, "#000", "fill-opacity=\"0.55\"");
    // legs / skirt
    s += svg::path("M22 176 L288 176 L284 196 L26 196 Z", "#160c07");
    s += svg::rect(40, 196, 10, 18, 2, "#120906");
    s += svg::rect(260, 196, 10, 18, 2, "#120906");
    // table top
    s += svg::path("M64 66 L268 66 L294 178 L16 178 Z", svg::url("wood"), svg::stroke("#8a6430", 0.8f));
    s += svg::path("M74 73 L260 73 L282 170 L28 170 Z", svg::url("felt"));
    // layout grid in perspective (bilinear across a quad)
    float qx[4] = {128, 252, 272, 140}, qy[4] = {82, 82, 160, 160};
    auto P = [&](float u, float v, float& x, float& y) {
        float tx = lerp(qx[0], qx[1], u), bx = lerp(qx[3], qx[2], u);
        x = lerp(tx, bx, v);
        y = lerp(qy[0], qy[3], v);
    };
    static const int reds[] = {1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36};
    for (int col = 0; col < 12; col++)
        for (int row = 0; row < 3; row++) {
            int num = col * 3 + (3 - row);
            bool red = std::find(std::begin(reds), std::end(reds), num) != std::end(reds);
            float x0, y0, x1, y1, x2, y2, x3, y3;
            float u0 = col / 12.f, u1 = (col + 1) / 12.f, v0 = row / 4.f, v1 = (row + 1) / 4.f;
            P(u0, v0, x0, y0); P(u1, v0, x1, y1); P(u1, v1, x2, y2); P(u0, v1, x3, y3);
            std::string d = "M" + svg::num(x0) + " " + svg::num(y0) + " L" + svg::num(x1) + " " + svg::num(y1) + " L" +
                            svg::num(x2) + " " + svg::num(y2) + " L" + svg::num(x3) + " " + svg::num(y3) + " Z";
            s += svg::path(d, red ? "#a3182a" : "#141216", svg::stroke("#e9dcb6", 0.5f, 0.8f) + " fill-opacity=\"0.85\"");
        }
    for (int k = 0; k < 3; k++) { // dozens row
        float x0, y0, x1, y1, x2, y2, x3, y3;
        P(k / 3.f, 0.75f, x0, y0); P((k + 1) / 3.f, 0.75f, x1, y1); P((k + 1) / 3.f, 1, x2, y2); P(k / 3.f, 1, x3, y3);
        std::string d = "M" + svg::num(x0) + " " + svg::num(y0) + " L" + svg::num(x1) + " " + svg::num(y1) + " L" +
                        svg::num(x2) + " " + svg::num(y2) + " L" + svg::num(x3) + " " + svg::num(y3) + " Z";
        s += svg::path(d, "none", svg::stroke("#e9dcb6", 0.5f, 0.8f));
    }
    {
        float x0, y0, x3, y3;
        P(0, 0, x0, y0); P(0, 0.75f, x3, y3);
        s += svg::path("M" + svg::num(x0) + " " + svg::num(y0) + " L" + svg::num(x3) + " " + svg::num(y3) + " L" +
                           svg::num(x3 - 13) + " " + svg::num((y0 + y3) / 2) + " Z",
                       "#1d7a4c", svg::stroke("#e9dcb6", 0.5f, 0.8f));
    }
    s += miniChips(170, 110, 3, 21);
    s += miniChips(214, 128, 2, 22);
    s += miniChips(236, 100, 4, 23);
    // wheel in perspective
    s += "<g transform=\"translate(80 112) scale(1 0.5)\">";
    s += svg::circle(0, 0, 54, svg::url("bowl"), svg::stroke("#9a7034", 1.4f));
    s += svg::circle(0, 0, 44, "#3a2212", svg::stroke("#b8924a", 1.f));
    for (int i = 0; i < 37; i++) {
        float a0 = i * 2 * PI / 37, a1 = (i + 1) * 2 * PI / 37;
        const char* col = i == 0 ? "#1d7a4c" : (i % 2 ? "#a3182a" : "#141216");
        s += svg::path(svg::sector(0, 0, 28, 38, a0, a1), col);
    }
    s += svg::circle(0, 0, 28, svg::url("cone"), svg::stroke("#d9b45c", 1.2f));
    s += svg::circle(0, 0, 6, svg::url("g"));
    s += svg::rect(-2, -20, 4, 40, 2, svg::url("g"));
    s += svg::rect(-20, -2, 40, 4, 2, svg::url("g"));
    s += svg::circle(30, -26, 3.5f, "#fbfbf7");
    s += "</g>";
    s += svg::close();
    return s;
}

std::string slotMachine(bool lit) {
    std::string s = svg::open(150, 300) + "<defs>" + goldGrad("g") +
                    svg::linear("cab", 0, 0, 1, 0, {{0, Color::hex(0x0d0b10)}, {0.5f, Color::hex(0x2a2430)}, {1, Color::hex(0x0d0b10)}}) +
                    svg::linear("scr", 0, 0, 0, 1, {{0, Color::hex(0x1b2a6b)}, {1, Color::hex(0x0b0f2c)}}) +
                    svg::linear("top", 0, 0, 0, 1, {{0, Color::hex(0x1b2d7a)}, {1, Color::hex(0x0a1236)}}) +
                    "</defs>";
    s += svg::rect(20, 182, 110, 112, 4, svg::url("cab"), svg::stroke("#9a7a3a", 0.8f, 0.8f));
    s += svg::rect(16, 288, 118, 10, 2, "#0a080b");
    s += svg::path("M14 168 L136 168 L142 186 L8 186 Z", "#24202a", svg::stroke("#8a6a30", 0.7f));
    static const char* btn[] = {"#d23a3a", "#e3b23c", "#3aa0e3", "#3ad27a", "#e3e3e3"};
    for (int i = 0; i < 5; i++) s += svg::rect(22 + i * 22, 173, 16, 7, 2.5f, btn[i], lit ? "" : "fill-opacity=\"0.5\"");
    s += svg::rect(12, 58, 126, 112, 7, svg::url("g"));
    s += svg::rect(19, 65, 112, 98, 4, svg::url("scr"));
    static const char* gem[] = {"#e0344a", "#a248e0", "#3fcf74", "#3f8ff0", "#f0c53f", "#e0344a"};
    for (int c = 0; c < 6; c++)
        for (int r = 0; r < 5; r++) {
            float x = 28 + c * 18.8f, y = 74 + r * 19;
            const char* col = gem[(c * 7 + r * 3) % 6];
            if ((c + r * 2) % 7 == 3) s += svg::circle(x, y, 7, "#7fd6ff", "fill-opacity=\"0.9\"");
            else s += svg::path("M" + svg::num(x) + " " + svg::num(y - 7) + " L" + svg::num(x + 6) + " " + svg::num(y) +
                                    " L" + svg::num(x) + " " + svg::num(y + 7) + " L" + svg::num(x - 6) + " " + svg::num(y) + " Z",
                                col);
        }
    // topper
    s += svg::path("M12 60 L12 32 Q75 -8 138 32 L138 60 Z", svg::url("g"));
    s += svg::path("M19 56 L19 36 Q75 2 131 36 L131 56 Z", svg::url("top"));
    for (float bx : {36.f, 114.f}) {
        std::string d = "M" + svg::num(bx + 4) + " 22 L" + svg::num(bx - 7) + " 38 L" + svg::num(bx) + " 38 L" +
                        svg::num(bx - 5) + " 53 L" + svg::num(bx + 9) + " 33 L" + svg::num(bx + 2) + " 33 L" +
                        svg::num(bx + 8) + " 22 Z";
        s += svg::path(d, svg::url("g"), svg::stroke("#fff4c0", 0.5f));
    }
    s += svg::rect(4, 30, 5, 264, 2, lit ? "#3fa9ff" : "#1a2a4a");
    s += svg::rect(141, 30, 5, 264, 2, lit ? "#3fa9ff" : "#1a2a4a");
    s += svg::close();
    return s;
}

Image slotStation() {
    const float S = TEX_SCALE;
    Image canvas((int)(300 * S), (int)(310 * S));
    for (int side = 0; side < 2; side++) {
        Image back = gfx::rasterSvg(slotMachine(false), S * 0.78f);
        for (size_t i = 0; i < back.px.size(); i += 4) {
            back.px[i] = (uint8_t)(back.px[i] * 0.45f);
            back.px[i + 1] = (uint8_t)(back.px[i + 1] * 0.45f);
            back.px[i + 2] = (uint8_t)(back.px[i + 2] * 0.5f);
        }
        int x = side == 0 ? (int)(10 * S) : (int)(173 * S);
        canvas.draw(back, x, (int)(52 * S));
    }
    Image main = gfx::rasterSvg(slotMachine(true), S);
    canvas.draw(main, (int)(75 * S), (int)(8 * S));
    // "ZEUS" lettering on the topper
    Image t = gfx::textMask("ZEUS", F_TITLE, 16, S);
    if (!t.empty()) {
        Image gold = t;
        canvas.drawTinted(gold, (int)(150 * S - t.w / 2.f), (int)(46 * S - t.h / 2.f), Color::hex(0xf3d77a));
    }
    return canvas;
}

std::string chandelier() {
    std::string s = svg::open(240, 170) + "<defs>" + goldGrad("g") +
                    svg::radial("cr", 0.4f, 0.3f, 0.7f, {{0, Color::hex(0xffffff)}, {1, Color::hex(0xb9d4e8)}}) + "</defs>";
    s += svg::line(120, 0, 120, 34, "#b08a3a", 2.4f);
    s += svg::ellipse(120, 34, 16, 5, svg::url("g"));
    auto arm = [&](float th) {
        float ex = 120 + std::cos(th) * 100, ey = 96 + std::sin(th) * 24;
        float mx = 120 + std::cos(th) * 55, my = 122 + std::sin(th) * 16;
        std::string a = "<path d=\"M120 102 Q" + svg::num(mx) + " " + svg::num(my) + " " + svg::num(ex) + " " +
                        svg::num(ey + 6) + "\" fill=\"none\" " + svg::stroke("#c9a24a", 2.6f) + "/>";
        a += svg::ellipse(ex, ey + 6, 7, 2.6f, svg::url("g"));
        a += svg::rect(ex - 1.6f, ey - 6, 3.2f, 11, 1, "#f5efe2");
        a += svg::ellipse(ex, ey - 8, 2.4f, 3.4f, "#fff3d0");
        // hanging crystal drops
        for (int k = 0; k < 3; k++) a += svg::ellipse(mx, my + 6 + k * 6, 1.7f, 2.6f, svg::url("cr"), "fill-opacity=\"0.9\"");
        return a;
    };
    for (int i = 0; i < 8; i++) {
        float th = i * PI / 4 + PI / 8;
        if (std::sin(th) < 0) s += arm(th);
    }
    s += svg::path("M108 40 C 100 60, 104 80, 112 92 L128 92 C136 80 140 60 132 40 Z", svg::url("g"));
    s += svg::ellipse(120, 100, 30, 9, svg::url("g"));
    // crystal swag under the bowl
    for (int k = 0; k < 9; k++) {
        float a = PI * (0.15f + 0.7f * k / 8.f);
        s += svg::ellipse(120 - std::cos(a) * 28, 108 + std::sin(a) * 14, 2, 3, svg::url("cr"));
    }
    for (int k = 0; k < 4; k++) s += svg::ellipse(120, 116 + k * 8, 2.2f, 3.4f, svg::url("cr"));
    s += svg::path("M116 146 L124 146 L120 158 Z", svg::url("cr"));
    for (int i = 0; i < 8; i++) {
        float th = i * PI / 4 + PI / 8;
        if (std::sin(th) >= 0) s += arm(th);
    }
    s += svg::close();
    return s;
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
        bg_ = gfx::upload(buildBackground());
        chand_ = gfx::svgTex(chandelier());
        stations_[0] = {SC_BLACKJACK, "БЛЭКДЖЕК", "Ставки от 10 · до 5 игроков · 3 к 2", 205, 548, Tex(), 220};
        stations_[1] = {SC_POKER, "ТЕХАССКИЙ ХОЛДЕМ", "No-Limit · до 6 мест · боты и друзья", 495, 548, Tex(), 220};
        stations_[2] = {SC_ROULETTE, "РУЛЕТКА", "Европейская · один ноль · все ставки", 785, 548, Tex(), 214};
        stations_[3] = {SC_SLOTS, "ГНЕВ ЗЕВСА", "Слот 6×5 · каскады · множители до ×500", 1075, 548, Tex(), 306};
        stations_[0].tex = gfx::svgTex(blackjackStation());
        stations_[1].tex = gfx::svgTex(pokerStation());
        stations_[2].tex = gfx::svgTex(rouletteStation());
        stations_[3].tex = gfx::upload(slotStation());
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
        if (dialog_.update(dt)) return;
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
        } else if (input::pressed(ANY_PAD, BTN_PLUS | BTN_B)) {
            audio::play(audio::SFX_BACK);
            dialog_.show("ПОКИНУТЬ КАЗИНО?", "Фишки и имена игроков сохранятся на карте памяти.", {"Остаться", "Выйти"},
                         [](int pick) {
                             if (pick == 1) { save::store(); app::quit(); }
                         });
        }
        cam_ = approach(cam_, (sel_ - 1.5f) * -18, 5, dt);
        for (int i = 0; i < 4; i++) stations_[i].light = approach(stations_[i].light, i == sel_ ? 1.f : 0.f, 9, dt);
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
        // stations
        for (int i = 0; i < 4; i++) drawStation(i);
        // bottom bar
        gfx::rectGrad(0, 610, SCREEN_W, 110, Color(0, 0, 0, 0), Color(0, 0, 0, 240));
        drawPlayers();
        ui::hints({{BTN_A, "Играть"}, {BTN_X, "Игроки"}, {BTN_Y, "Настройки"}, {BTN_PLUS, "Выход"}}, 1262, 700);
        dialog_.render();
    }

private:
    struct Bokeh { float x, y, r; Color c; float phase, speed; };
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
