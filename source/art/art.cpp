#include "art.h"

#include <cstring>

#include "../core/save.h"
#include "shade.h"
#include "svg.h"

namespace art {

const i64 CHIP_VALUES[CHIP_COUNT] = {10, 50, 100, 500, 1000, 5000, 25000};

namespace {

Tex g_faces[52];
Tex g_back;
Tex g_cardShadow;
Tex g_chips[CHIP_COUNT];
Tex g_pchips[PLAYER_COLORS];
Tex g_chipPiece[CHIP_COUNT];
Tex g_pchipPiece[PLAYER_COLORS];
float g_rim = 3.8f; // logical rim height of a chip seen from the seat
constexpr float CHIP_THICK = 7.5f, CHIP_ELEV = 58.f;
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

// Gold-foil lettering: the glyph mask is shaded as polished metal.
void foil(Image& dst, const std::string& s, FontId f, float size, float cx, float cy, float bevelPx = 1.4f,
          Color tone = Color::hex(0xf0c45a)) {
    Image m = gfx::textMask(s, f, size, TEX_SCALE);
    if (m.empty()) return;
    Image t = crop(m, alphaBounds(m));
    if (t.empty()) return;
    Image pad(t.w + 4, t.h + 4);
    pad.draw(t, 2, 2);
    for (size_t i = 0; i < pad.px.size(); i += 4) { pad.px[i] = tone.r; pad.px[i + 1] = tone.g; pad.px[i + 2] = tone.b; }
    Image sh = pad.shadow(2, 0.45f);
    shade::bevel(pad, bevelPx * TEX_SCALE, 1.0f, shade::mat::gold());
    int x = (int)std::lround(cx * TEX_SCALE - pad.w / 2.f), y = (int)std::lround(cy * TEX_SCALE - pad.h / 2.f);
    dst.draw(sh, x - 4 + 1, y - 4 + 2);
    dst.draw(pad, x, y);
}

std::string place(int suit, float cx, float cy, float size, bool flip, const std::string& fill) {
    float s = size / 100.f;
    std::string tr = "translate(" + svg::num(cx) + " " + svg::num(cy) + ")" + (flip ? " rotate(180)" : "") +
                     " scale(" + svg::num(s) + ") translate(-50 -50)";
    return "<g transform=\"" + tr + "\" fill=\"" + fill + "\">" + suitShape(suit) + "</g>";
}

std::string cardOutline(const std::string& fill) {
    return svg::open(CARD_W, CARD_H) + svg::rect(0.6f, 0.6f, CARD_W - 1.2f, CARD_H - 1.2f, 7, fill);
}

// Paper body with a softly rounded, lit edge.
shade::Layer paperLayer() {
    shade::Layer L;
    L.svg = cardOutline("#fffbf1") + svg::close();
    L.mat = shade::mat::paper();
    L.mat.exposure = 1.08f;
    L.bevel = 2.4f;
    L.depth = 0.35f;
    L.grain = 0.025f;
    return L;
}

std::string crownPath(int rank) {
    if (rank == 13) return "M8 62 L4 18 L28 40 L50 6 L72 40 L96 18 L92 62 Z";
    if (rank == 12) return "M10 62 L14 30 Q30 46 34 24 Q42 40 50 8 Q58 40 66 24 Q70 46 86 30 L90 62 Z";
    return "M50 2 L94 14 L90 50 C86 72 68 86 50 98 C32 86 14 72 10 50 L6 14 Z";
}

Image buildFace(int rank, int suit) {
    const float W = CARD_W, H = CARD_H;
    Color sc = suitColor(suit);
    std::string col = sc.css();
    std::vector<shade::Layer> layers;
    layers.push_back(paperLayer());
    // printed ink: corner pips and the pip layout
    std::string ink = svg::open(W, H);
    ink += place(suit, 12, 31.5f, 11.5f, false, col);
    ink += place(suit, W - 12, H - 31.5f, 11.5f, true, col);
    const float L = 31, C = 48, R = 65;
    auto pip = [&](float x, float y) { ink += place(suit, x, y, 19, y > 67.5f, col); };
    bool court = rank >= 11 && rank <= 13;
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
        default: break;
    }
    ink += svg::close();
    shade::Layer inkL;
    inkL.svg = ink;
    inkL.mat = shade::mat::gloss();
    inkL.bevel = 0.7f;
    inkL.depth = 0.25f;
    layers.push_back(inkL);
    if (rank == 14) {
        // ace: foil rings and a large lacquered suit
        shade::Layer ring;
        ring.svg = svg::open(W, H) + svg::circle(W / 2, H / 2, 31, "none", svg::stroke("#f0c45a", 2.2f)) +
                   svg::circle(W / 2, H / 2, 27, "none", svg::stroke("#f0c45a", 0.9f)) + svg::close();
        ring.mat = shade::mat::gold();
        ring.bevel = 1.2f;
        layers.push_back(ring);
        shade::Layer big;
        big.svg = svg::open(W, H) + place(suit, W / 2, H / 2, suit == SPADES ? 44 : 38, false, col) + svg::close();
        big.mat = shade::mat::gloss();
        big.bevel = 3.5f;
        big.depth = 0.7f;
        big.shadow = 2;
        big.shadowOpacity = 0.35f;
        layers.push_back(big);
    }
    if (court) {
        float fx = 21, fy = 20, fw = W - 42, fh = H - 40;
        bool red = suit == HEARTS || suit == DIAMONDS;
        // dark lacquered panel with a damask lattice, framed in gold foil
        std::string panel = svg::open(W, H) + "<defs>" +
                            svg::linear("pn", 0, 0, 0, 1,
                                        red ? std::vector<svg::Stop>{{0, Color::hex(0x5a0e1c)}, {1, Color::hex(0x2a050c)}}
                                            : std::vector<svg::Stop>{{0, Color::hex(0x1c2030)}, {1, Color::hex(0x0b0c12)}}) +
                            "</defs>" + svg::rect(fx, fy, fw, fh, 3, svg::url("pn"));
        for (float c0 = -fh; c0 < fw; c0 += 6.f) {
            float x0 = std::max(0.f, c0), x1 = std::min(fw, c0 + fh);
            if (x1 > x0) panel += svg::line(fx + x0, fy + (x0 - c0), fx + x1, fy + (x1 - c0), "#d8b45a", 0.35f, 0.22f);
            float y0 = std::max(0.f, -c0), y1 = std::min(fh, fw - c0);
            (void)y0; (void)y1;
        }
        for (float k = 0; k < fw + fh; k += 6.f) {
            float u0 = std::max(0.f, k - fh), u1 = std::min(fw, k);
            if (u1 > u0) panel += svg::line(fx + u0, fy + k - u0, fx + u1, fy + k - u1, "#d8b45a", 0.35f, 0.22f);
        }
        panel += svg::close();
        shade::Layer pl;
        pl.svg = panel;
        pl.mat = shade::mat::lacquer();
        pl.bevel = 1.2f;
        pl.depth = -0.6f; // pressed in
        layers.push_back(pl);
        shade::Layer frame;
        frame.svg = svg::open(W, H) + svg::rect(fx, fy, fw, fh, 3, "none", svg::stroke("#f0c45a", 1.8f)) +
                    svg::rect(fx + 3, fy + 3, fw - 6, fh - 6, 2, "none", svg::stroke("#f0c45a", 0.6f)) + svg::close();
        frame.mat = shade::mat::gold();
        frame.bevel = 1.f;
        layers.push_back(frame);
        // emblem
        std::string tr = "translate(" + svg::num(W / 2 - 17) + " 32) scale(0.34)";
        shade::Layer em;
        em.svg = svg::open(W, H) + "<g transform=\"" + tr + "\">" + svg::path(crownPath(rank), "#f0c45a") +
                 (rank != 11 ? svg::rect(rank == 13 ? 8 : 10, 56, rank == 13 ? 84 : 80, 12, 3, "#e2b14c") : "") + "</g>" +
                 svg::close();
        em.mat = shade::mat::gold();
        em.bevel = 2.2f;
        em.depth = 0.9f;
        em.shadow = 1;
        layers.push_back(em);
        shade::Layer jewels;
        std::string j = svg::open(W, H) + "<g transform=\"" + tr + "\">";
        if (rank == 13) for (float jx : {4.f, 50.f, 96.f}) j += svg::circle(jx, jx == 50 ? 6 : 18, 6.5f, red ? "#d01430" : "#2a62d8");
        if (rank == 12) j += svg::circle(50, 10, 7, red ? "#d01430" : "#2a62d8");
        if (rank == 11) j += "<g transform=\"translate(30 26) scale(0.4)\" fill=\"" + std::string(red ? "#d01430" : "#2a62d8") + "\">" + suitShape(suit) + "</g>";
        j += "</g>" + svg::close();
        jewels.svg = j;
        jewels.mat = shade::mat::gloss();
        jewels.bevel = 2.f;
        jewels.depth = 1.f;
        layers.push_back(jewels);
        shade::Layer small;
        small.svg = svg::open(W, H) + place(suit, W / 2, 101, 13, false, "#f0c45a") + svg::close();
        small.mat = shade::mat::gold();
        small.bevel = 1.6f;
        layers.push_back(small);
    }
    Image img = shade::relief(W, H, layers);
    const char* rl = rankLabel(rank);
    float rs = rank == 10 ? 16.5f : 18.5f;
    stamp(img, rl, F_SERIF, rs, 12, 15.5f, sc);
    stamp(img, rl, F_SERIF, rs, W - 12, H - 15.5f, sc, true);
    if (court) foil(img, rl, F_SERIF, 40, W / 2, 76, 1.6f);
    return img;
}

Image buildBack() {
    const float W = CARD_W, H = CARD_H;
    std::vector<shade::Layer> layers;
    layers.push_back(paperLayer());
    float x0 = 5.5f, y0 = 5.5f, x1 = W - 5.5f, y1 = H - 5.5f;
    shade::Layer panel;
    panel.svg = svg::open(W, H) + "<defs>" +
                svg::linear("wine", 0, 0, 0.4f, 1, {{0, Color::hex(0x8a1028)}, {0.55f, Color::hex(0x5c0818)}, {1, Color::hex(0x2c030c)}}) + "</defs>" +
                svg::rect(x0, y0, x1 - x0, y1 - y0, 4, svg::url("wine")) + svg::close();
    panel.mat = shade::mat::lacquer();
    panel.bevel = 1.2f;
    panel.depth = -0.5f;
    panel.grain = 0.04f;
    layers.push_back(panel);
    // gold-foil lattice, borders and medallion, all stamped in metal
    std::string g = svg::open(W, H);
    const float Ww = x1 - x0, Hh = y1 - y0;
    for (float c = -Hh; c < Ww; c += 9.f) {
        float u0 = std::max(0.f, c), u1 = std::min(Ww, c + Hh);
        if (u1 > u0) g += svg::line(x0 + u0, y0 + u0 - c, x0 + u1, y0 + u1 - c, "#e8bf5c", 0.5f, 0.6f);
    }
    for (float k = 0; k < Ww + Hh; k += 9.f) {
        float u0 = std::max(0.f, k - Hh), u1 = std::min(Ww, k);
        if (u1 > u0) g += svg::line(x0 + u0, y0 + k - u0, x0 + u1, y0 + k - u1, "#e8bf5c", 0.5f, 0.6f);
    }
    g += svg::rect(x0 + 3, y0 + 3, x1 - x0 - 6, y1 - y0 - 6, 3, "none", svg::stroke("#e8bf5c", 1.4f));
    float cx = W / 2, cy = H / 2;
    g += svg::path("M" + svg::num(cx) + " " + svg::num(cy - 32) + " L" + svg::num(cx + 26) + " " + svg::num(cy) + " L" +
                       svg::num(cx) + " " + svg::num(cy + 32) + " L" + svg::num(cx - 26) + " " + svg::num(cy) + " Z",
                   "#2a050c", svg::stroke("#e8bf5c", 2.f));
    g += svg::close();
    shade::Layer foilL;
    foilL.svg = g;
    foilL.mat = shade::mat::gold();
    foilL.bevel = 0.9f;
    foilL.depth = 0.8f;
    layers.push_back(foilL);
    shade::Layer med;
    std::string m = svg::open(W, H) + svg::circle(cx, cy, 16.5f, "none", svg::stroke("#e8bf5c", 2.2f));
    for (int i = 0; i < 4; i++) {
        float a = i * PI / 2 - PI / 2;
        m += place(i, cx + std::cos(a) * 22.f, cy + std::sin(a) * 22.f * 1.15f, 7.f, false, "#e8bf5c");
    }
    m += svg::close();
    med.svg = m;
    med.mat = shade::mat::gold();
    med.bevel = 1.3f;
    med.shadow = 1;
    layers.push_back(med);
    Image img = shade::relief(W, H, layers);
    foil(img, "GC", F_TITLE, 13, cx, cy + 0.5f, 1.1f);
    return img;
}

// Top view of a chip: clay body, inlaid edge spots, glossy centre inlay.
Image buildChip(Color base, Color spot, Color textCol, const std::string& label, bool inlayStar) {
    const float S = CHIP_R * 2, c = CHIP_R, R = CHIP_R - 1.f;
    std::vector<shade::Layer> L;
    shade::Layer body;
    body.svg = svg::open(S, S) + svg::circle(c, c, R, base.css()) + svg::close();
    body.mat = shade::mat::clay();
    body.bevel = 4.f;
    body.depth = 0.8f;
    body.grain = 0.05f;
    L.push_back(body);
    std::string sp = svg::open(S, S);
    for (int i = 0; i < 8; i++) {
        float a = (i * 45 + 22.5f) * PI / 180, hw = 8.5f * PI / 180;
        sp += svg::path(svg::sector(c, c, R * 0.70f, R - 0.4f, a - hw, a + hw), spot.css());
    }
    sp += svg::close();
    shade::Layer spots;
    spots.svg = sp;
    spots.mat = shade::mat::clay();
    spots.bevel = 0.9f;
    spots.depth = 0.3f;
    spots.grain = 0.04f;
    L.push_back(spots);
    shade::Layer ring;
    ring.svg = svg::open(S, S) + svg::circle(c, c, R * 0.66f, "none", svg::stroke(spot.css(), 0.9f, 0.85f)) +
               svg::circle(c, c, R * 0.585f, "none", svg::stroke(spot.css(), 1.5f, 0.6f) + " stroke-dasharray=\"1.6 1.9\"") +
               svg::close();
    ring.flat = true;
    L.push_back(ring);
    shade::Layer inlay;
    Color inl = inlayStar ? base.scaled(0.55f) : Color::hex(0xf1eadb);
    inlay.svg = svg::open(S, S) + svg::circle(c, c, R * 0.5f, inl.css()) + svg::close();
    inlay.mat = shade::mat::gloss();
    inlay.bevel = 1.6f;
    inlay.depth = 0.7f;
    L.push_back(inlay);
    if (inlayStar) {
        shade::Layer st;
        st.svg = svg::open(S, S) + svg::path(svg::star(c, c, R * 0.33f, R * 0.14f, 5), "#f0c45a") + svg::close();
        st.mat = shade::mat::gold();
        st.bevel = 1.6f;
        L.push_back(st);
    }
    Image img = shade::relief(S, S, L);
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
        Image top = buildChip(Color::hex(kChipBase[i]), Color::hex(kChipSpot[i]), Color::hex(kChipText[i]),
                              fmtChip(CHIP_VALUES[i]), false);
        g_chipPiece[i] = gfx::upload(shade::chipPiece(top, Color::hex(kChipBase[i]), Color::hex(kChipSpot[i]), CHIP_R,
                                                      CHIP_THICK, CHIP_ELEV, TEX_SCALE, &g_rim));
        g_chips[i] = gfx::upload(top);
    } else {
        int i = step - 54 - CHIP_COUNT;
        Color base = playerColor(i);
        Image top = buildChip(base, Color(245, 242, 235), base, "", true);
        g_pchipPiece[i] = gfx::upload(shade::chipPiece(top, base, Color(245, 242, 235), CHIP_R, CHIP_THICK, CHIP_ELEV,
                                                       TEX_SCALE, &g_rim));
        g_pchips[i] = gfx::upload(top);
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
    for (auto& t : g_chipPiece) t = Tex();
    for (auto& t : g_pchipPiece) t = Tex();
    for (auto& d : g_done) d = false;
}

Image cardImage(const Card& c) { return buildFace(c.rank, c.suit); }

const Image& cached(const std::string& key, const std::function<Image()>& make) {
    static std::map<std::string, Image> cache;
    auto it = cache.find(key);
    if (it == cache.end()) it = cache.emplace(key, make()).first;
    return it->second;
}
Image cardBackImage() { return buildBack(); }
Image chipImage(int i, bool piece) {
    Image top = buildChip(Color::hex(kChipBase[i]), Color::hex(kChipSpot[i]), Color::hex(kChipText[i]), fmtChip(CHIP_VALUES[i]), false);
    if (!piece) return top;
    return shade::chipPiece(top, Color::hex(kChipBase[i]), Color::hex(kChipSpot[i]), CHIP_R, CHIP_THICK, CHIP_ELEV);
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

// Draws one stack of chip pieces; `pieces` returns the texture for the k-th chip.
static void drawPieces(float cx, float cy, float scale, float alpha, int count,
                       const std::function<const Tex&(int)>& pieces) {
    float rimL = g_rim * scale;
    float se = std::sin(CHIP_ELEV * PI / 180.f);
    // soft contact shadow on the felt
    gfx::glowEllipse(cx + 2 * scale, cy + rimL * 0.6f + 3 * scale, CHIP_R * 1.25f * scale, CHIP_R * se * 1.15f * scale,
                     pal::black, 0.7f * alpha, false);
    for (int k = 0; k < count; k++) {
        float jitter = ((k * 37) % 5 - 2) * 0.4f * scale;
        float y = cy - k * rimL;
        gfx::drawCentered(pieces(k), cx + jitter, y + rimL * 0.5f, scale, 0, alpha);
    }
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
    float spacing = CHIP_R * 1.85f * scale;
    float width = (stacks - 1) * spacing;
    for (int s = 0; s < stacks; s++) {
        float sx = cx - width / 2 + s * spacing;
        int from = s * perStack, to = std::min(shown, from + perStack);
        chip(0); // make sure pieces exist
        drawPieces(sx, cy, scale, alpha, to - from, [&](int k) -> const Tex& {
            int d = chips[from + k];
            chip(d);
            return g_chipPiece[d];
        });
    }
    return width + CHIP_R * 2 * scale;
}

void drawPlayerChipStack(int colorIdx, int count, float cx, float cy, float scale, float alpha) {
    count = std::clamp(count, 1, 12);
    int ci = ((colorIdx % PLAYER_COLORS) + PLAYER_COLORS) % PLAYER_COLORS;
    playerChip(ci);
    drawPieces(cx, cy, scale, alpha, count, [&](int) -> const Tex& { return g_pchipPiece[ci]; });
}

} // namespace art
