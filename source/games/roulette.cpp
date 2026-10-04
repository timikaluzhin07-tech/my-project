// European roulette: a real-feeling wheel with a physical ball, full betting layout,
// coloured wheel chips for every player.
#include <map>

#include "../art/art.h"
#include "../art/backdrop.h"
#include "../art/svg.h"
#include "../art/tableart.h"
#include "../core/anim.h"
#include "../core/app.h"
#include "../core/audio.h"
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/save.h"
#include "../core/ui.h"
#include "roulette_logic.h"

namespace {

using namespace roulette;

constexpr float WR = 205;               // wheel radius (logical)
constexpr float WX = 250, WY = 300;     // wheel centre at rest
constexpr float ZX = 640, ZY = 352;     // wheel centre while the ball runs
constexpr float ZSCALE = 1.5f;
constexpr float R_TRACK = 0.835f, R_POCKET = 0.53f, R_ROTOR = 0.70f;
const i64 MAX_SPOT = 5000;
const float BANK_X = 860, BANK_Y = 40;

float pocketAngle(int idx) { return idx * 2 * PI / POCKETS - PI / 2; }
float wrapAngle(float a) {
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}

const char* kRules =
    "Европейская рулетка с одним зеро: 37 ячеек, от 0 до 36.\n\n"
    "Выплаты: номер — 35 к 1, сплит (2 номера) — 17 к 1, стрит (3) — 11 к 1, угол (4) — 8 к 1, "
    "сикслайн (6) — 5 к 1, колонна и дюжина — 2 к 1, красное/чёрное, чёт/нечет, 1–18/19–36 — 1 к 1. "
    "При выпадении зеро ставки на равные шансы проигрывают.\n\n"
    "Управление: крестовина — выбор поля (между номерами — сплиты и углы, под номерами — стриты), "
    "A — поставить фишку, B — снять фишку, L/R — номинал, Y — снять все свои ставки, − — повторить прошлые ставки, "
    "X — ставки сделаны (ход переходит к следующему игроку, затем крупье запускает шарик).\n\n"
    "У каждого игрока свой цвет фишек — как на настоящем столе.";

// ---------------------------------------------------------------------------
Image buildBowl() {
    float D = WR * 2, c = WR;
    std::string s = svg::open(D, D) + "<defs>" +
                    svg::radialU("rim", c, c, WR, {{0.88f, Color::hex(0x5a2e14)}, {0.95f, Color::hex(0x3a1b0b)}, {1, Color::hex(0x1e0d05)}}) +
                    svg::radialU("track", c, c, WR * 0.92f,
                                 {{0.74f, Color::hex(0x2c170b)}, {0.79f, Color::hex(0x6e4421)}, {0.9f, Color::hex(0xc29159)},
                                  {0.97f, Color::hex(0xdcb07a)}, {1, Color::hex(0x8a5a2c)}}) +
                    svg::linear("gold", 0, 0, 0, 1, {{0, Color::hex(0xfbe7a6)}, {0.5f, Color::hex(0xc99a3c)}, {1, Color::hex(0x8f6a22)}}) +
                    "</defs>";
    s += svg::circle(c, c, WR, svg::url("rim"));
    s += svg::circle(c, c, WR - 3, "none", svg::stroke("#e0bb62", 2.2f, 0.9f));
    s += svg::circle(c, c, WR * 0.92f, svg::url("track"));
    s += svg::circle(c, c, WR * 0.92f, "none", svg::stroke("#e0bb62", 1.4f, 0.8f));
    s += svg::circle(c, c, WR * 0.72f, "#140a05");
    // deflector diamonds
    for (int k = 0; k < 8; k++) {
        float a = k * PI / 4 + PI / 8;
        float x = c + std::cos(a) * WR * 0.785f, y = c + std::sin(a) * WR * 0.785f;
        float len = WR * 0.045f, wid = WR * 0.018f;
        float tx = -std::sin(a), ty = std::cos(a);
        bool along = k % 2 == 0;
        float ax = along ? tx : std::cos(a), ay = along ? ty : std::sin(a);
        float bx = -ay, by = ax;
        std::string d = "M" + svg::num(x + ax * len) + " " + svg::num(y + ay * len) + " L" + svg::num(x + bx * wid) + " " +
                        svg::num(y + by * wid) + " L" + svg::num(x - ax * len) + " " + svg::num(y - ay * len) + " L" +
                        svg::num(x - bx * wid) + " " + svg::num(y - by * wid) + " Z";
        s += svg::path(d, svg::url("gold"), svg::stroke("#6a4a14", 0.6f));
    }
    s += svg::close();
    Image img = gfx::rasterSvg(s);
    img.grain(0.04f, 3, 6);
    return img;
}

Image buildRotor() {
    float Rr = WR * R_ROTOR, D = Rr * 2, c = Rr;
    std::string s = svg::open(D, D) + "<defs>" +
                    svg::radialU("cone", c, c, WR * 0.47f, {{0.3f, Color::hex(0xb27a3e)}, {0.8f, Color::hex(0x6a3c18)}, {1, Color::hex(0x3a1e0a)}}) +
                    svg::radialU("tur", c - 6, c - 6, WR * 0.17f, {{0, Color::hex(0xfff2c0)}, {0.6f, Color::hex(0xd2a84a)}, {1, Color::hex(0x7a5418)}}) +
                    svg::linear("arm", 0, 0, 1, 1, {{0, Color::hex(0xfbe7a6)}, {0.5f, Color::hex(0xc99a3c)}, {1, Color::hex(0x8f6a22)}}) +
                    "</defs>";
    const int* order = wheelOrder();
    float rNumIn = WR * 0.585f, rNumOut = WR * 0.695f, rPocketIn = WR * 0.47f;
    for (int i = 0; i < POCKETS; i++) {
        float a0 = pocketAngle(i) - PI / POCKETS, a1 = pocketAngle(i) + PI / POCKETS;
        int n = order[i];
        const char* col = n == 0 ? "#127a45" : (isRed(n) ? "#b01a2c" : "#17151a");
        const char* deep = n == 0 ? "#0a4a28" : (isRed(n) ? "#5e0c16" : "#0b0a0d");
        s += svg::path(svg::sector(c, c, rNumIn, rNumOut, a0, a1), col);
        s += svg::path(svg::sector(c, c, rPocketIn, rNumIn, a0, a1), deep);
    }
    for (int i = 0; i < POCKETS; i++) {
        float a = pocketAngle(i) - PI / POCKETS;
        s += svg::line(c + std::cos(a) * rPocketIn, c + std::sin(a) * rPocketIn, c + std::cos(a) * rNumOut,
                       c + std::sin(a) * rNumOut, "#e3c46d", 1.1f, 0.95f);
    }
    s += svg::circle(c, c, rNumOut, "none", svg::stroke("#e3c46d", 1.6f));
    s += svg::circle(c, c, rNumIn, "none", svg::stroke("#e3c46d", 1.2f));
    s += svg::circle(c, c, rPocketIn, svg::url("cone"), svg::stroke("#e3c46d", 1.6f));
    for (int k = 0; k < 8; k++) {
        float a = k * PI / 4;
        s += svg::line(c + std::cos(a) * WR * 0.2f, c + std::sin(a) * WR * 0.2f, c + std::cos(a) * WR * 0.44f,
                       c + std::sin(a) * WR * 0.44f, "#2a1405", 1.2f, 0.4f);
    }
    // turret with the four-arm spinner
    float arm = WR * 0.33f, aw = WR * 0.028f;
    s += svg::rect(c - arm, c - aw, arm * 2, aw * 2, aw, svg::url("arm"), svg::stroke("#6a4a14", 0.6f));
    s += svg::rect(c - aw, c - arm, aw * 2, arm * 2, aw, svg::url("arm"), svg::stroke("#6a4a14", 0.6f));
    for (int k = 0; k < 4; k++) {
        float a = k * PI / 2;
        s += svg::circle(c + std::cos(a) * arm, c + std::sin(a) * arm, aw * 1.9f, svg::url("tur"), svg::stroke("#6a4a14", 0.6f));
    }
    s += svg::circle(c, c, WR * 0.12f, svg::url("tur"), svg::stroke("#6a4a14", 0.8f));
    s += svg::circle(c, c, WR * 0.05f, svg::url("arm"));
    s += svg::close();
    Image img = gfx::rasterSvg(s);
    float rText = WR * 0.64f;
    for (int i = 0; i < POCKETS; i++) {
        Image m = gfx::textMask(std::to_string(order[i]), F_NUM, 12.5f, TEX_SCALE);
        float a = pocketAngle(i);
        img.drawRotated(m, (c + std::cos(a) * rText) * TEX_SCALE, (c + std::sin(a) * rText) * TEX_SCALE, a + PI / 2,
                        Color::hex(0xf6efe0));
    }
    return img;
}

Image buildLayout(const Grid& g) {
    Image img = art::feltImage(SCREEN_W, SCREEN_H, Color::hex(0x1d7a4b), Color::hex(0x0b3b24), 31);
    std::string s = svg::open(SCREEN_W, SCREEN_H) + "<defs>" +
                    svg::linear("wood", 0, 0, 0, 1, {{0, Color::hex(0x5a3519)}, {1, Color::hex(0x2a160a)}}) +
                    svg::radialU("pit", WX, WY, WR + 26, {{0.85f, Color::hex(0x000000)}, {1, Color::hex(0x000000)}}) + "</defs>";
    // wooden rim along the top and bottom of the table
    s += svg::rect(0, 0, SCREEN_W, 18, 0, svg::url("wood"));
    s += svg::rect(0, 17, SCREEN_W, 1.6f, 0, "#b08a3a");
    std::string W = "#f2ead6";
    float lw = 1.8f;
    // zero
    float zx0 = g.x0 - g.zeroW;
    std::string zd = "M" + svg::num(g.x0) + " " + svg::num(g.y0) + " L" + svg::num(zx0 + 14) + " " + svg::num(g.y0) +
                     " L" + svg::num(zx0) + " " + svg::num(g.y0 + 1.5f * g.ch) + " L" + svg::num(zx0 + 14) + " " +
                     svg::num(g.bottom()) + " L" + svg::num(g.x0) + " " + svg::num(g.bottom()) + " Z";
    s += svg::path(zd, "#16834d", svg::stroke(W, lw, 0.9f));
    for (int c = 0; c < 12; c++)
        for (int r = 0; r < 3; r++) {
            int n = numberAt(c, r);
            float x = g.cellX(c), y = g.rowY(r);
            s += svg::rect(x, y, g.cw, g.ch, 0, "none", svg::stroke(W, lw, 0.9f));
            s += svg::ellipse(x + g.cw / 2, y + g.ch / 2, 19, 25, isRed(n) ? "#b51b2e" : "#141217", svg::stroke("#000", 0.6f, 0.3f));
        }
    for (int r = 0; r < 3; r++) s += svg::rect(g.right(), g.rowY(r), g.colW, g.ch, 0, "none", svg::stroke(W, lw, 0.9f));
    for (int d = 0; d < 3; d++) s += svg::rect(g.x0 + d * 4 * g.cw, g.bottom(), 4 * g.cw, g.dozenH, 0, "none", svg::stroke(W, lw, 0.9f));
    for (int k = 0; k < 6; k++)
        s += svg::rect(g.x0 + k * 2 * g.cw, g.bottom() + g.dozenH, 2 * g.cw, g.evenH, 0, "none", svg::stroke(W, lw, 0.9f));
    // red / black diamonds
    for (int k = 0; k < 2; k++) {
        float cx = g.x0 + (5 + 2 * k) * g.cw, cy = g.bottom() + g.dozenH + g.evenH / 2;
        s += svg::path("M" + svg::num(cx) + " " + svg::num(cy - 18) + " L" + svg::num(cx + 30) + " " + svg::num(cy) + " L" +
                           svg::num(cx) + " " + svg::num(cy + 18) + " L" + svg::num(cx - 30) + " " + svg::num(cy) + " Z",
                       k == 0 ? "#b51b2e" : "#141217", svg::stroke(W, 1.2f, 0.9f));
    }
    // history board frame
    s += svg::rect(870, 34, 384, 64, 8, "#0a0a0c", svg::stroke("#b08a3a", 1.2f) + " fill-opacity=\"0.75\"");
    s += svg::close();
    img.draw(gfx::rasterSvg(s), 0, 0);
    Color ink = Color::hex(0xf6efe0);
    art::stampText(img, "0", zx0 + g.zeroW / 2 + 4, g.y0 + 1.5f * g.ch, F_NUM, 30, ink, -90);
    for (int c = 0; c < 12; c++)
        for (int r = 0; r < 3; r++) {
            int n = numberAt(c, r);
            art::stampText(img, std::to_string(n), g.cellX(c) + g.cw / 2, g.rowY(r) + g.ch / 2, F_NUM, 22, ink);
        }
    for (int r = 0; r < 3; r++) art::stampText(img, "2 to 1", g.right() + g.colW / 2, g.rowY(r) + g.ch / 2, F_SANS_BOLD, 14, ink, -90);
    const char* dz[] = {"1st 12", "2nd 12", "3rd 12"};
    for (int d = 0; d < 3; d++) art::stampText(img, dz[d], g.x0 + (d * 4 + 2) * g.cw, g.bottom() + g.dozenH / 2, F_SERIF, 20, ink);
    const char* ev[] = {"1 to 18", "EVEN", "", "", "ODD", "19 to 36"};
    for (int k = 0; k < 6; k++)
        if (ev[k][0]) art::stampText(img, ev[k], g.x0 + (2 * k + 1) * g.cw, g.bottom() + g.dozenH + g.evenH / 2, F_SERIF, 18, ink);
    art::stampText(img, "ROULETTE", 1062, 120, F_TITLE, 13, Color::hex(0xe4c66c, 150));
    img.vignette(0.45f, 0.4f);
    return img;
}

// ---------------------------------------------------------------------------
enum Phase { BETTING, SPIN_START, SPINNING, RESULT, CLEANUP };

struct Player {
    int profile = 0;
    int order = 0;
    std::map<int, i64> bets, lastBets;
    int chipSel = 1;
    int cursor = 0;
    i64 returned = 0, staked = 0;
    i64 total() const { i64 t = 0; for (auto& kv : bets) t += kv.second; return t; }
};

class RouletteScene : public Scene {
public:
    RouletteScene() {
        spots_ = buildSpots(grid_);
        bg_ = gfx::upload(buildLayout(grid_));
        bowl_ = gfx::upload(buildBowl());
        rotor_ = gfx::upload(buildRotor());
        art::ensureBuilt();
        int redSpot = 0;
        for (int i = 0; i < (int)spots_.size(); i++)
            if (spots_[i].kind == RED) redSpot = i;
        auto act = save::active();
        for (size_t k = 0; k < act.size(); k++) {
            Player p;
            p.profile = act[k];
            p.order = (int)k;
            p.cursor = redSpot;
            players_.push_back(p);
        }
        rotorAngle_ = rng().uniform(0, 2 * PI);
        caption_.say("Делайте ваши ставки");
        phase_ = BETTING;
    }

