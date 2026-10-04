// Blackjack table: up to five players against the dealer.
#include "../art/art.h"
#include "../art/backdrop.h"
#include "../art/shade.h"
#include "../art/svg.h"
#include "../art/tableart.h"
#include "../core/anim.h"
#include "../core/app.h"
#include "../core/audio.h"
#include "../core/fx.h"
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/save.h"
#include "../core/trophy.h"
#include "../core/ui.h"
#include "blackjack_logic.h"
#include "quips.h"

namespace {

struct Pt { float x, y; };

constexpr float TCX = 640, TCY = 130;
constexpr float FELT_RX = 690, FELT_RY = 520;
constexpr float CARD_SCALE = 0.74f;
constexpr float DEALER_SCALE = 0.8f;
const Pt SHOE = {1112, 84};
const Pt DISCARD = {168, 84};
const Pt TRAY = {640, 22};
const float DEALER_Y = 140;

Pt spotPos(int spot) {
    float th = (150 - 30 * spot) * PI / 180;
    return {TCX + 470 * std::cos(th), TCY + 400 * std::sin(th)};
}
float spotAngle(int spot) {
    Pt p = spotPos(spot);
    return -std::atan2(p.x - TCX, p.y - TCY) * 180 / PI * 0.7f;
}
Pt upVec(int spot) {
    float a = spotAngle(spot) * PI / 180;
    return {std::sin(a), -std::cos(a)};
}
Pt rightVec(int spot) {
    float a = spotAngle(spot) * PI / 180;
    return {std::cos(a), std::sin(a)};
}
float panFor(float x) { return (x - 640) / 700.f; }

const char* kRules =
    "Цель — набрать больше очков, чем дилер, но не больше 21. Карты 2–10 по номиналу, "
    "J, Q, K — 10 очков, туз — 1 или 11.\n\n"
    "Блэкджек (туз + десятка первыми двумя картами) платит 3 к 2. Обычный выигрыш — 1 к 1, "
    "при равенстве ставка возвращается.\n\n"
    "Дилер добирает до 16 и останавливается на любых 17. Игра идёт шестью колодами.\n\n"
    "Ещё (A) — взять карту. Хватит (B) — остановиться. Удвоить (X) — удвоить ставку и получить ровно одну карту. "
    "Сплит (Y) — разделить пару на две руки (до четырёх рук; на разделённые тузы приходит по одной карте). "
    "Сдаться (R) — вернуть половину ставки, пока у вас две первые карты.\n\n"
    "Если у дилера открыт туз, можно взять страховку за половину ставки — она платит 2 к 1, "
    "когда у дилера блэкджек.";

std::string ellipseRing(float rxO, float ryO, float rxI, float ryI) {
    auto ell = [](float rx, float ry, int sweep) {
        return "M" + svg::num(TCX - rx) + " " + svg::num(TCY) + " A" + svg::num(rx) + " " + svg::num(ry) + " 0 1 " + std::to_string(sweep) +
               " " + svg::num(TCX + rx) + " " + svg::num(TCY) + " A" + svg::num(rx) + " " + svg::num(ry) + " 0 1 " + std::to_string(sweep) +
               " " + svg::num(TCX - rx) + " " + svg::num(TCY) + " Z";
    };
    return ell(rxO, ryO, 0) + " " + ell(rxI, ryI, 1);
}

shade::Layer layer(const std::string& svgBody, const shade::Material& m, float bevel, float depth = 1.f, int shadow = 0) {
    shade::Layer l;
    l.svg = svgBody;
    l.mat = m;
    l.bevel = bevel;
    l.depth = depth;
    l.shadow = shadow;
    return l;
}

// Dealer's lacquered edge with the brass trim and the chip float tray.
Image buildDealerEdge() {
    std::vector<shade::Layer> L;
    shade::Layer bar = layer(svg::rect(-40, -40, SCREEN_W + 80, 76, 0, "#6a3a1a"), shade::mat::lacquer(), 7, 0.7f);
    bar.paint = [](Image& im) { art::woodGrain(im, 0, 0, Color::hex(0x2a1208), Color::hex(0x8a4e24), 77, false); };
    L.push_back(bar);
    L.push_back(layer(svg::rect(-40, 31, SCREEN_W + 80, 3.2f, 0, "#e2b450"), shade::mat::gold(), 1.4f));
    L.push_back(layer(svg::rect(468, -20, 344, 72, 9, "#1a120c"), shade::mat::lacquer(), 5, 0.9f, 6));
    L.push_back(layer(svg::rect(477, -20, 326, 63, 5, "#0b0807"), shade::mat::leather(), 3, -0.8f));
    static const uint32_t cols[] = {0xd9a62b, 0x5b2c86, 0x232327, 0x232327, 0x1f8a52, 0x1f8a52,
                                    0xb3202e, 0xb3202e, 0x1f5fae, 0x1f5fae, 0xb8541d, 0x146c63};
    std::string chips;
    for (int i = 0; i < 12; i++) {
        float x = 482 + i * 26.6f;
        Color c = Color::hex(cols[i]);
        chips += svg::rect(x, -20, 23, 60, 4, c.css());
        // chip edges: grooves between chips and the white edge inserts
        for (float y = -18; y < 39; y += 4.4f) {
            chips += svg::rect(x, y, 23, 0.8f, 0, c.scaled(0.45f).css());
            if ((int)((y + 18) / 4.4f) % 2 == 0) {
                chips += svg::rect(x + 3, y + 1.2f, 4, 2.4f, 0, "#f2eee4");
                chips += svg::rect(x + 16, y + 1.2f, 4, 2.4f, 0, "#f2eee4");
            }
        }
    }
    shade::Layer cl = layer(chips, shade::mat::clay(), 11.5f, 0.9f);
    cl.grain = 0.04f;
    L.push_back(cl);
    L.push_back(layer(svg::rect(468, 41, 344, 7, 3, "#cfd0d6"), shade::mat::silver(), 2.5f, 1.f, 3));
    return art::regionRelief(0, 0, SCREEN_W, 70, L);
}

// Smoked-acrylic shoe and discard holder.
Image buildShoe(bool discard, float& ox, float& oy) {
    Pt c = discard ? DISCARD : SHOE;
    float R = 86;
    ox = c.x - R;
    oy = c.y - 8 - R;
    std::string g = "<g transform=\"translate(" + svg::num(c.x) + " " + svg::num(c.y - 8) + ") rotate(" + (discard ? "25" : "-25") + ")\">";
    std::vector<shade::Layer> L;
    if (!discard) {
        L.push_back(layer(g + svg::rect(-46, -36, 92, 82, 9, "#141318") + "</g>", shade::mat::gloss(), 9, 0.9f, 8));
        L.push_back(layer(g + svg::rect(-38, -28, 76, 58, 5, "#08080a") + "</g>", shade::mat::gloss(), 3, -0.8f));
        std::string back = svg::rect(-35, -25, 70, 52, 4, "#7a1028") + svg::rect(-31, -21, 62, 44, 3, "none", svg::stroke("#e2b450", 1.2f));
        for (float c = -44; c < 62; c += 7) {
            float u0 = std::max(0.f, c), u1 = std::min(62.f, c + 44);
            back += svg::line(-31 + u0, -21 + u0 - c, -31 + u1, -21 + u1 - c, "#e2b450", 0.5f, 0.5f);
            back += svg::line(-31 + u0, 23 - (u0 - c), -31 + u1, 23 - (u1 - c), "#e2b450", 0.5f, 0.5f);
        }
        L.push_back(layer(g + back + "</g>", shade::mat::lacquer(), 1.5f, 0.6f));
        L.push_back(layer(g + svg::rect(-48, 28, 96, 20, 5, "#c9cad2") + "</g>", shade::mat::silver(), 4, 1.f, 3));
        L.push_back(layer(g + svg::rect(-30, 35, 60, 5, 2.5f, "#060607") + "</g>", shade::mat::gloss(), 1.5f, -1.f));
    } else {
        L.push_back(layer(g + svg::rect(-44, -34, 88, 78, 7, "#101014") + "</g>", shade::mat::gloss(), 7, 0.9f, 8));
        L.push_back(layer(g + svg::rect(-37, -27, 74, 64, 4, "#1c1c22") + "</g>", shade::mat::gloss(), 3, -0.8f));
        std::string stack;
        for (int k = 0; k < 4; k++)
            stack += "<g transform=\"rotate(" + svg::num(-3.f + k * 2.2f) + ")\">" + svg::rect(-31, -23, 62, 54, 3, k == 3 ? "#7a1028" : "#5a0a1c", svg::stroke("#e2b450", 0.8f, 0.8f)) + "</g>";
        L.push_back(layer(g + stack + "</g>", shade::mat::lacquer(), 1.2f, 0.5f));
        L.push_back(layer(g + svg::rect(-46, 38, 92, 9, 4, "#c9cad2") + "</g>", shade::mat::silver(), 3, 1.f, 3));
    }
    return art::regionRelief(ox, oy, 2 * R, 2 * R, L, 4);
}

Image buildTable() {
    const float S = TEX_SCALE;
    int W = (int)(SCREEN_W * S), H = (int)(SCREEN_H * S);
    Image img = art::feltImage(SCREEN_W, SCREEN_H, Color::hex(0x1f7d4e), Color::hex(0x0a3a23), 11);
    // lamp over the table, occlusion along the rail and the dealer's edge
    art::feltLight(img, TCX, 330, 560, 330,
                   [](float x, float y) {
                       float dx = (x - TCX) / FELT_RX, dy = (y - TCY) / FELT_RY;
                       return std::min((1 - std::sqrt(dx * dx + dy * dy)) * FELT_RY, y - 34.f);
                   },
                   30, 0.8f);
    // outside the felt is the dark room
    for (int py = 0; py < H; py++)
        for (int px = 0; px < W; px++) {
            float x = (px + 0.5f) / S, y = (py + 0.5f) / S;
            float ex = (x - TCX) / FELT_RX, ey = (y - TCY) / FELT_RY;
            float e = std::sqrt(ex * ex + ey * ey);
            float edge = (e - 1) * 520; // ~logical px outside the edge
            if (edge > -1) {
                uint8_t* p = img.at(px, py);
                float k = clamp01(edge + 1);
                p[0] = (uint8_t)lerp(p[0], 10, k);
                p[1] = (uint8_t)lerp(p[1], 7, k);
                p[2] = (uint8_t)lerp(p[2], 6, k);
            }
        }
    std::string s = svg::open(SCREEN_W, SCREEN_H);
    // insurance band
    auto arc = [](float rx, float ry, float a0, float a1) {
        float x0 = TCX + std::cos(a0) * rx, y0 = TCY + std::sin(a0) * ry;
        float x1 = TCX + std::cos(a1) * rx, y1 = TCY + std::sin(a1) * ry;
        return "M" + svg::num(x0) + " " + svg::num(y0) + " A" + svg::num(rx) + " " + svg::num(ry) + " 0 0 0 " +
               svg::num(x1) + " " + svg::num(y1);
    };
    float a0 = 152 * PI / 180, a1 = 28 * PI / 180;
    s += svg::path(arc(420, 178, a0, a1), "none", svg::stroke("#e2c26a", 1.6f, 0.75f));
    s += svg::path(arc(452, 208, a0, a1), "none", svg::stroke("#e2c26a", 1.6f, 0.75f));
    // betting circles
    for (int i = 0; i < 5; i++) {
        Pt p = spotPos(i);
        s += svg::circle(p.x, p.y, 39, "none", svg::stroke("#e2c26a", 1.8f, 0.8f));
        s += svg::circle(p.x, p.y, 34, "none", svg::stroke("#e2c26a", 0.8f, 0.55f));
    }
    s += svg::close();
    Image prints = gfx::rasterSvg(s);
    img.draw(prints, 0, 0);
    Color goldInk = Color::hex(0xe4c66c, 215);
    art::arcText(img, "BLACKJACK PAYS 3 TO 2", TCX, TCY, 330, 126, F_SERIF, 27, goldInk, 1.08f);
    art::arcText(img, "Dealer must draw to 16 and stand on all 17s", TCX, TCY, 362, 152, F_SANS, 15,
                 Color::hex(0xe9dcb6, 190), 1.05f);
    art::arcText(img, "INSURANCE  PAYS  2  TO  1", TCX, TCY, 436, 193, F_SANS_BOLD, 15, goldInk, 1.15f);
    art::stampText(img, "GRAND CASINO", TCX, 214, F_TITLE, 13, Color::hex(0xe4c66c, 110));
    // padded leather armrest with a brass edge
    {
        const float top = 300;
        std::vector<shade::Layer> L;
        shade::Layer rail = layer(svg::path(ellipseRing(FELT_RX + 50, FELT_RY + 50, FELT_RX + 3, FELT_RY + 3), "#2e1c14", "fill-rule=\"evenodd\""),
                                  shade::mat::leather(), 22, 0.55f, 10);
        rail.grain = 0.06f;
        rail.grainCell = 2.2f;
        rail.shadowOpacity = 0.6f;
        L.push_back(rail);
        L.push_back(layer(svg::ellipse(TCX, TCY, FELT_RX + 26, FELT_RY + 26, "none", svg::stroke("#a08a70", 1.1f, 0.8f) + " stroke-dasharray=\"4 3.2\""),
                          shade::mat::leather(), 0.6f, -0.8f));
        L.push_back(layer(svg::path(ellipseRing(FELT_RX + 4, FELT_RY + 4, FELT_RX, FELT_RY), "#e2b450", "fill-rule=\"evenodd\""),
                          shade::mat::gold(), 1.6f));
        Image r = art::regionRelief(0, top, SCREEN_W, SCREEN_H - top, L);
        img.draw(r, 0, (int)(top * S));
    }
    img.draw(buildDealerEdge(), 0, 0);
    for (int d = 0; d < 2; d++) {
        float ox, oy;
        Image sh = buildShoe(d == 1, ox, oy);
        img.draw(sh, (int)std::lround(ox * S), (int)std::lround(oy * S));
    }
    img.vignette(0.35f, 0.5f);
    return img;
}

enum Phase { BETTING, DEALING, INSURANCE, PLAYING, DEALER, SETTLING, ROUND_END };

struct Seat {
    int profile = 0;
    int order = 0; // index among active players => controller
    int spot = 2;
    std::vector<bj::Hand> hands;
    std::vector<std::vector<int>> sprites;
    int cur = 0;
    i64 bet = 0, lastBet = 0, flying = 0;
    std::vector<i64> placed;
    int chipSel = 2;
    i64 insurance = 0;
    i64 staked = 0; // everything taken from the wallet this round
    i64 returned = 0;
    std::vector<std::string> label;
    std::vector<Color> labelColor;
    bool inRound() const { return !hands.empty(); }
};

class BlackjackScene : public Scene {
public:
    BlackjackScene() : shoe_(bj::DECKS, 0.75f) {
        table_ = gfx::upload(art::cached("blackjack.table", buildTable));
        art::ensureBuilt();
        auto act = save::active();
        static const int layouts[5][5] = {{2}, {3, 1}, {3, 2, 1}, {4, 3, 2, 1}, {4, 3, 2, 1, 0}};
        int n = std::min<int>((int)act.size(), 5);
        for (int k = 0; k < n; k++) {
            Seat s;
            s.profile = act[k];
            s.order = k;
            s.spot = layouts[n - 1][k];
            s.chipSel = 1;
            seats_.push_back(s);
        }
        if (act.size() > 5) ui::toast("За столом пять мест — " + save::player(act[5]).name + " пока наблюдает");
        caption_.say("Делайте ваши ставки");
        audio::play(audio::SFX_SHUFFLE, 0.7f);
        phase_ = BETTING;
        turn_ = 0;
    }

