#include "art.h"

#include <cstring>

#include "../core/save.h"
#include "svg.h"

namespace art {

const i64 CHIP_VALUES[CHIP_COUNT] = {10, 50, 100, 500, 1000, 5000, 25000};

namespace {

Tex g_faces[52];
Tex g_back;
Tex g_cardShadow;
Tex g_chips[CHIP_COUNT];
Tex g_pchips[PLAYER_COLORS];
bool g_done[64 + CHIP_COUNT + PLAYER_COLORS] = {};

const uint32_t kChipBase[CHIP_COUNT] = {0x1f5fae, 0xb3202e, 0x232327, 0x5b2c86, 0xd9a62b, 0xb8541d, 0x146c63};
const uint32_t kChipSpot[CHIP_COUNT] = {0xffffff, 0xffffff, 0xf2f2f2, 0xf3d56e, 0x1f1d1d, 0xffffff, 0xf3d56e};
const uint32_t kChipText[CHIP_COUNT] = {0x1f5fae, 0xa31b28, 0x1b1b1f, 0x4e2473, 0x7a5a10, 0x9a4316, 0x0f5a52};

struct Bounds { int x0, y0, x1, y1; };

Bounds alphaBounds(const Image& m) {
    Bounds b = {m.w, m.h, -1, -1};
    for (int y = 0; y < m.h; y++)
        for (int x = 0; x < m.w; x++)
            if (m.at(x, y)[3] > 6) {
                b.x0 = std::min(b.x0, x); b.y0 = std::min(b.y0, y);
                b.x1 = std::max(b.x1, x); b.y1 = std::max(b.y1, y);
            }
    return b;
}

Image crop(const Image& m, const Bounds& b) {
    if (b.x1 < b.x0) return Image();
    Image c(b.x1 - b.x0 + 1, b.y1 - b.y0 + 1);
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) memcpy(c.at(x, y), m.at(x + b.x0, y + b.y0), 4);
    return c;
}

// Composites tightly-cropped text centred on (cx, cy) in logical units.
void stamp(Image& dst, const std::string& s, FontId f, float size, float cx, float cy, Color c, bool rot180 = false) {
    Image m = gfx::textMask(s, f, size, TEX_SCALE);
    if (m.empty()) return;
    Image t = crop(m, alphaBounds(m));
    if (t.empty()) return;
    if (rot180) t = t.flipped180();
    dst.drawTinted(t, (int)std::lround(cx * TEX_SCALE - t.w / 2.f), (int)std::lround(cy * TEX_SCALE - t.h / 2.f), c);
}

std::string place(int suit, float cx, float cy, float size, bool flip, const std::string& fill) {
    float s = size / 100.f;
    std::string tr = "translate(" + svg::num(cx) + " " + svg::num(cy) + ")" + (flip ? " rotate(180)" : "") +
                     " scale(" + svg::num(s) + ") translate(-50 -50)";
    return "<g transform=\"" + tr + "\" fill=\"" + fill + "\">" + suitShape(suit) + "</g>";
}

std::string goldDefs() {
    return svg::linear("gold", 0, 0, 0, 1,
                       {{0, Color::hex(0xfbeab0)}, {0.45f, Color::hex(0xd8b04c)}, {0.55f, Color::hex(0xb08a33)},
                        {1, Color::hex(0xe9c96b)}});
}

std::string cardShape() {
    return svg::rect(0.6f, 0.6f, CARD_W - 1.2f, CARD_H - 1.2f, 7, svg::url("paper"),
                     svg::stroke("#b9ad95", 0.9f));
}

std::string paperDefs() {
    return svg::linear("paper", 0, 0, 0.35f, 1, {{0, Color::hex(0xfefcf7)}, {1, Color::hex(0xeee6d4)}});
}

Image buildFace(int rank, int suit) {
    const float W = CARD_W, H = CARD_H;
    Color sc = suitColor(suit);
    std::string col = sc.css();
    std::string s = svg::open(W, H) + "<defs>" + paperDefs() + goldDefs() +
                    svg::linear("frame", 0, 0, 0, 1, {{0, Color::hex(0xfaf3e2)}, {1, Color::hex(0xeadbb8)}}) +
                    "</defs>" + cardShape();
    // corner suit pips
    s += place(suit, 12, 31.5f, 11.5f, false, col);
    s += place(suit, W - 12, H - 31.5f, 11.5f, true, col);

    const float L = 31, C = 48, R = 65;
    auto pip = [&](float x, float y) { s += place(suit, x, y, 19, y > 67.5f, col); };
    switch (rank) {
        case 2: pip(C, 30); pip(C, 104); break;
        case 3: pip(C, 30); pip(C, 67); pip(C, 104); break;
        case 4: pip(L, 30); pip(R, 30); pip(L, 104); pip(R, 104); break;
        case 5: pip(L, 30); pip(R, 30); pip(C, 67); pip(L, 104); pip(R, 104); break;
        case 6: for (float y : {30.f, 67.f, 104.f}) { pip(L, y); pip(R, y); } break;
        case 7: for (float y : {30.f, 67.f, 104.f}) { pip(L, y); pip(R, y); } pip(C, 48.5f); break;
        case 8: for (float y : {30.f, 67.f, 104.f}) { pip(L, y); pip(R, y); } pip(C, 48.5f); pip(C, 85.5f); break;
        case 9:
            for (float y : {30.f, 54.7f, 79.3f, 104.f}) { pip(L, y); pip(R, y); }
            pip(C, 67);
            break;
        case 10:
            for (float y : {30.f, 54.7f, 79.3f, 104.f}) { pip(L, y); pip(R, y); }
            pip(C, 42.3f); pip(C, 91.7f);
            break;
        case 14: {
            s += svg::circle(W / 2, H / 2, 31, "none", svg::stroke("#b8923d", 1.1f, 0.85f));
            s += svg::circle(W / 2, H / 2, 27.5f, "none", svg::stroke("#b8923d", 0.6f, 0.6f));
            s += place(suit, W / 2, H / 2, suit == SPADES ? 44 : 38, false, col);
            if (suit == SPADES) s += svg::circle(W / 2, H / 2 + 2, 3, svg::url("gold"));
            break;
        }
        default: { // J Q K
            float fx = 21, fy = 20, fw = W - 42, fh = H - 40;
            s += svg::rect(fx, fy, fw, fh, 3, svg::url("frame"), svg::stroke("#b08a3a", 1.2f));
            s += svg::rect(fx + 2.6f, fy + 2.6f, fw - 5.2f, fh - 5.2f, 2, "none", svg::stroke("#c9a24a", 0.5f, 0.8f));
            // fine diagonal hatch inside the frame
            for (float c0 = -fh; c0 < fw; c0 += 5.5f) {
                float x0 = std::max(0.f, c0), x1 = std::min(fw, c0 + fh);
                if (x1 <= x0) continue;
                s += svg::line(fx + x0, fy + (x0 - c0), fx + x1, fy + (x1 - c0), "#c9a24a", 0.35f, 0.35f);
            }
            float ex = W / 2, ey = 44;
            std::string g = "<g transform=\"translate(" + svg::num(ex - 17) + " " + svg::num(ey - 12) +
                            ") scale(0.34)\">";
            if (rank == 13) {
                g += svg::path("M8 62 L4 18 L28 40 L50 6 L72 40 L96 18 L92 62 Z", svg::url("gold"),
                               svg::stroke("#6b4e14", 2.4f));
                g += svg::rect(8, 56, 84, 13, 2, svg::url("gold"), svg::stroke("#6b4e14", 2.4f));
                for (float jx : {4.f, 50.f, 96.f}) g += svg::circle(jx, jx == 50 ? 6 : 18, 6, col, svg::stroke("#6b4e14", 2));
                g += svg::circle(50, 62.5f, 5, col);
                g += svg::circle(28, 62.5f, 3.5f, "#fffaf0");
                g += svg::circle(72, 62.5f, 3.5f, "#fffaf0");
            } else if (rank == 12) {
                g += svg::path("M10 62 L14 30 Q30 46 34 24 Q42 40 50 8 Q58 40 66 24 Q70 46 86 30 L90 62 Z",
                               svg::url("gold"), svg::stroke("#6b4e14", 2.4f));
                g += svg::rect(10, 56, 80, 11, 5, svg::url("gold"), svg::stroke("#6b4e14", 2.2f));
                g += svg::circle(50, 10, 6.5f, col, svg::stroke("#6b4e14", 2));
                for (float jx : {30.f, 50.f, 70.f}) g += svg::circle(jx, 61.5f, 3.5f, jx == 50 ? col : "#fffaf0");
            } else {
                g += svg::path("M50 2 L94 14 L90 50 C86 72 68 86 50 98 C32 86 14 72 10 50 L6 14 Z", svg::url("gold"),
                               svg::stroke("#6b4e14", 2.6f));
                g += svg::path("M50 12 L84 21 L81 49 C78 66 64 78 50 87 C36 78 22 66 19 49 L16 21 Z", "#fbf5e6");
                g += "<g transform=\"translate(30 26) scale(0.4)\" fill=\"" + col + "\">" + suitShape(suit) + "</g>";
            }
            g += "</g>";
            s += g;
            s += place(suit, W / 2, 101, 13, false, col);
            break;
        }
    }
    s += svg::close();
    Image img = gfx::rasterSvg(s);
    const char* rl = rankLabel(rank);
    FontId f = F_SERIF;
    float rs = rank == 10 ? 16.5f : 18.5f;
    stamp(img, rl, f, rs, 12, 15.5f, sc);
    stamp(img, rl, f, rs, W - 12, H - 15.5f, sc, true);
    if (rank >= 11 && rank <= 13) stamp(img, rl, F_SERIF, 40, W / 2, 77, sc);
    return img;
}

Image buildBack() {
    const float W = CARD_W, H = CARD_H;
    std::string s = svg::open(W, H) + "<defs>" + paperDefs() + goldDefs() +
                    svg::linear("wine", 0, 0, 0.4f, 1, {{0, Color::hex(0x6d1426)}, {1, Color::hex(0x3a0712)}}) +
                    svg::radial("med", 0.5f, 0.5f, 0.5f, {{0, Color::hex(0x7c1a2e)}, {1, Color::hex(0x420a17)}}) +
                    "</defs>" + cardShape();
    float x0 = 5.5f, y0 = 5.5f, x1 = W - 5.5f, y1 = H - 5.5f;
    s += svg::rect(x0, y0, x1 - x0, y1 - y0, 4, svg::url("wine"));
    // gold lattice clipped by hand to the inner rectangle (nanosvg has no clip paths)
    const float Ww = x1 - x0, Hh = y1 - y0;
    for (float c = -Hh; c < Ww; c += 6.5f) { // lines u - v = c
        float u0 = std::max(0.f, c), u1 = std::min(Ww, c + Hh);
        if (u1 > u0) s += svg::line(x0 + u0, y0 + u0 - c, x0 + u1, y0 + u1 - c, "#d7b45a", 0.45f, 0.42f);
    }
    for (float k = 0; k < Ww + Hh; k += 6.5f) { // lines u + v = k
        float u0 = std::max(0.f, k - Hh), u1 = std::min(Ww, k);
        if (u1 > u0) s += svg::line(x0 + u0, y0 + k - u0, x0 + u1, y0 + k - u1, "#d7b45a", 0.45f, 0.42f);
    }
    s += svg::rect(x0 + 3, y0 + 3, x1 - x0 - 6, y1 - y0 - 6, 3, "none", svg::stroke("#e3c46d", 0.9f, 0.9f));
    s += svg::rect(x0 + 5.2f, y0 + 5.2f, x1 - x0 - 10.4f, y1 - y0 - 10.4f, 2, "none", svg::stroke("#e3c46d", 0.4f, 0.7f));
    // medallion
    float cx = W / 2, cy = H / 2;
    s += svg::path("M" + svg::num(cx) + " " + svg::num(cy - 30) + " L" + svg::num(cx + 24) + " " + svg::num(cy) + " L" +
                       svg::num(cx) + " " + svg::num(cy + 30) + " L" + svg::num(cx - 24) + " " + svg::num(cy) + " Z",
                   svg::url("med"), svg::stroke("#e3c46d", 1.2f));
    s += svg::circle(cx, cy, 15.5f, "#3c0812", svg::stroke("#e3c46d", 1.1f));
    for (int i = 0; i < 4; i++) {
        float a = i * PI / 2 - PI / 2;
        s += place(i, cx + std::cos(a) * 21.5f, cy + std::sin(a) * 21.5f * 1.15f, 6.5f, false, "#e3c46d");
    }
    s += svg::close();
    Image img = gfx::rasterSvg(s);
    stamp(img, "GC", F_TITLE, 13, cx, cy + 0.5f, Color::hex(0xecd081));
    return img;
}

Image buildChip(Color base, Color spot, Color textCol, const std::string& label, bool inlayStar) {
    const float S = CHIP_R * 2, c = CHIP_R, R = CHIP_R - 1.f;
    Color lite = base.scaled(1.35f), dark = base.scaled(0.62f);
    std::string s = svg::open(S, S) + "<defs>" +
                    svg::radial("b", 0.38f, 0.3f, 0.8f, {{0, lite}, {0.55f, base}, {1, dark}}) +
                    svg::radial("inl", 0.4f, 0.35f, 0.7f, {{0, Color::hex(0xfffdf6)}, {1, Color::hex(0xe4dac4)}}) +
                    "</defs>";
    s += svg::circle(c, c, R, svg::url("b"), svg::stroke("#000000", 0.9f, 0.45f));
    for (int i = 0; i < 8; i++) {
        float a = (i * 45 + 22.5f) * PI / 180, hw = 8.5f * PI / 180;
        s += svg::path(svg::sector(c, c, R * 0.70f, R - 0.6f, a - hw, a + hw), spot.css(), "fill-opacity=\"0.95\"");
    }
    s += svg::circle(c, c, R * 0.66f, "none", svg::stroke(spot.css(), 0.9f, 0.75f));
    s += svg::circle(c, c, R * 0.585f, "none",
                     svg::stroke(spot.css(), 1.5f, 0.55f) + " stroke-dasharray=\"1.6 1.9\"");
    s += svg::circle(c, c, R * 0.5f, svg::url("inl"), svg::stroke(dark.css(), 0.7f));
    if (inlayStar) s += svg::path(svg::star(c, c, R * 0.32f, R * 0.14f, 5), base.css());
    s += svg::close();
    Image img = gfx::rasterSvg(s);
    if (!label.empty()) stamp(img, label, F_NUM, label.size() >= 3 ? 9.5f : 11.f, c, c, textCol);
    return img;
}

void buildCardShadow() {
    std::string s = svg::open(CARD_W, CARD_H) + svg::rect(0, 0, CARD_W, CARD_H, 7, "#000") + svg::close();
    Image img = gfx::rasterSvg(s).shadow(6, 0.55f);
    g_cardShadow = gfx::upload(img);
}

} // namespace