    ~RouletteScene() override {
        if (ballLoop_) audio::stopLoop(ballLoop_, 0.05f);
    }

    void update(float dt) override {
        time_ += dt;
        caption_.update(dt);
        banner_.update(dt);
        for (auto& f : flying_) f.update(dt);
        pruneDone(flying_);
        timers_.update(dt);
        updateWheel(dt);
        float zt = (phase_ == SPINNING || (phase_ == RESULT && resultT_ < 1.6f)) ? 1.f : 0.f;
        zoom_ = approach(zoom_, zt, 3.2f, dt);
        cursorAnim_ = approach(cursorAnim_, 1.f, 14, dt);
        if (pause_.update(dt)) return;
        if (input::pressed(ANY_PAD, BTN_PLUS)) { openPause(); return; }
        seq_.update(dt);
        if (phase_ == RESULT) resultT_ += dt;
        if (seq_.busy()) return;
        if (phase_ == BETTING) updateBetting();
    }

    void render() override {
        gfx::draw(bg_, 0, 0);
        drawHistory();
        drawHover();
        drawBets();
        drawDolly();
        for (auto& f : flying_) f.draw();
        // dim the layout while the wheel is in focus
        float ze = ease::inOutCubic(zoom_);
        if (ze > 0.01f) gfx::dim(0.55f * ze);
        drawWheel(ze);
        drawPlates(1 - ze);
        drawResultPlaque(ze);
        drawHud();
        caption_.render(640, 66);
        banner_.render(640, 360, 56);
        pause_.render();
    }

private:
    // ====================================================================== betting
    Player& cur() { return players_[std::min(turn_, (int)players_.size() - 1)]; }