    void update(float dt) override {
        t_ += dt;
        caption_.update(dt);
        banner_.update(dt);
        for (auto& s : sprites_) s.update(dt);
        for (auto& f : flying_) f.update(dt);
        pruneDone(flying_);
        timers_.update(dt);
        if (pause_.update(dt)) return;
        if (input::pressed(ANY_PAD, BTN_PLUS)) { openPause(); return; }
        seq_.update(dt);
        if (seq_.busy()) return;
        switch (phase_) {
            case BETTING: updateBetting(); break;
            case INSURANCE: updateInsurance(); break;
            case PLAYING: updatePlaying(); break;
            case ROUND_END:
                endTimer_ += dt;
                if (endTimer_ > 3.2f || input::pressed(ANY_PAD, BTN_A)) cleanup();
                break;
            default: break;
        }
    }

    void render() override {
        gfx::draw(table_, 0, 0);
        // lamp light pooling on the felt
        gfx::glowEllipse(640, 360, 520, 300, Color::hex(0xfff0c8), 0.07f);
        // bets and insurance
        for (auto& s : seats_) {
            Pt p = spotPos(s.spot);
            i64 shown = 0;
            if (s.inRound()) for (auto& h : s.hands) shown += h.bet;
            else shown = s.bet - s.flying;
            if (shown > 0) art::drawChipStack(shown, p.x, p.y + 8, 0.78f, 1.f, 2);
            if (s.insurance > 0) {
                Pt ip = insurancePos(s);
                art::drawChipStack(s.insurance, ip.x, ip.y, 0.5f);
            }
        }
        bool turnRing = (phase_ == BETTING || phase_ == PLAYING || phase_ == INSURANCE) && !seq_.busy();
        if (turnRing && cur() >= 0) {
            Pt p = spotPos(seats_[cur()].spot);
            float pulse = 0.5f + 0.5f * std::sin(t_ * 5);
            gfx::ring(p.x, p.y, 44 + pulse * 2, 2.5f, pal::goldLight.alpha(0.5f + 0.4f * pulse));
            gfx::glow(p.x, p.y, 70, pal::gold, 0.12f + 0.08f * pulse);
        }
        for (auto& s : sprites_) s.draw();
        // hand totals / results
        for (auto& s : seats_) {
            for (size_t h = 0; h < s.hands.size(); h++) {
                if (s.hands[h].cards.empty()) continue;
                Pt lp = labelPos(s, (int)h);
                bool active = phase_ == PLAYING && cur() >= 0 && &seats_[cur()] == &s && (int)h == s.cur && !seq_.busy();
                std::string txt = h < s.label.size() && !s.label[h].empty() ? s.label[h] : bj::totalLabel(s.hands[h].cards);
                Color col = h < s.label.size() && !s.label[h].empty() ? s.labelColor[h] : pal::ivory;
                pill(txt, lp.x, lp.y, col, active);
            }
        }
        if (!dealer_.empty()) {
            bool holeHidden = dealerSprites_.size() >= 2 && sprites_[dealerSprites_[1]].face < 0.5f;
            std::string dt = holeHidden ? bj::totalLabel({dealer_[0]}) : bj::totalLabel(dealer_);
            float lx = dealerX((int)dealer_.size() - 1, (int)dealer_.size()) + 62;
            pill(dt, lx, DEALER_Y, pal::ivory, false);
        }
        for (auto& f : flying_) f.draw();
        // plates
        for (auto& s : seats_) {
            Pt p = spotPos(s.spot), u = upVec(s.spot);
            Pt pp = {p.x - u.x * 70, p.y - u.y * 70};
            bool active = cur() >= 0 && &seats_[cur()] == &s && phase_ != DEALING && phase_ != DEALER && !seq_.busy() &&
                          phase_ != ROUND_END && phase_ != SETTLING;
            std::string l2;
            if (phase_ == BETTING) l2 = fmtMoney(save::player(s.profile).balance - s.bet);
            Color l2c = pal::goldLight;
            if (phase_ == ROUND_END && s.staked > 0) {
                i64 net = s.returned - s.staked;
                l2 = (net > 0 ? "+" : "") + fmtMoney(net);
                l2c = net > 0 ? pal::greenBright : (net < 0 ? pal::redBright : pal::ivory);
            }
            art::seatPlate(s.profile, pp.x, pp.y, active, 0.5f + 0.5f * std::sin(t_ * 5), l2, l2c);
        }
        caption_.render(640, 66);
        banner_.render(640, 330, 58);
        drawHud();
        pause_.render();
    }

private:
    // ---------------------------------------------------------------- helpers
    float dealDur() const { return save::data().settings.fastDeal ? 0.2f : 0.34f; }
    float stepGap() const { return save::data().settings.fastDeal ? 0.16f : 0.3f; }