std::string suitShape(int suit) {
    switch (suit) {
        case SPADES:
            return "<path d=\"M50 4 C58 20 96 38 96 62 C96 78 84 86 72 86 C64 86 57 82 53 76 C54 86 58 92 66 97 "
                   "L34 97 C42 92 46 86 47 76 C43 82 36 86 28 86 C16 86 4 78 4 62 C4 38 42 20 50 4 Z\"/>";
        case HEARTS:
            return "<path d=\"M50 92 C46 84 4 62 4 32 C4 16 16 6 29 6 C39 6 46 12 50 21 C54 12 61 6 71 6 C84 6 96 16 "
                   "96 32 C96 62 54 84 50 92 Z\"/>";
        case DIAMONDS:
            return "<path d=\"M50 3 C60 22 74 38 90 50 C74 62 60 78 50 97 C40 78 26 62 10 50 C26 38 40 22 50 3 Z\"/>";
        default:
            return "<circle cx=\"50\" cy=\"27\" r=\"20\"/><circle cx=\"26\" cy=\"58\" r=\"20\"/>"
                   "<circle cx=\"74\" cy=\"58\" r=\"20\"/><circle cx=\"50\" cy=\"50\" r=\"11\"/>"
                   "<path d=\"M46 52 C46 74 41 88 31 97 L69 97 C59 88 54 74 54 52 Z\"/>";
    }
}