    void updateBetting() {
        if (turn_ >= (int)players_.size()) { startSpin(); return; }
        Player& p = cur();
        Profile& prof = save::player(p.profile);
        int pad = input::padForSeat(p.order);
        int before = p.cursor;
        if (input::repeat(pad, BTN_LEFT)) p.cursor = navigate(spots_, p.cursor, -1, 0);
        if (input::repeat(pad, BTN_RIGHT)) p.cursor = navigate(spots_, p.cursor, 1, 0);
        if (input::repeat(pad, BTN_UP)) p.cursor = navigate(spots_, p.cursor, 0, -1);
        if (input::repeat(pad, BTN_DOWN)) p.cursor = navigate(spots_, p.cursor, 0, 1);
        if (p.cursor != before) {
            audio::play(audio::SFX_NAV, 0.5f, (spots_[p.cursor].x - 640) / 700.f);
            cursorFrom_ = {spots_[before].x, spots_[before].y};
            cursorAnim_ = 0;
        }
        if (input::repeat(pad, BTN_L)) { p.chipSel = std::max(0, p.chipSel - 1); audio::play(audio::SFX_NAV); }
        if (input::repeat(pad, BTN_R)) { p.chipSel = std::min(art::CHIP_COUNT - 1, p.chipSel + 1); audio::play(audio::SFX_NAV); }
        i64 avail = prof.balance - p.total();
        const Spot& sp = spots_[p.cursor];
        if (input::pressed(pad, BTN_A)) {
            i64 v = art::CHIP_VALUES[p.chipSel];
            i64 have = p.bets.count(p.cursor) ? p.bets[p.cursor] : 0;
            if (v > avail) { audio::play(audio::SFX_ERROR); ui::toast("Недостаточно фишек", pal::redBright); }
            else if (have + v > MAX_SPOT) { audio::play(audio::SFX_ERROR); ui::toast("Лимит поля " + fmtMoney(MAX_SPOT), pal::redBright); }
            else {
                p.bets[p.cursor] = have + v;
                audio::play(audio::SFX_CHIP, 0.9f, (sp.x - 640) / 700.f, 0.95f + rng().uniform(0, 0.12f));
            }
        }
        if (input::pressed(pad, BTN_B) && p.bets.count(p.cursor)) {
            i64 v = art::CHIP_VALUES[p.chipSel];
            i64& have = p.bets[p.cursor];
            have -= std::min(have, v);
            if (have <= 0) p.bets.erase(p.cursor);
            audio::play(audio::SFX_CHIP, 0.6f, 0, 0.85f);
        }
        if (input::pressed(pad, BTN_Y) && !p.bets.empty()) {
            p.bets.clear();
            audio::play(audio::SFX_CHIPS, 0.5f);
        }
        if (input::pressed(pad, BTN_MINUS) && !p.lastBets.empty()) {
            i64 need = 0;
            for (auto& kv : p.lastBets) need += kv.second;
            if (need + p.total() > prof.balance) { audio::play(audio::SFX_ERROR); ui::toast("Не хватает фишек на повтор", pal::redBright); }
            else {
                for (auto& kv : p.lastBets) p.bets[kv.first] = std::min(MAX_SPOT, p.bets[kv.first] + kv.second);
                audio::play(audio::SFX_CHIPS, 0.8f);
            }
        }
        if (input::pressed(pad, BTN_X)) {
            audio::play(audio::SFX_KNOCK, 0.7f);
            turn_++;
            if (turn_ < (int)players_.size()) {
                cursorAnim_ = 1;
                caption_.say("Ставит " + save::player(players_[turn_].profile).name, 1.6f);
            }
        }
    }