    int cur() const { return turn_ < (int)seats_.size() ? turn_ : -1; }
    int padOf(const Seat& s) const { return input::padForSeat(s.order); }

    Pt insurancePos(const Seat& s) const {
        Pt p = spotPos(s.spot), u = upVec(s.spot), r = rightVec(s.spot);
        return {p.x + r.x * 58 + u.x * 26, p.y + r.y * 58 + u.y * 26};
    }

    void pill(const std::string& txt, float x, float y, Color c, bool active) {
        float w = std::max(34.f, gfx::textWidth(txt, F_NUM, 17) + 20);
        gfx::roundRect(x - w / 2, y - 13, w, 26, 13, Color(6, 6, 8, 215));
        gfx::roundRectOutline(x - w / 2, y - 13, w, 26, 13, active ? 1.6f : 1.f,
                              active ? pal::goldLight : Color(255, 255, 255, 50));
        gfx::text(txt, x, y, F_NUM, 17, c, 0);
    }

    Pt cardPos(const Seat& s, int h, int i, float* ang) const {
        Pt p = spotPos(s.spot), u = upVec(s.spot), r = rightVec(s.spot);
        int n = (int)s.hands.size();
        float ho = (h - (n - 1) / 2.f) * 78;
        Pt base = {p.x + u.x * 102 + r.x * ho, p.y + u.y * 102 + r.y * ho};
        if (ang) *ang = spotAngle(s.spot);
        return {base.x + r.x * 15 * i + u.x * 19 * i, base.y + r.y * 15 * i + u.y * 19 * i};
    }
    Pt labelPos(const Seat& s, int h) const {
        Pt c0 = cardPos(s, h, 0, nullptr), u = upVec(s.spot);
        return {c0.x - u.x * 62, c0.y - u.y * 62};
    }
    float dealerX(int i, int n) const { return 640 + (i - (n - 1) / 2.f) * 56; }