Color suitColor(int suit) { return (suit == HEARTS || suit == DIAMONDS) ? Color::hex(0xb0142c) : Color::hex(0x16151b); }

int buildSteps() { return 52 + 2 + CHIP_COUNT + PLAYER_COLORS; }

void buildStep(int step) {
    if (step < 0 || step >= buildSteps() || g_done[step]) return;
    g_done[step] = true;
    if (step < 52) {
        int rank = step / 4 + 2, suit = step % 4;
        g_faces[step] = gfx::upload(buildFace(rank, suit));
    } else if (step == 52) {
        g_back = gfx::upload(buildBack());
    } else if (step == 53) {
        buildCardShadow();
    } else if (step < 54 + CHIP_COUNT) {
        int i = step - 54;
        g_chips[i] = gfx::upload(buildChip(Color::hex(kChipBase[i]), Color::hex(kChipSpot[i]), Color::hex(kChipText[i]),
                                           fmtChip(CHIP_VALUES[i]), false));
    } else {
        int i = step - 54 - CHIP_COUNT;
        Color base = playerColor(i);
        g_pchips[i] = gfx::upload(buildChip(base, Color(255, 255, 255), base, "", true));
    }
}

void ensureBuilt() {
    for (int i = 0; i < buildSteps(); i++) buildStep(i);
}