    // ====================================================================== wheel
    void startSpin() {
        bool any = false;
        for (auto& p : players_) any |= !p.bets.empty();
        if (!any) {
            turn_ = 0;
            caption_.say("Сделайте хотя бы одну ставку");
            audio::play(audio::SFX_ERROR);
            return;
        }
        phase_ = SPIN_START;
        for (auto& p : players_) {
            p.staked = p.total();
            p.returned = 0;
            save::player(p.profile).balance -= p.staked;
            if (!p.bets.empty()) p.lastBets = p.bets;
        }
        caption_.say("Ставки сделаны. Ставок больше нет!", 2.6f);
        result_ = rng().range(0, 36);
        seq_.then(0.8f, [this] {
            phase_ = SPINNING;
            rotorTarget_ = 1.25f;
            ballPhase_ = 1;
            ballAngle_ = rotorAngle_ + rng().uniform(0, 2 * PI);
            ballSpeed_ = -rng().uniform(11.5f, 13.5f);
            ballR_ = R_TRACK;
            ballLoop_ = audio::loop(audio::SFX_BALL_LOOP, 0.5f, 1.1f);
            audio::play(audio::SFX_WHOOSH, 0.4f);
        });
    }

    void updateWheel(float dt) {
        rotorSpeed_ = approach(rotorSpeed_, rotorTarget_, 0.6f, dt);
        rotorAngle_ += rotorSpeed_ * dt;
        if (ballPhase_ == 1) {
            ballSpeed_ *= std::exp(-0.27f * dt);
            ballAngle_ += ballSpeed_ * dt;
            float sp = std::fabs(ballSpeed_);
            audio::setLoop(ballLoop_, 0.18f + 0.03f * sp, 0.75f + 0.035f * sp);
            if (sp < 3.4f) beginDrop();
        } else if (ballPhase_ == 2) {
            dropT_ += dt;
            float u = clamp01(dropT_ / dropDur_);
            float e = ease::outCubic(u) + 0.04f * std::sin(u * PI * 3) * (1 - u) * (1 - u);
            float phi = phi0_ - delta_ * std::min(1.f, e);
            ballAngle_ = rotorAngle_ + phi;
            // radius: falls off the track, bounces on the frets, settles in the pocket
            float fall = clamp01(u / 0.42f);
            float r = lerp(R_TRACK, R_POCKET, ease::inQuad(fall));
            if (u > 0.42f) {
                float b = (u - 0.42f) / 0.58f;
                r = R_POCKET + 0.075f * std::fabs(std::sin(b * PI * 3.5f)) * (1 - b) * (1 - b);
                int bounce = (int)(b * 3.5f);
                if (bounce != lastBounce_ && bounce < 4) {
                    lastBounce_ = bounce;
                    audio::play(audio::SFX_BALL_CLACK, 0.8f * (1 - b), 0, 0.9f + rng().uniform(0, 0.2f));
                }
            }
            ballR_ = r;
            if (u >= 1) ballSettled();
        } else if (ballPhase_ == 3) {
            ballAngle_ = rotorAngle_ + pocketAngle(pocketIndex(result_));
            ballR_ = R_POCKET;
        }
    }