    int spawn(Card c) {
        CardSprite sp;
        sp.card = c;
        sp.place(SHOE.x - 20, SHOE.y + 10, -25, 0, 0.7f);
        sprites_.push_back(sp);
        return (int)sprites_.size() - 1;
    }

    void dealTo(Seat& s, int h, bool sideways = false) {
        Card c = shoe_.draw();
        s.hands[h].cards.push_back(c);
        int id = spawn(c);
        s.sprites[h].push_back(id);
        float a;
        Pt p = cardPos(s, h, (int)s.hands[h].cards.size() - 1, &a);
        sprites_[id].moveTo(p.x, p.y, a + (sideways ? 90 : 0), 1, CARD_SCALE, dealDur());
        audio::play(audio::SFX_CARD_SLIDE, 0.8f, panFor(p.x), 0.95f + rng().uniform(0, 0.1f));
    }

    void dealDealer(bool faceUp) {
        Card c = shoe_.draw();
        dealer_.push_back(c);
        int id = spawn(c);
        dealerSprites_.push_back(id);
        int n = (int)dealer_.size();
        for (int i = 0; i < n; i++) {
            CardSprite& sp = sprites_[dealerSprites_[i]];
            float face = i == n - 1 ? (faceUp ? 1.f : 0.f) : sp.tf;
            sp.moveTo(dealerX(i, n), DEALER_Y, 0, face, DEALER_SCALE, i == n - 1 ? dealDur() : 0.25f);
        }
        audio::play(audio::SFX_CARD_SLIDE, 0.8f, 0, 0.95f);
    }

    void relayout(Seat& s) {
        for (size_t h = 0; h < s.hands.size(); h++)
            for (size_t i = 0; i < s.sprites[h].size(); i++) {
                float a;
                Pt p = cardPos(s, (int)h, (int)i, &a);
                CardSprite& sp = sprites_[s.sprites[h][i]];
                bool side = s.hands[h].doubled && i == 2;
                sp.moveTo(p.x, p.y, a + (side ? 90 : 0), sp.tf, CARD_SCALE, 0.25f);
            }
    }

    void flyChips(i64 amount, Pt from, Pt to, float delay, float dur = 0.45f) {
        if (amount <= 0) return;
        FlyingChips f;
        f.amount = amount;
        f.fx = from.x; f.fy = from.y; f.tx = to.x; f.ty = to.y;
        f.delay = delay;
        f.dur = dur;
        f.scale = 0.62f;
        f.onArrive = [] {};
        flying_.push_back(f);
    }

    Pt platePos(const Seat& s) const {
        Pt p = spotPos(s.spot), u = upVec(s.spot);
        return {p.x - u.x * 70, p.y - u.y * 70};
    }