void shutdown() {
    for (auto& t : g_faces) t = Tex();
    g_back = Tex();
    g_cardShadow = Tex();
    for (auto& t : g_chips) t = Tex();
    for (auto& t : g_pchips) t = Tex();
    for (auto& d : g_done) d = false;
}

const Tex& cardFace(const Card& c) {
    int i = std::clamp(c.index(), 0, 51);
    buildStep(i);
    return g_faces[i];
}
const Tex& cardBack() { buildStep(52); return g_back; }
const Tex& chip(int d) {
    d = std::clamp(d, 0, CHIP_COUNT - 1);
    buildStep(54 + d);
    return g_chips[d];
}
const Tex& playerChip(int ci) {
    ci = ((ci % PLAYER_COLORS) + PLAYER_COLORS) % PLAYER_COLORS;
    buildStep(54 + CHIP_COUNT + ci);
    return g_pchips[ci];
}

void drawCard(const Card& c, float cx, float cy, float scale, float angleDeg, float faceUp, float alpha, bool shadow,
              Color tint) {
    buildStep(53);
    float t = clamp01(faceUp);
    float sx = std::fabs(std::cos(t * PI));
    float lift = 1 + 0.08f * std::sin(t * PI);
    if (shadow) {
        float off = 2.5f + 6 * std::sin(t * PI);
        gfx::drawCenteredXY(g_cardShadow, cx + off * 0.6f, cy + off, scale * std::max(sx, 0.15f) * lift, scale * lift,
                            angleDeg, alpha);
    }
    const Tex& tex = (t >= 0.5f && c.valid()) ? cardFace(c) : cardBack();
    gfx::drawCenteredXY(tex, cx, cy, scale * std::max(sx, 0.02f) * lift, scale * lift, angleDeg, alpha, tint);
}