    void beginDrop() {
        ballPhase_ = 2;
        dropT_ = 0;
        dropDur_ = 2.4f;
        lastBounce_ = -1;
        audio::stopLoop(ballLoop_, 0.4f);
        ballLoop_ = 0;
        audio::play(audio::SFX_BALL_CLACK, 0.9f);
        phi0_ = wrapAngle(ballAngle_ - rotorAngle_);
        float target = pocketAngle(pocketIndex(result_));
        float d = std::fmod(phi0_ - target, 2 * PI);
        if (d < 0) d += 2 * PI;
        while (d < 3.0f) d += 2 * PI;
        delta_ = d; // ball keeps travelling against the rotor
        rotorTarget_ = 0.45f;
    }

    void ballSettled() {
        ballPhase_ = 3;
        phase_ = RESULT;
        resultT_ = 0;
        audio::play(audio::SFX_BALL_CLACK, 0.5f, 0, 0.8f);
        history_.insert(history_.begin(), result_);
        if (history_.size() > 12) history_.pop_back();
        caption_.say(std::to_string(result_) + ", " + std::string(colorName(result_)), 3);
        audio::play(audio::SFX_KNOCK, 0.9f);
        seq_.wait(1.9f);
        seq_.then(0.01f, [this] { payout(); });
    }