    // ---------------------------------------------------------------- betting
    void updateBetting() {
        if (turn_ >= (int)seats_.size()) { startDeal(); return; }
        Seat& s = seats_[turn_];
        Profile& prof = save::player(s.profile);
        int pad = padOf(s);
        i64 avail = prof.balance - s.bet;
        if (input::repeat(pad, BTN_LEFT | BTN_L)) { s.chipSel = std::max(0, s.chipSel - 1); audio::play(audio::SFX_NAV); }
        if (input::repeat(pad, BTN_RIGHT | BTN_R)) { s.chipSel = std::min(art::CHIP_COUNT - 1, s.chipSel + 1); audio::play(audio::SFX_NAV); }
        if (input::pressed(pad, BTN_A)) {
            i64 v = art::CHIP_VALUES[s.chipSel];
            if (v > avail) { audio::play(audio::SFX_ERROR); ui::toast("Недостаточно фишек", pal::redBright); }
            else if (s.bet + v > bj::MAX_BET) { audio::play(audio::SFX_ERROR); ui::toast("Максимальная ставка " + fmtMoney(bj::MAX_BET), pal::redBright); }
            else addChip(s, v);
        }
        if (input::pressed(pad, BTN_B) && !s.placed.empty()) {
            s.bet -= s.placed.back();
            s.placed.pop_back();
            audio::play(audio::SFX_CHIP, 0.6f, panFor(spotPos(s.spot).x), 0.9f);
        }
        if (input::pressed(pad, BTN_Y) && s.lastBet > 0 && s.bet == 0) {
            i64 want = std::min(s.lastBet, prof.balance);
            want -= want % bj::MIN_BET;
            if (want >= bj::MIN_BET) {
                i64 rest = want;
                for (int i = art::CHIP_COUNT - 1; i >= 0; i--)
                    while (rest >= art::CHIP_VALUES[i]) { addChip(s, art::CHIP_VALUES[i]); rest -= art::CHIP_VALUES[i]; }
            }
        }
        if (input::pressed(pad, BTN_X)) {
            audio::play(audio::SFX_KNOCK, 0.7f);
            turn_++;
            if (turn_ >= (int)seats_.size()) startDeal();
        }
    }

    void addChip(Seat& s, i64 v) {
        s.bet += v;
        s.placed.push_back(v);
        s.flying += v;
        FlyingChips f;
        f.amount = v;
        f.fx = 640 + (art::chipIndexFor(v) - 3) * 54.f; f.fy = 676;
        Pt p = spotPos(s.spot);
        f.tx = p.x; f.ty = p.y + 4;
        f.dur = 0.28f;
        f.scale = 0.64f;
        Seat* sp = &s;
        f.onArrive = [sp, v] { sp->flying -= v; };
        flying_.push_back(f);
        audio::play(audio::SFX_CHIP, 0.9f, panFor(p.x), 0.95f + rng().uniform(0, 0.12f));
    }

    void startDeal() {
        bool any = false;
        for (auto& s : seats_) any |= s.bet > 0;
        if (!any) {
            turn_ = 0;
            caption_.say("Нужна хотя бы одна ставка");
            audio::play(audio::SFX_ERROR);
            return;
        }
        // make sure in-flight chips have landed
        for (auto& f : flying_) { f.t = 1; f.delay = 0; f.update(0); }
        pruneDone(flying_);
        phase_ = DEALING;
        caption_.say("Ставки приняты");
        for (auto& s : seats_) {
            s.hands.clear();
            s.sprites.clear();
            s.label.clear();
            s.labelColor.clear();
            s.cur = 0;
            s.insurance = 0;
            s.staked = s.returned = 0;
            s.flying = 0;
            if (s.bet <= 0) continue;
            Profile& p = save::player(s.profile);
            p.balance -= s.bet;
            s.staked = s.bet;
            s.lastBet = s.bet;
            bj::Hand h;
            h.bet = s.bet;
            s.hands.push_back(h);
            s.sprites.push_back({});
            s.label.push_back("");
            s.labelColor.push_back(pal::ivory);
        }
        float g = stepGap();
        for (int round = 0; round < 2; round++) {
            for (auto& s : seats_) {
                if (!s.inRound()) continue;
                Seat* sp = &s;
                seq_.then(g, [this, sp] { dealTo(*sp, 0); });
            }
            bool up = round == 0;
            seq_.then(g, [this, up] { dealDealer(up); });
        }
        seq_.then(g + 0.2f, [this] { afterDeal(); });
    }

    // ---------------------------------------------------------------- insurance / peek
    void afterDeal() {
        int up = dealer_[0].rank;
        if (up == 14) {
            phase_ = INSURANCE;
            turn_ = -1;
            nextInsurance();
            caption_.say("Страховка?", 3);
        } else if (bj::cardValue(up) == 10) {
            peek();
        } else {
            beginPlay();
        }
    }

    void nextInsurance() {
        for (turn_++; turn_ < (int)seats_.size(); turn_++) {
            Seat& s = seats_[turn_];
            if (s.inRound() && save::player(s.profile).balance >= s.hands[0].bet / 2) return;
        }
        turn_ = (int)seats_.size();
        peek();
    }

    void updateInsurance() {
        if (turn_ >= (int)seats_.size()) return;
        Seat& s = seats_[turn_];
        int pad = padOf(s);
        if (input::pressed(pad, BTN_A)) {
            i64 ins = s.hands[0].bet / 2;
            save::player(s.profile).balance -= ins;
            s.insurance = ins;
            s.staked += ins;
            audio::play(audio::SFX_CHIP, 0.9f, panFor(spotPos(s.spot).x));
            nextInsurance();
        } else if (input::pressed(pad, BTN_B)) {
            audio::play(audio::SFX_KNOCK, 0.6f);
            nextInsurance();
        }
    }