void cardHighlight(float cx, float cy, float scale, Color c, float alpha, float angleDeg) {
    (void)angleDeg;
    float w = CARD_W * scale, h = CARD_H * scale;
    gfx::glowEllipse(cx, cy, w * 0.85f, h * 0.75f, c, 0.35f * alpha);
    gfx::roundRectOutline(cx - w / 2 - 2, cy - h / 2 - 2, w + 4, h + 4, 8 * scale + 2, 2.2f, c.alpha(alpha));
}

void cardSlot(float cx, float cy, float scale, float alpha) {
    float w = CARD_W * scale, h = CARD_H * scale;
    gfx::roundRectOutline(cx - w / 2, cy - h / 2, w, h, 7 * scale, 1.3f, Color(240, 225, 180, 70).alpha(alpha));
}

void drawChip(int denom, float cx, float cy, float scale, float alpha) {
    gfx::drawCentered(chip(denom), cx, cy, scale, 0, alpha);
}

int chipIndexFor(i64 v) {
    int idx = 0;
    for (int i = 0; i < CHIP_COUNT; i++)
        if (CHIP_VALUES[i] <= v) idx = i;
    return idx;
}

float drawChipStack(i64 amount, float cx, float cy, float scale, float alpha, int maxStacks) {
    if (amount <= 0) return 0;
    std::vector<int> chips;
    i64 rest = amount;
    for (int i = CHIP_COUNT - 1; i >= 0 && chips.size() < 60; i--)
        while (rest >= CHIP_VALUES[i] && chips.size() < 60) {
            chips.push_back(i);
            rest -= CHIP_VALUES[i];
        }
    if (chips.empty()) chips.push_back(0); // odd remainder still shows a chip
    const int perStack = 10;
    int stacks = std::min(maxStacks, (int)(chips.size() + perStack - 1) / perStack);
    int shown = std::min((int)chips.size(), stacks * perStack);
    float spacing = CHIP_R * 1.75f * scale;
    float width = (stacks - 1) * spacing;
    for (int s = 0; s < stacks; s++) {
        float sx = cx - width / 2 + s * spacing;
        int from = s * perStack, to = std::min(shown, from + perStack);
        gfx::glowEllipse(sx + 1.5f * scale, cy + 4 * scale, CHIP_R * 1.15f * scale, CHIP_R * 1.0f * scale, pal::black,
                         0.6f * alpha, false);
        for (int i = from; i < to; i++) {
            int k = i - from;
            float jitter = ((i * 37) % 5 - 2) * 0.35f * scale;
            float y = cy - k * 4.2f * scale;
            if (k > 0) gfx::ellipse(sx + jitter, y + 3.0f * scale, CHIP_R * scale * 0.96f, CHIP_R * scale * 0.96f,
                                    Color(0, 0, 0, 90).alpha(alpha));
            gfx::drawCentered(chip(chips[i]), sx + jitter, y, scale, 0, alpha);
        }
    }
    return width + CHIP_R * 2 * scale;
}

void drawPlayerChipStack(int colorIdx, int count, float cx, float cy, float scale, float alpha) {
    count = std::clamp(count, 1, 12);
    gfx::glowEllipse(cx + 1.5f * scale, cy + 4 * scale, CHIP_R * 1.15f * scale, CHIP_R * scale, pal::black, 0.6f * alpha,
                     false);
    for (int k = 0; k < count; k++) {
        float y = cy - k * 4.2f * scale;
        if (k > 0) gfx::ellipse(cx, y + 3.0f * scale, CHIP_R * scale * 0.96f, CHIP_R * scale * 0.96f,
                                Color(0, 0, 0, 90).alpha(alpha));
        gfx::drawCentered(playerChip(colorIdx), cx, y, scale, 0, alpha);
    }
}

} // namespace art