    void payout() {
        float delay = 0;
        bool anyWin = false;
        i64 biggest = 0;
        for (auto& p : players_) {
            int plate = plateIndex(p);
            for (auto& kv : p.bets) {
                const Spot& sp = spots_[kv.first];
                i64 ret = roulette::settle(sp, kv.second, result_);
                p.returned += ret;
                if (ret > 0) {
                    anyWin = true;
                    FlyingChips f = chipFlight(p, BANK_X, BANK_Y, sp.x, sp.y, 0.3f + delay, 3);
                    flying_.push_back(f);
                    FlyingChips back = chipFlight(p, sp.x, sp.y, plateX(plate), 600, 1.2f + delay, 4);
                    flying_.push_back(back);
                } else {
                    FlyingChips f = chipFlight(p, sp.x, sp.y, BANK_X, BANK_Y, 0.05f + delay * 0.3f, 2);
                    flying_.push_back(f);
                }
                delay += 0.05f;
            }
            i64 net = p.returned - p.staked;
            save::player(p.profile).balance += p.returned;
            if (p.staked > 0) save::recordResult(p.profile, net);
            biggest = std::max(biggest, net);
            p.bets.clear();
        }
        save::store();
        audio::play(audio::SFX_CHIPS, 0.8f);
        if (biggest >= 3000) { audio::play(audio::SFX_BIGWIN, 0.7f); banner_.show("КРУПНЫЙ ВЫИГРЫШ", "+" + fmtMoney(biggest), 2.2f); }
        else if (anyWin) audio::play(audio::SFX_WIN, 0.8f);
        phase_ = CLEANUP;
        showNet_ = true;
        seq_.wait(2.6f);
        seq_.then(0.01f, [this] {
            if (leaveAfter_) { app::go(SC_HALL); return; }
            phase_ = BETTING;
            turn_ = 0;
            showNet_ = false;
            ballPhase_ = 0;
            rotorTarget_ = 0.35f;
            caption_.say("Делайте ваши ставки");
        });
    }

    FlyingChips chipFlight(const Player& p, float fx, float fy, float tx, float ty, float delay, int count) {
        FlyingChips f;
        f.fx = fx; f.fy = fy; f.tx = tx; f.ty = ty;
        f.delay = delay;
        f.dur = 0.5f;
        f.scale = 0.6f;
        f.playerColor = save::player(p.profile).color;
        f.count = count;
        return f;
    }

    // ====================================================================== drawing
    void drawWheel(float ze) {
        float cx = lerp(WX, ZX, ze), cy = lerp(WY, ZY, ze), sc = lerp(1.f, ZSCALE, ze);
        gfx::glow(cx + 5, cy + 10, (WR + 30) * sc, pal::black, 0.75f, false);
        gfx::drawCentered(bowl_, cx, cy, sc);
        gfx::drawCentered(rotor_, cx, cy, sc, rotorAngle_ * 180 / PI);
        // warm highlight on the polished track
        gfx::glowEllipse(cx - WR * 0.3f * sc, cy - WR * 0.35f * sc, WR * 0.5f * sc, WR * 0.35f * sc, Color::hex(0xfff2d0), 0.12f);
        if (ballPhase_ > 0) {
            float r = ballR_ * WR * sc;
            float bx = cx + std::cos(ballAngle_) * r, by = cy + std::sin(ballAngle_) * r;
            float br = WR * 0.032f * sc;
            gfx::circle(bx + br * 0.35f, by + br * 0.45f, br * 1.05f, Color(0, 0, 0, 120));
            gfx::circle(bx, by, br, Color::hex(0xf4f2ee));
            gfx::circle(bx - br * 0.32f, by - br * 0.35f, br * 0.38f, Color(255, 255, 255, 230));
            if (ballPhase_ == 1) {
                // motion streak
                float tr = ballSpeed_ > 0 ? -1.f : 1.f;
                for (int k = 1; k <= 4; k++) {
                    float a = ballAngle_ + tr * k * 0.03f * std::fabs(ballSpeed_) / 10.f;
                    gfx::circle(cx + std::cos(a) * r, cy + std::sin(a) * r, br * (1 - k * 0.18f), Color(255, 255, 255, (uint8_t)(70 - k * 15)));
                }
            }
        }
        if (phase_ == RESULT || phase_ == CLEANUP) {
            float a = rotorAngle_ + pocketAngle(pocketIndex(result_));
            float r = WR * 0.64f * sc;
            gfx::glow(cx + std::cos(a) * r, cy + std::sin(a) * r, 26 * sc, pal::goldLight, 0.5f + 0.3f * std::sin(time_ * 6));
        }
    }