    void peek() {
        phase_ = DEALING;
        caption_.say("Дилер проверяет закрытую карту…", 1.4f);
        if (dealerSprites_.size() >= 2) {
            CardSprite& h = sprites_[dealerSprites_[1]];
            h.moveTo(h.tx + 4, h.ty - 8, -4, 0, DEALER_SCALE, 0.25f);
        }
        seq_.then(0.9f, [this] {
            CardSprite& h = sprites_[dealerSprites_[1]];
            h.moveTo(dealerX(1, (int)dealer_.size()), DEALER_Y, 0, 0, DEALER_SCALE, 0.2f);
            if (bj::isBlackjack(dealer_)) {
                revealHole();
                caption_.say(quips::pick(quips::DEALER_BLACKJACK), 2.5f);
                for (auto& s : seats_)
                    if (s.insurance > 0) {
                        i64 win = s.insurance * 3;
                        save::player(s.profile).balance += win;
                        s.returned += win;
                        flyChips(s.insurance * 2, TRAY, insurancePos(s), 0.2f);
                        flyChips(win, insurancePos(s), platePos(s), 1.0f);
                        s.insurance = 0;
                    }
                for (auto& s : seats_)
                    for (auto& h : s.hands) h.done = true;
                seq_.then(1.2f, [this] { settle(); });
            } else {
                bool hadIns = false;
                for (auto& s : seats_)
                    if (s.insurance > 0) {
                        hadIns = true;
                        flyChips(s.insurance, insurancePos(s), TRAY, 0.1f);
                        s.insurance = 0;
                    }
                if (hadIns) audio::play(audio::SFX_CHIPS, 0.6f);
                caption_.say("Блэкджека у дилера нет", 1.6f);
                seq_.then(0.6f, [this] { beginPlay(); });
            }
        });
    }

    // ---------------------------------------------------------------- playing
    void beginPlay() {
        phase_ = PLAYING;
        turn_ = 0;
        advance();
    }

    // Moves to the next hand that needs a decision; deals second cards to split hands.
    void advance() {
        while (turn_ < (int)seats_.size()) {
            Seat& s = seats_[turn_];
            while (s.cur < (int)s.hands.size()) {
                bj::Hand& h = s.hands[s.cur];
                if (h.cards.size() == 1) {
                    // second card for a split hand
                    Seat* sp = &s;
                    int hi = s.cur;
                    seq_.then(stepGap(), [this, sp, hi] {
                        dealTo(*sp, hi);
                        bj::Hand& hh = sp->hands[hi];
                        if (hh.splitAces || hh.tot().value == 21) hh.done = true;
                    });
                    seq_.then(0.05f, [this] { advance(); });
                    return;
                }
                if (h.done || h.blackjack() || h.tot().value >= 21) {
                    if (h.blackjack() && !h.done) {
                        h.done = true;
                        setLabel(s, s.cur, "БЛЭКДЖЕК", pal::goldLight);
                        audio::play(audio::SFX_WIN, 0.7f, panFor(spotPos(s.spot).x));
                        Pt bp = spotPos(s.spot);
                        fx::burst(bp.x, bp.y - 60, 40, Color::hex(0xffd060));
                        fx::shockwave(bp.x, bp.y - 60, Color::hex(0xffd060), 150, 0.55f);
                        input::rumble(padOf(s), 0.5f, 0.3f);
                        audio::play(audio::SFX_APPLAUSE, 0.45f);
                        caption_.say(quips::pick(quips::PLAYER_BLACKJACK), 2.f);
                    }
                    h.done = true;
                    s.cur++;
                    continue;
                }
                return; // waiting for this player's decision
            }
            turn_++;
        }
        dealerTurn();
    }

    void setLabel(Seat& s, int h, const std::string& t, Color c) {
        if ((int)s.label.size() <= h) { s.label.resize(h + 1); s.labelColor.resize(h + 1, pal::ivory); }
        s.label[h] = t;
        s.labelColor[h] = c;
    }

    void updatePlaying() {
        if (turn_ >= (int)seats_.size()) return;
        Seat& s = seats_[turn_];
        if (s.cur >= (int)s.hands.size()) { advance(); return; }
        bj::Hand& h = s.hands[s.cur];
        Profile& p = save::player(s.profile);
        int pad = padOf(s);
        float px = spotPos(s.spot).x;
        if (input::pressed(pad, BTN_A)) {
            dealTo(s, s.cur);
            if (h.bust()) {
                h.done = true;
                setLabel(s, s.cur, "ПЕРЕБОР", pal::redBright);
                seq_.then(0.35f, [this, px, pad] {
                    audio::play(audio::SFX_LOSE, 0.6f, panFor(px));
                    audio::play(audio::SFX_GROAN, 0.3f, panFor(px));
                    if (rng().chance(0.6)) caption_.say(quips::pick(quips::PLAYER_BUST), 2.2f);
                    input::rumble(pad, 0.45f, 0.25f);
                    fx::shake(0.12f, false);
                });
            } else if (h.tot().value == 21) {
                h.done = true;
            }
            seq_.then(dealDur() + 0.05f, [this] { advance(); });
        } else if (input::pressed(pad, BTN_B)) {
            h.done = true;
            audio::play(audio::SFX_KNOCK, 0.7f, panFor(px));
            advance();
        } else if (input::pressed(pad, BTN_X)) {
            if (!bj::canDouble(h)) { audio::play(audio::SFX_ERROR); return; }
            if (p.balance < h.bet) { audio::play(audio::SFX_ERROR); ui::toast("Не хватает фишек на удвоение", pal::redBright); return; }
            p.balance -= h.bet;
            s.staked += h.bet;
            h.bet *= 2;
            h.doubled = true;
            audio::play(audio::SFX_CHIP, 0.9f, panFor(px));
            Seat* sp = &s;
            int hi = s.cur;
            seq_.then(0.25f, [this, sp, hi] {
                dealTo(*sp, hi, true);
                bj::Hand& hh = sp->hands[hi];
                hh.done = true;
                if (hh.bust()) {
                    setLabel(*sp, hi, "ПЕРЕБОР", pal::redBright);
                    audio::play(audio::SFX_LOSE, 0.6f);
                }
            });
            seq_.then(dealDur() + 0.1f, [this] { advance(); });
        } else if (input::pressed(pad, BTN_Y)) {
            if (!bj::canSplit(h, (int)s.hands.size())) { audio::play(audio::SFX_ERROR); return; }
            if (p.balance < h.bet) { audio::play(audio::SFX_ERROR); ui::toast("Не хватает фишек на сплит", pal::redBright); return; }
            p.balance -= h.bet;
            s.staked += h.bet;
            bj::Hand nh;
            nh.bet = h.bet;
            nh.fromSplit = true;
            nh.cards.push_back(h.cards[1]);
            bool aces = h.cards[0].rank == 14;
            nh.splitAces = aces;
            h.cards.pop_back();
            h.fromSplit = true;
            h.splitAces = aces;
            int moved = s.sprites[s.cur].back();
            s.sprites[s.cur].pop_back();
            s.hands.insert(s.hands.begin() + s.cur + 1, nh);
            s.sprites.insert(s.sprites.begin() + s.cur + 1, std::vector<int>{moved});
            s.label.insert(s.label.begin() + s.cur + 1, "");
            s.labelColor.insert(s.labelColor.begin() + s.cur + 1, pal::ivory);
            relayout(s);
            audio::play(audio::SFX_CHIP, 0.9f, panFor(px));
            seq_.then(0.3f, [this] { advance(); });
        } else if (input::pressed(pad, BTN_R | BTN_ZR)) {
            if (!bj::canSurrender(h, (int)s.hands.size())) { audio::play(audio::SFX_ERROR); return; }
            h.surrendered = true;
            h.done = true;
            setLabel(s, s.cur, "СДАЛСЯ", pal::muted);
            audio::play(audio::SFX_KNOCK, 0.6f);
            advance();
        }
    }

    // ---------------------------------------------------------------- dealer
    void revealHole() {
        if (dealerSprites_.size() < 2) return;
        CardSprite& h = sprites_[dealerSprites_[1]];
        h.moveTo(h.tx, h.ty, 0, 1, DEALER_SCALE, 0.35f);
        audio::play(audio::SFX_CARD_FLIP, 0.9f);
    }

    void dealerTurn() {
        phase_ = DEALER;
        turn_ = (int)seats_.size();
        bool live = false;
        for (auto& s : seats_)
            for (auto& h : s.hands) live |= !h.bust() && !h.surrendered && !h.blackjack();
        seq_.then(0.4f, [this] { revealHole(); });
        if (live) seq_.then(0.7f, [this] { dealerDraw(); });
        else seq_.then(0.9f, [this] { settle(); });
    }

    void dealerDraw() {
        if (bj::dealerHits(dealer_)) {
            dealDealer(true);
            seq_.then(save::data().settings.fastDeal ? 0.45f : 0.75f, [this] { dealerDraw(); });
        } else {
            int v = bj::total(dealer_).value;
            if (v > 21) {
                caption_.say(quips::pick(quips::DEALER_BUST), 2.5f);
                audio::play(audio::SFX_APPLAUSE, 0.35f);
            }
            else caption_.say("У дилера " + std::to_string(v), 2.5f);
            seq_.then(0.6f, [this] { settle(); });
        }
    }

    // ---------------------------------------------------------------- settlement
    void settle() {
        phase_ = SETTLING;
        bool anyWin = false, anyBJ = false, big = false;
        float delay = 0;
        int inRound = 0;
        for (auto& s : seats_) inRound += s.inRound();
        bool dealerBust = bj::total(dealer_).value > 21;
        for (auto& s : seats_) {
            if (!s.inRound()) continue;
            Pt sp = spotPos(s.spot), plate = platePos(s);
            int handWins = 0;
            for (size_t h = 0; h < s.hands.size(); h++) {
                bj::Outcome o;
                i64 ret = bj::settle(s.hands[h], dealer_, &o);
                i64 bet = s.hands[h].bet;
                s.returned += ret;
                save::player(s.profile).balance += ret;
                switch (o) {
                    case bj::BLACKJACK: setLabel(s, (int)h, "+" + fmtMoney(ret - bet), pal::goldLight); anyBJ = true; break;
                    case bj::WIN: setLabel(s, (int)h, "+" + fmtMoney(ret - bet), pal::greenBright); anyWin = true; break;
                    case bj::PUSH: setLabel(s, (int)h, "НИЧЬЯ", pal::ivory); break;
                    case bj::SURRENDER: setLabel(s, (int)h, "СДАЛСЯ", pal::muted); break;
                    case bj::BUST: setLabel(s, (int)h, "ПЕРЕБОР", pal::redBright); break;
                    default: setLabel(s, (int)h, "ПРОИГРЫШ", pal::redBright); break;
                }
                if (o == bj::BLACKJACK) trophy::unlock(s.profile, trophy::BLACKJACK);
                if (o == bj::WIN || o == bj::BLACKJACK) handWins++;
                if (o == bj::WIN && s.hands[h].cards.size() >= 5) trophy::unlock(s.profile, trophy::FIVE_CARDS);
                if (o == bj::WIN && s.hands[h].doubled) trophy::unlock(s.profile, trophy::DOUBLE_WIN);
                bj::Hand* hp = &s.hands[h];
                if (ret > bet) flyChips(ret - bet, TRAY, sp, delay, 0.5f);
                if (ret < bet) {
                    flyChips(bet - ret, sp, TRAY, delay, 0.5f);
                    hp->bet = ret; // what is left in the circle goes home later
                }
                if (ret > 0) {
                    timers_.add(delay + 0.75f, [this, hp, ret, sp, plate] {
                        hp->bet = 0;
                        flyChips(ret, sp, plate, 0, 0.45f);
                        audio::play(audio::SFX_CHIP, 0.6f, panFor(sp.x));
                    });
                }
                for (int id : s.sprites[h]) sprites_[id].dim = (o == bj::LOSE || o == bj::BUST);
                delay += 0.15f;
            }
            i64 net = s.returned - s.staked;
            save::recordResult(s.profile, net);
            if (net >= 1000 && net >= s.staked * 2) big = true;
            if (s.hands.size() >= 2 && handWins == (int)s.hands.size()) trophy::unlock(s.profile, trophy::SPLIT_WIN);
            if (dealerBust) trophy::unlock(s.profile, trophy::DEALER_BUST);
            trophy::unlock(s.profile, trophy::FIRST_GAME);
            if (inRound >= 3) trophy::unlock(s.profile, trophy::COMPANY);
            jackpot::feed(s.staked);
            trophy::checkBalance(s.profile);
        }
        save::store();
        audio::play(audio::SFX_CHIPS, 0.8f);
        if (anyBJ) {
            banner_.show("БЛЭКДЖЕК!", "", 2.0f);
            audio::play(audio::SFX_BIGWIN, 0.7f);
            audio::play(audio::SFX_CHEER, 0.5f);
            fx::coins(26, 0.6f);
            fx::shake(0.25f);
        } else if (big) {
            banner_.show("КРУПНЫЙ ВЫИГРЫШ", "", 2.0f);
            audio::play(audio::SFX_BIGWIN, 0.6f);
            audio::play(audio::SFX_CHEER, 0.55f);
            fx::coins(40, 0.8f);
            fx::shake(0.3f);
        } else if (anyWin) audio::play(audio::SFX_WIN, 0.7f);
        if (anyWin || anyBJ) audio::play(audio::SFX_KACHING, 0.45f);
        endTimer_ = 0;
        seq_.then(1.0f, [this] { phase_ = ROUND_END; });
    }