    void drawHistory() {
        gfx::text("ПОСЛЕДНИЕ", 884, 48, F_SANS_BOLD, 11, pal::muted, -1);
        for (size_t i = 0; i < history_.size(); i++) {
            int n = history_[i];
            float x = 896 + i * 30.f, y = 72;
            Color c = n == 0 ? Color::hex(0x127a45) : (isRed(n) ? Color::hex(0xb51b2e) : Color::hex(0x1b1a1f));
            gfx::circle(x, y, 13, c);
            gfx::ring(x, y, 13, 1, i == 0 ? pal::goldLight : Color(255, 255, 255, 60));
            gfx::text(std::to_string(n), x, y, F_NUM, 14, pal::ivory, 0);
        }
        if (history_.empty()) gfx::text("—", 1062, 72, F_SANS, 16, pal::dim, 0);
    }

    void drawHover() {
        if (phase_ != BETTING || seq_.busy() || turn_ >= (int)players_.size()) return;
        const Player& p = cur();
        const Spot& sp = spots_[p.cursor];
        Color pc = playerColor(save::player(p.profile).color);
        // light up every number the bet covers
        for (int n : sp.numbers) {
            float x, y, w, h;
            if (n == 0) { x = grid_.x0 - grid_.zeroW; y = grid_.y0; w = grid_.zeroW; h = grid_.ch * 3; }
            else {
                int c = (n - 1) / 3, r = 2 - (n - 1) % 3;
                x = grid_.cellX(c); y = grid_.rowY(r); w = grid_.cw; h = grid_.ch;
            }
            gfx::rect(x + 1, y + 1, w - 2, h - 2, Color(255, 225, 140, 46));
        }
        if (sp.kind >= COLUMN) {
            // also outline the outside box itself
            gfx::glow(sp.x, sp.y, 50, pal::gold, 0.25f);
        }
        float e = ease::outCubic(cursorAnim_);
        float cx = lerp(cursorFrom_.x, sp.x, e), cy = lerp(cursorFrom_.y, sp.y, e);
        float pulse = 0.5f + 0.5f * std::sin(time_ * 6);
        gfx::glow(cx, cy, 30, pc, 0.45f);
        gfx::ring(cx, cy, 15 + pulse * 2, 2.5f, pc.scaled(1.3f));
        gfx::ring(cx, cy, 19 + pulse * 2, 1.f, pal::white.alpha(0.6f));
    }

    void drawBets() {
        int n = (int)players_.size();
        for (int k = 0; k < n; k++) {
            const Player& p = players_[k];
            int col = save::player(p.profile).color;
            for (auto& kv : p.bets) {
                const Spot& sp = spots_[kv.first];
                int chips = std::clamp((int)(std::log2((double)kv.second / 10.0) + 1.5), 1, 8);
                float ox = (k - (n - 1) / 2.f) * 7, oy = -k * 3.f;
                art::drawPlayerChipStack(col, chips, sp.x + ox, sp.y + oy, 0.58f);
            }
        }
    }

    void drawDolly() {
        if (!(phase_ == RESULT || phase_ == CLEANUP) || resultT_ < 1.0f) return;
        const Spot* s = nullptr;
        for (auto& sp : spots_)
            if (sp.kind == STRAIGHT && sp.numbers[0] == result_) s = &sp;
        if (!s) return;
        float drop = ease::outBounce(clamp01((resultT_ - 1.0f) / 0.5f));
        float y = s->y - 26 * (1 - drop);
        gfx::glow(s->x, s->y, 44, pal::goldLight, 0.4f);
        gfx::ellipse(s->x + 2, s->y + 4, 13, 7, Color(0, 0, 0, 110));
        gfx::roundRect(s->x - 9, y - 26, 18, 26, 6, Color::hex(0xe9e4da));
        gfx::ellipse(s->x, y - 26, 9, 4, Color::hex(0xfdfbf6));
        gfx::circle(s->x, y - 32, 6, pal::gold);
        gfx::circle(s->x - 2, y - 34, 2, pal::white);
    }

    int plateIndex(const Player& p) const { return (int)(&p - &players_[0]); }
    float plateX(int i) const {
        int n = (int)players_.size();
        float w = 186;
        return 640 - (n - 1) * w / 2 + i * w;
    }

    void drawResultPlaque(float ze) {
        if (!(phase_ == RESULT) || ze < 0.05f) return;
        float a = clamp01(resultT_ * 3) * ze;
        float x = 1100, y = 340;
        ui::panel(x - 110, y - 120, 220, 240, a);
        Color c = result_ == 0 ? Color::hex(0x127a45) : (isRed(result_) ? Color::hex(0xb51b2e) : Color::hex(0x1b1a1f));
        float pop = ease::outBack(clamp01(resultT_ / 0.4f), 2.5f);
        gfx::glow(x, y - 18, 90 * pop, c.scaled(1.6f), 0.4f * a);
        gfx::circle(x, y - 18, 62 * pop, c.alpha(a));
        gfx::ring(x, y - 18, 62 * pop, 3, pal::gold.alpha(a));
        gfx::text(std::to_string(result_), x, y - 18, F_NUM, 64 * pop, pal::ivory, 0, a);
        gfx::textGold(colorName(result_), x, y + 78, F_TITLE, 22, 0, a);
    }