    void cleanup() {
        phase_ = DEALING;
        for (auto& sp : sprites_) {
            sp.dim = false;
            sp.moveTo(DISCARD.x, DISCARD.y, 25, 0, 0.7f, 0.45f, rng().uniform(0, 0.15f));
        }
        audio::play(audio::SFX_CARD_SLIDE, 0.6f, -0.6f, 0.9f);
        seq_.then(0.7f, [this] {
            timers_.clear();
            for (auto& s : seats_) for (auto& h : s.hands) h.bet = 0;
            sprites_.clear();
            dealerSprites_.clear();
            dealer_.clear();
            for (auto& s : seats_) {
                s.hands.clear();
                s.sprites.clear();
                s.label.clear();
                s.labelColor.clear();
                s.bet = 0;
                s.placed.clear();
                s.insurance = 0;
            }
            if (leaveAfter_) { app::go(SC_HALL); return; }
            if (shoe_.needsShuffle()) {
                shoe_.shuffle();
                caption_.say("Перетасовка колод", 2.0f);
                audio::play(audio::SFX_SHUFFLE, 0.8f);
                seq_.wait(1.2f);
            }
            seq_.then(0.1f, [this] {
                phase_ = BETTING;
                turn_ = 0;
                caption_.say("Делайте ваши ставки");
            });
        });
    }

    // ---------------------------------------------------------------- HUD
    void drawHud() {
        gfx::rectGrad(0, 628, SCREEN_W, 92, Color(0, 0, 0, 0), Color(0, 0, 0, 220));
        if (seq_.busy() || pause_.open) return;
        if (phase_ == BETTING && cur() >= 0) {
            Seat& s = seats_[cur()];
            Profile& p = save::player(s.profile);
            art::drawChipRack(s.chipSel, 640, 676, p.balance - s.bet);
            gfx::text("СТАВКА", 70, 662, F_SANS_BOLD, 14, pal::muted, -1);
            gfx::text(fmtMoney(s.bet), 70, 686, F_NUM, 24, pal::goldLight, -1);
            gfx::text(p.name, 260, 662, F_SANS_BOLD, 16, playerColor(p.color).scaled(1.3f), 0);
            gfx::text("делает ставку", 260, 684, F_SANS, 15, pal::cream, 0);
            std::vector<std::pair<u32, std::string>> h = {{BTN_A, "Фишка"}, {BTN_B, "Убрать"}};
            if (s.lastBet > 0 && s.bet == 0) h.push_back({BTN_Y, "Повторить"});
            h.push_back({BTN_X, s.bet > 0 ? "Готово" : "Пропустить"});
            ui::hints(h, 1262, 676, 1, 1.f, 16);
        } else if (phase_ == INSURANCE && cur() >= 0) {
            Seat& s = seats_[cur()];
            gfx::textShadow(save::player(s.profile).name + ": страховка за " + fmtMoney(s.hands[0].bet / 2) + "?", 70, 676,
                            F_SANS_BOLD, 22, pal::ivory, -1);
            ui::hints({{BTN_A, "Застраховать"}, {BTN_B, "Нет"}}, 1262, 676);
        } else if (phase_ == PLAYING && cur() >= 0) {
            Seat& s = seats_[cur()];
            if (s.cur >= (int)s.hands.size()) return;
            bj::Hand& h = s.hands[s.cur];
            Profile& p = save::player(s.profile);
            std::string who = p.name;
            if (s.hands.size() > 1) who += strf("  ·  рука %d из %d", s.cur + 1, (int)s.hands.size());
            gfx::textShadow(who, 70, 664, F_SANS_BOLD, 21, pal::ivory, -1);
            gfx::text("ваш ход", 70, 688, F_SANS, 15, pal::muted, -1);
            std::vector<std::pair<u32, std::string>> hs = {{BTN_A, "Ещё"}, {BTN_B, "Хватит"}};
            if (bj::canDouble(h) && p.balance >= h.bet) hs.push_back({BTN_X, "Удвоить"});
            if (bj::canSplit(h, (int)s.hands.size()) && p.balance >= h.bet) hs.push_back({BTN_Y, "Сплит"});
            if (bj::canSurrender(h, (int)s.hands.size())) hs.push_back({BTN_R, "Сдаться"});
            ui::hints(hs, 1262, 676);
        } else if (phase_ == ROUND_END) {
            ui::hints({{BTN_A, "Следующий раунд"}, {BTN_PLUS, "Меню"}}, 1262, 676);
        }
    }

    void openPause() {
        audio::play(audio::SFX_SELECT);
        pause_.rulesTitle = "ПРАВИЛА БЛЭКДЖЕКА";
        pause_.rulesText = kRules;
        std::vector<ui::MenuItem> items;
        items.push_back({"Продолжить", nullptr, nullptr, [this] { pause_.close(); }});
        items.push_back({"Правила", nullptr, nullptr, [this] { pause_.rules = true; }});
        items.push_back({"Быстрая раздача", [] { return std::string(save::data().settings.fastDeal ? "Вкл" : "Выкл"); },
                         [](int) { save::data().settings.fastDeal = !save::data().settings.fastDeal; save::store(); }, nullptr});
        items.push_back({"Покинуть стол", nullptr, nullptr, [this] {
                             pause_.close();
                             if (phase_ == BETTING) {
                                 save::store();
                                 app::go(SC_HALL);
                             } else {
                                 leaveAfter_ = true;
                                 ui::toast("Покинем стол после этого раунда");
                             }
                         }});
        pause_.show(items);
    }

    Tex table_;
    Shoe shoe_;
    Timers timers_;
    std::vector<Seat> seats_;
    std::vector<Card> dealer_;
    std::vector<int> dealerSprites_;
    std::vector<CardSprite> sprites_;
    std::vector<FlyingChips> flying_;
    Seq seq_;
    Phase phase_ = BETTING;
    int turn_ = 0;
    float t_ = 0, endTimer_ = 0;
    bool leaveAfter_ = false;
    art::Caption caption_;
    ui::Banner banner_;
    ui::PauseMenu pause_;
};

} // namespace

std::unique_ptr<Scene> makeBlackjackScene() { return std::make_unique<BlackjackScene>(); }