    void drawPlates(float alpha) {
        if (alpha <= 0.01f) return;
        for (size_t i = 0; i < players_.size(); i++) {
            const Player& p = players_[i];
            bool active = phase_ == BETTING && (int)i == turn_ && !seq_.busy();
            std::string l2;
            Color c = pal::goldLight;
            if (phase_ == BETTING) {
                i64 tot = p.total();
                l2 = fmtMoney(save::player(p.profile).balance - tot) + (tot > 0 ? "  · ставка " + fmtMoney(tot) : "");
            } else if (showNet_ && p.staked > 0) {
                i64 net = p.returned - p.staked;
                l2 = (net > 0 ? "+" : "") + fmtMoney(net);
                c = net > 0 ? pal::greenBright : (net < 0 ? pal::redBright : pal::ivory);
            }
            art::seatPlate(p.profile, plateX((int)i), 600, active, 0.5f + 0.5f * std::sin(time_ * 5), l2, c, alpha, 178);
        }
    }

    void drawHud() {
        gfx::rectGrad(0, 636, SCREEN_W, 84, Color(0, 0, 0, 0), Color(0, 0, 0, 220));
        if (pause_.open) return;
        if (phase_ == BETTING && !seq_.busy() && turn_ < (int)players_.size()) {
            Player& p = cur();
            Profile& prof = save::player(p.profile);
            const Spot& sp = spots_[p.cursor];
            // info strip under the layout
            float y = 512;
            gfx::roundRect(470, y - 18, 784, 36, 18, Color(0, 0, 0, 120));
            gfx::text(prof.name, 490, y, F_SANS_BOLD, 18, playerColor(prof.color).scaled(1.35f), -1);
            float nx = 500 + gfx::textWidth(prof.name, F_SANS_BOLD, 18);
            gfx::text("·  " + sp.label + "  ·  " + std::to_string(sp.payout) + " к 1", nx, y, F_SANS, 18, pal::ivory, -1);
            i64 mine = p.bets.count(p.cursor) ? p.bets.at(p.cursor) : 0;
            if (mine > 0) gfx::text("на поле " + fmtMoney(mine), 1238, y, F_NUM, 17, pal::goldLight, 1);
            art::drawChipRack(p.chipSel, 360, 678, prof.balance - p.total());
            std::vector<std::pair<u32, std::string>> h = {{BTN_A, "Ставка"}, {BTN_B, "Снять"}, {BTN_L, "Номинал"}};
            if (!p.lastBets.empty() && p.bets.empty()) h.push_back({BTN_MINUS, "Повтор"});
            h.push_back({BTN_X, p.bets.empty() ? "Пас" : "Готово"});
            ui::hints(h, 1262, 678, 1, 1.f, 16);
        } else if (phase_ == SPINNING) {
            gfx::text("Шарик в игре…", 640, 690, F_SANS, 18, pal::cream, 0);
        }
    }

    void openPause() {
        audio::play(audio::SFX_SELECT);
        pause_.rulesTitle = "ПРАВИЛА РУЛЕТКИ";
        pause_.rulesText = kRules;
        std::vector<ui::MenuItem> items;
        items.push_back({"Продолжить", nullptr, nullptr, [this] { pause_.close(); }});
        items.push_back({"Правила", nullptr, nullptr, [this] { pause_.rules = true; }});
        items.push_back({"Покинуть стол", nullptr, nullptr, [this] {
                             pause_.close();
                             if (phase_ == BETTING) app::go(SC_HALL);
                             else {
                                 leaveAfter_ = true;
                                 ui::toast("Уйдём после этого вращения");
                             }
                         }});
        pause_.show(items);
    }

    Grid grid_;
    std::vector<Spot> spots_;
    Tex bg_, bowl_, rotor_;
    std::vector<Player> players_;
    std::vector<FlyingChips> flying_;
    std::vector<int> history_;
    Phase phase_ = BETTING;
    int turn_ = 0;
    float time_ = 0;
    // wheel
    float rotorAngle_ = 0, rotorSpeed_ = 0.35f, rotorTarget_ = 0.35f;
    int ballPhase_ = 0;
    float ballAngle_ = 0, ballSpeed_ = 0, ballR_ = R_TRACK;
    float dropT_ = 0, dropDur_ = 2.4f, phi0_ = 0, delta_ = 0;
    int lastBounce_ = -1;
    int ballLoop_ = 0;
    int result_ = 0;
    float resultT_ = 0;
    float zoom_ = 0;
    bool showNet_ = false;
    bool leaveAfter_ = false;
    struct { float x = 0, y = 0; } cursorFrom_;
    float cursorAnim_ = 1;
    Seq seq_;
    Timers timers_;
    art::Caption caption_;
    ui::Banner banner_;
    ui::PauseMenu pause_;
};

} // namespace

std::unique_ptr<Scene> makeRouletteScene() { return std::make_unique<RouletteScene>(); }
