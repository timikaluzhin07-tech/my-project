// "Гнев Зевса" — the Zeus slot machine scene.
#include "../art/art.h"
#include "../art/slotart.h"
#include "../core/anim.h"
#include "../core/app.h"
#include "../core/audio.h"
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/save.h"
#include "../core/ui.h"
#include "slot_logic.h"

namespace {

using namespace slot;
using slotart::CELL;

constexpr float GX = 640 - CELL * COLS / 2, GY = 128; // grid top-left
constexpr float MEDX = 1080, MEDY = 268, MEDS = 0.86f;
const i64 BETS[] = {20, 40, 100, 200, 400, 1000, 2000, 5000};
constexpr int BET_LEVELS = 8;

float cellX(int c) { return GX + c * CELL + CELL / 2; }
float cellY(float r) { return GY + r * CELL + CELL / 2; }

const char* kRules =
    "Поле 6×5. Платят 8 и более одинаковых символов в любом месте экрана. "
    "Выигравшие символы разбиваются, сверху падают новые — каскады продолжаются, пока появляются выигрыши.\n\n"
    "Сферы Зевса (×2 … ×500) остаются на поле до конца каскадов. Если вращение принесло выигрыш, "
    "все множители складываются и умножают его.\n\n"
    "4 и более молний Зевса в любом месте дают 15 бесплатных вращений (и 3×, 5× или 100× ставки за 4, 5 или 6 молний). "
    "В бесплатных вращениях множители копятся в общий множитель бонуса; 3 молнии добавляют ещё 5 вращений.\n\n"
    "Выплаты (× ставки за 8–9 / 10–11 / 12+): корона 10/25/50, часы 2.5/10/25, перстень 2/5/15, кубок 1.5/2/12, "
    "рубин 1/1.5/10, аметист 0.8/1.2/8, топаз 0.5/1/5, изумруд 0.4/0.9/4, сапфир 0.25/0.75/2.\n\n"
    "Теоретическая отдача ~96%. Максимальный выигрыш ×5000 ставки. Купить бонус — ×145 ставки.";

struct SymInst {
    Cell cell;
    float y = 0, ty = 0, vy = 0, delay = 0;
    bool landed = true;
    bool hit = false;
    float glow = 0;
    float pop = 0; // orb arrival pulse
};

struct Dying {
    Cell cell;
    float x, y, t = 0;
};

struct Particle {
    float x, y, vx, vy, life, max, size, rot, vr;
    Color c;
    int kind; // 0 spark (additive), 1 shard, 2 coin
};

struct Bolt {
    float x1, y1, x2, y2, t, dur;
    uint32_t seed;
    float width;
};

struct Floater {
    std::string text;
    float x, y, t = 0;
    Color c;
};

enum Phase { IDLE, SPINNING, BIGWIN, FS_INTRO, FS_OUTRO };

class SlotsScene : public Scene {
public:
    SlotsScene() {
        slotart::build(a_);
        art::ensureBuilt();
        auto act = save::active();
        for (size_t k = 0; k < act.size(); k++) players_.push_back({act[k], (int)k, save::data().settings.slotBet});
        // a calm starting screen
        Engine e;
        Spin s = e.play(BETS[0], rng());
        for (int c = 0; c < COLS; c++)
            for (int r = 0; r < ROWS; r++) {
                grid_[c][r].cell = s.initial[c][r].s == SCATTER || s.initial[c][r].s == ORB ? Cell{(Sym_t)(c % 5), 0}
                                                                                            : s.initial[c][r];
                grid_[c][r].y = grid_[c][r].ty = cellY((float)r);
            }
        turbo_ = save::data().settings.slotTurbo;
        nextFlash_ = 3;
    }

    audio::Music music() const override { return audio::MUS_OLYMPUS; }
    bool ambience() const override { return false; }

    void update(float dt) override {
        time_ += dt;
        float sdt = turbo_ ? dt * 1.7f : dt;
        banner_.update(dt);
        updateSymbols(sdt);
        updateFx(dt);
        winShown_ = approachMoney(winShown_, winTarget_, dt);
        if (dialog_.update(dt)) return;
        if (pause_.update(dt)) return;
        if (input::pressed(ANY_PAD, BTN_PLUS) && phase_ == IDLE) { openPause(); return; }
        seq_.update(sdt);
        switch (phase_) {
            case IDLE: updateIdle(dt); break;
            case BIGWIN: updateBigWin(dt); break;
            case FS_INTRO:
            case FS_OUTRO:
                introT_ += dt;
                if (introT_ > 0.8f && (input::pressed(ANY_PAD, BTN_A) || introT_ > (auto_ ? 3.f : 8.f))) {
                    if (phase_ == FS_INTRO) { phase_ = SPINNING; seq_.then(0.3f, [this] { startSpin(false); }); }
                    else endFreeSpins();
                }
                break;
            default:
                if (auto_ && input::pressed(ANY_PAD, BTN_Y | BTN_B)) { auto_ = false; ui::toast("Автоигра остановлена"); }
                break;
        }
    }

    void render() override {
        gfx::draw(a_.background, 0, 0);
        // storm flashes in the sky
        if (flash_ > 0) gfx::dim(flash_ * 0.35f, Color(200, 210, 255));
        for (auto& b : skyBolts_) drawBolt(b);
        drawMedallion();
        drawGrid();
        drawSidePanel();
        for (auto& b : bolts_) drawBolt(b);
        drawParticles(false);
        for (auto& f : floaters_) {
            float a = 1 - clamp01((f.t - 0.9f) / 0.5f);
            gfx::textShadow(f.text, f.x, f.y - f.t * 30, F_NUM, 26, f.c, 0, a);
        }
        gfx::textGold(free_ ? "БЕСПЛАТНЫЕ ВРАЩЕНИЯ" : "ГНЕВ ЗЕВСА", 640, 82, F_TITLE, free_ ? 30 : 38);
        drawHud();
        if (phase_ == BIGWIN) drawBigWin();
        if (phase_ == FS_INTRO || phase_ == FS_OUTRO) drawIntro();
        drawParticles(true);
        banner_.render(640, 330, 52);
        pause_.render();
        dialog_.render();
    }

private:
    using Sym_t = slot::Sym;
    struct PlayerState { int profile; int order; int bet; };

    PlayerState& cur() { return players_[turn_]; }
    i64 bet() { return BETS[std::clamp(cur().bet, 0, BET_LEVELS - 1)]; }
    int pad() { return input::padForSeat(cur().order); }

    static float approachMoney(float cur, float target, float dt) {
        if (cur == target) return cur;
        float d = target - cur;
        float step = std::max(std::fabs(d) * 6 * dt, 1.f);
        if (std::fabs(d) <= step) return target;
        return cur + (d > 0 ? step : -step);
    }

    // ====================================================================== input
    void updateIdle(float dt) {
        (void)dt;
        Profile& p = save::player(cur().profile);
        int pd = pad();
        if (input::repeat(pd, BTN_LEFT) && cur().bet > 0) { cur().bet--; audio::play(audio::SFX_CHIP, 0.6f, 0, 0.9f); }
        if (input::repeat(pd, BTN_RIGHT) && cur().bet < BET_LEVELS - 1) { cur().bet++; audio::play(audio::SFX_CHIP, 0.6f, 0, 1.1f); }
        if (input::pressed(pd, BTN_X)) {
            turbo_ = !turbo_;
            save::data().settings.slotTurbo = turbo_;
            ui::toast(turbo_ ? "Турбо-режим включён" : "Турбо-режим выключен");
        }
        if (input::pressed(pd, BTN_MINUS) && players_.size() > 1) nextPlayer(false);
        if (input::pressed(pd, BTN_ZR | BTN_R)) {
            i64 cost = bet() * BUY_COST;
            if (p.balance < cost) { audio::play(audio::SFX_ERROR); ui::toast("Для покупки бонуса нужно " + fmtMoney(cost), pal::redBright); }
            else
                dialog_.show("КУПИТЬ БОНУС?", "15 бесплатных вращений за " + fmtMoney(cost) + " (×" + std::to_string(BUY_COST) + " ставки).",
                             {"Отмена", "Купить"}, [this, cost](int pick) {
                                 if (pick != 1) return;
                                 save::player(cur().profile).balance -= cost;
                                 spent_ = cost;
                                 startSpin(true);
                             },
                             pd);
        }
        if (input::pressed(pd, BTN_Y)) {
            auto_ = !auto_;
            ui::toast(auto_ ? "Автоигра: спины идут сами (Y — стоп)" : "Автоигра остановлена");
            if (auto_) tryStart();
            return;
        }
        if (input::pressed(pd, BTN_A)) tryStart();
    }

    void tryStart() {
        Profile& p = save::player(cur().profile);
        if (p.balance < bet()) {
            audio::play(audio::SFX_ERROR);
            ui::toast(p.balance < BETS[0] ? "Фишки закончились — касса в меню «Игроки»" : "Не хватает на эту ставку",
                      pal::redBright, 2.6f);
            auto_ = false;
            return;
        }
        p.balance -= bet();
        spent_ = bet();
        startSpin(false);
    }

    // ====================================================================== spin flow
    void startSpin(bool buy) {
        phase_ = SPINNING;
        winTarget_ = winShown_ = free_ ? (float)fsTotal_ : 0.f;
        spinWin_ = 0;
        orbTotal_ = 0;
        orbShown_ = 0;
        stepWin_.clear();
        Engine& e = free_ ? fsEngine_ : baseEngine_;
        spin_ = e.play(bet(), rng(), buy);
        // old symbols drop away
        for (int c = 0; c < COLS; c++)
            for (int r = 0; r < ROWS; r++) {
                SymInst& s = grid_[c][r];
                s.ty = GY + ROWS * CELL + CELL * 2 + r * 6;
                s.vy = 0;
                s.delay = c * 0.035f;
                s.landed = false;
                s.hit = false;
                s.glow = 0;
            }
        audio::play(audio::SFX_REEL_DROP, 0.7f);
        seq_.then(0.42f, [this] { dropIn(); });
    }

    void dropIn() {
        for (int c = 0; c < COLS; c++)
            for (int r = 0; r < ROWS; r++) {
                SymInst& s = grid_[c][r];
                s.cell = spin_.initial[c][r];
                s.y = GY - CELL * (ROWS - r) - CELL * 0.5f - c * 4;
                s.ty = cellY((float)r);
                s.vy = 0;
                s.delay = c * 0.075f + (ROWS - 1 - r) * 0.012f;
                s.landed = false;
                s.hit = false;
                s.pop = 0;
            }
        landSoundCol_ = -1;
        seq_.then(0.95f, [this] { afterLand(); runStep(0); });
    }

    void afterLand() {
        // scatter anticipation and orb arrivals
        int scat = 0;
        for (int c = 0; c < COLS; c++)
            for (int r = 0; r < ROWS; r++) {
                if (grid_[c][r].cell.s == SCATTER) scat++;
                if (grid_[c][r].cell.s == ORB && grid_[c][r].pop <= 0) {
                    grid_[c][r].pop = 1;
                    audio::play(audio::SFX_ORB, 0.6f, (cellX(c) - 640) / 700.f);
                }
            }
        if (scat >= 3) audio::play(audio::SFX_THUNDER, 0.35f);
    }

    void runStep(size_t k) {
        if (k >= spin_.steps.size()) { finishTumbles(); return; }
        const Step& st = spin_.steps[k];
        for (int c = 0; c < COLS; c++)
            for (int r = 0; r < ROWS; r++) grid_[c][r].hit = st.hit[c][r];
        stepWin_.clear();
        for (auto& w : st.wins)
            stepWin_ += (stepWin_.empty() ? "" : "   ") + std::string(symbolName(w.sym)) + " ×" + std::to_string(w.count) + "  " + fmtMoney(w.amount);
        spinWin_ += st.win;
        winTarget_ = (float)(free_ ? fsTotal_ + spinWin_ : spinWin_);
        audio::play(audio::SFX_WIN, 0.45f, 0, 1.0f + 0.06f * (float)k);
        // floating amount at the centre of the winning symbols
        float sx = 0, sy = 0;
        int n = 0;
        for (int c = 0; c < COLS; c++)
            for (int r = 0; r < ROWS; r++)
                if (st.hit[c][r]) { sx += cellX(c); sy += cellY((float)r); n++; }
        if (n) floaters_.push_back({fmtMoney(st.win), sx / n, sy / n, 0, pal::goldLight});
        seq_.then(0.75f, [this, k] { explode(k); });
    }

    void explode(size_t k) {
        const Step& st = spin_.steps[k];
        for (int c = 0; c < COLS; c++)
            for (int r = 0; r < ROWS; r++) {
                if (!st.hit[c][r]) continue;
                SymInst& s = grid_[c][r];
                dying_.push_back({s.cell, cellX(c), s.y, 0});
                burst(cellX(c), s.y, slotart::symbolColor(s.cell.s), 10);
            }
        audio::play(audio::SFX_SHATTER, 0.7f);
        // survivors fall, fresh symbols enter from the top
        for (int c = 0; c < COLS; c++)
            for (int r = 0; r < ROWS; r++) {
                SymInst& s = grid_[c][r];
                s.cell = st.after[c][r];
                s.ty = cellY((float)r);
                s.y = cellY((float)(r - st.fall[c][r]));
                bool fresh = r < st.fresh[c];
                s.vy = 0;
                s.delay = fresh ? 0.12f + c * 0.03f : 0.05f;
                s.landed = st.fall[c][r] == 0;
                s.hit = false;
                if (fresh && s.cell.s == ORB) s.pop = 0;
            }
        seq_.then(0.75f, [this, k] { afterLand(); runStep(k + 1); });
    }

    void finishTumbles() {
        stepWin_.clear();
        std::vector<std::pair<int, int>> orbs;
        for (int c = 0; c < COLS; c++)
            for (int r = 0; r < ROWS; r++)
                if (grid_[c][r].cell.s == ORB) orbs.push_back({c, r});
        float d = 0.1f;
        if (spin_.tumbleWin > 0 && !orbs.empty()) {
            for (auto [c, r] : orbs) {
                int v = grid_[c][r].cell.mult;
                seq_.then(d, [this, c, r, v] {
                    zapTo(cellX(c), cellY((float)r));
                    grid_[c][r].pop = 1.2f;
                    orbShown_ += v;
                    floaters_.push_back({"×" + std::to_string(v), cellX(c), cellY((float)r) - 20, 0, slotart::orbColor(v).scaled(1.8f)});
                });
                d = turbo_ ? 0.22f : 0.38f;
            }
            seq_.then(0.5f, [this] {
                int mult = free_ ? spin_.fsMultAfter : spin_.orbSum;
                floaters_.push_back({"×" + std::to_string(mult), 640, 330, 0, pal::goldLight});
                audio::play(audio::SFX_BOOM, 0.6f);
                flash_ = 0.6f;
                winTarget_ = (float)(free_ ? fsTotal_ + spin_.baseWin : spin_.baseWin);
            });
            seq_.wait(0.6f);
        }
        if ((!free_ && spin_.scatters >= 4) || (free_ && spin_.scatters >= 3)) {
            seq_.then(0.2f, [this] {
                for (int c = 0; c < COLS; c++)
                    for (int r = 0; r < ROWS; r++)
                        if (grid_[c][r].cell.s == SCATTER) {
                            grid_[c][r].hit = true;
                            zapTo(cellX(c), cellY((float)r));
                        }
                audio::play(audio::SFX_THUNDER, 0.9f);
                flash_ = 1;
                winTarget_ = (float)((free_ ? fsTotal_ : 0) + spin_.total);
            });
            seq_.wait(1.2f);
        }
        seq_.then(0.15f, [this] { settleSpin(); });
    }

    void settleSpin() {
        for (auto& col : grid_)
            for (auto& s : col) s.hit = false;
        i64 win = spin_.total;
        Profile& p = save::player(cur().profile);
        p.balance += win;
        lastWin_ = win;
        if (free_) {
            fsTotal_ += win;
            fsLeft_--;
            fsPlayed_++;
            if (spin_.freeSpins > 0) {
                fsLeft_ += spin_.freeSpins;
                banner_.show("+" + std::to_string(spin_.freeSpins) + " ВРАЩЕНИЙ", "", 1.8f);
            }
            if (fsTotal_ >= bet() * MAX_WIN_X) fsLeft_ = 0;
            winTarget_ = (float)fsTotal_;
        } else {
            save::recordResult(cur().profile, win - spent_);
            winTarget_ = (float)win;
        }
        save::store();
        float x = win / (float)bet();
        if (win > 0 && x >= 15 && !free_) {
            startBigWin(win);
            return;
        }
        if (win > 0) audio::play(x >= 3 ? audio::SFX_BIGWIN : audio::SFX_COIN, x >= 3 ? 0.5f : 0.7f);
        afterSpin();
    }

    void afterSpin() {
        if (!free_ && spin_.freeSpins > 0) {
            free_ = true;
            fsEngine_ = Engine();
            fsEngine_.freeGame = true;
            fsLeft_ = spin_.freeSpins;
            fsTotal_ = 0;
            fsPlayed_ = 0;
            fsStake_ = spent_;
            phase_ = FS_INTRO;
            introT_ = 0;
            audio::play(audio::SFX_BOOM, 0.9f);
            audio::play(audio::SFX_THUNDER, 0.8f);
            flash_ = 1.2f;
            return;
        }
        if (free_) {
            if (fsLeft_ > 0) {
                phase_ = SPINNING;
                seq_.then(turbo_ ? 0.35f : 0.7f, [this] { startSpin(false); });
            } else {
                phase_ = FS_OUTRO;
                introT_ = 0;
                audio::play(audio::SFX_BIGWIN, 0.8f);
                coinShower(60);
            }
            return;
        }
        phase_ = IDLE;
        bool rotated = false;
        if (players_.size() > 1 && save::data().settings.slotRotate) { nextPlayer(true); rotated = true; }
        if (auto_ && !rotated) seq_.then(turbo_ ? 0.25f : 0.5f, [this] { if (phase_ == IDLE && auto_) tryStart(); });
        else if (rotated) auto_ = false;
    }

    void endFreeSpins() {
        free_ = false;
        save::recordResult(cur().profile, fsTotal_); // the stake was booked with the triggering spin
        save::store();
        i64 tot = fsTotal_;
        winTarget_ = (float)tot;
        if (tot >= bet() * 15) { startBigWin(tot); return; }
        phase_ = IDLE;
        if (players_.size() > 1 && save::data().settings.slotRotate) nextPlayer(true);
    }

    void nextPlayer(bool automatic) {
        turn_ = (turn_ + 1) % (int)players_.size();
        audio::play(audio::SFX_KNOCK, 0.6f);
        ui::toast("Ход: " + save::player(cur().profile).name, playerColor(save::player(cur().profile).color).scaled(1.3f));
        (void)automatic;
        winTarget_ = winShown_ = 0;
    }

    // ====================================================================== big win
    void startBigWin(i64 amount) {
        phase_ = BIGWIN;
        bigAmount_ = amount;
        bigT_ = 0;
        bigShown_ = 0;
        audio::play(audio::SFX_BIGWIN, 0.9f);
        coinShower(90);
    }

    void updateBigWin(float dt) {
        bigT_ += dt;
        float dur = 4.f;
        bigShown_ = (float)bigAmount_ * ease::outCubic(clamp01(bigT_ / dur));
        if ((int)(bigT_ * 14) != (int)((bigT_ - dt) * 14) && bigT_ < dur) audio::play(audio::SFX_TICK, 0.35f, 0, 1.2f);
        if ((int)(bigT_ * 3) != (int)((bigT_ - dt) * 3) && bigT_ < dur) coinShower(8);
        if (input::pressed(ANY_PAD, BTN_A) && bigT_ < dur) bigT_ = dur;
        else if ((input::pressed(ANY_PAD, BTN_A) && bigT_ > dur + 0.2f) || bigT_ > dur + 2.6f) {
            phase_ = IDLE;
            if (free_) return;
            if (players_.size() > 1 && save::data().settings.slotRotate) { nextPlayer(true); auto_ = false; }
            else if (auto_) seq_.then(0.4f, [this] { if (phase_ == IDLE && auto_) tryStart(); });
        }
    }

    void drawBigWin() {
        float a = clamp01(bigT_ * 3);
        gfx::dim(0.55f * a);
        float x = bigAmount_ / (float)bet();
        const char* title = x >= 100 ? "ЭПИЧЕСКИЙ ВЫИГРЫШ" : (x >= 50 ? "МЕГА ВЫИГРЫШ" : "БОЛЬШОЙ ВЫИГРЫШ");
        float pulse = 1 + 0.04f * std::sin(time_ * 6);
        gfx::glowEllipse(640, 300, 420, 160, Color::hex(0xffb030), 0.35f * a);
        gfx::textGold(title, 640, 270, F_TITLE, 58 * pulse, 0, a);
        gfx::textGlow(fmtMoney((i64)bigShown_), 640, 360, F_NUM, 80, Color::hex(0xffc040), 0.6f * a, 0);
        gfx::textShadow(fmtMoney((i64)bigShown_), 640, 360, F_NUM, 80, pal::goldLight, 0, a);
        gfx::text(strf("×%.0f ставки", x), 640, 430, F_SANS_BOLD, 22, pal::ivory, 0, a);
    }

    void drawIntro() {
        float a = clamp01(introT_ * 3);
        gfx::dim(0.6f * a);
        float s = ease::outBack(clamp01(introT_ / 0.5f), 2);
        gfx::glowEllipse(640, 320, 460, 200, Color::hex(0x6a9aff), 0.35f * a);
        if (phase_ == FS_INTRO) {
            gfx::textGold(std::to_string(fsLeft_), 640, 250, F_TITLE, 110 * s, 0, a);
            gfx::textGold("БЕСПЛАТНЫХ ВРАЩЕНИЙ", 640, 350, F_TITLE, 40, 0, a);
            gfx::text("Множители сфер копятся до конца бонуса", 640, 405, F_SANS, 20, pal::ivory, 0, a);
        } else {
            gfx::textGold("БОНУС ЗАВЕРШЁН", 640, 250, F_TITLE, 44, 0, a);
            gfx::textShadow(fmtMoney(fsTotal_), 640, 330, F_NUM, 76 * s, pal::goldLight, 0, a);
            gfx::text(strf("за %d бесплатных вращений · общий множитель ×%d", fsPlayed_, fsEngine_.fsMult), 640, 395, F_SANS, 20,
                      pal::ivory, 0, a);
        }
        if (introT_ > 0.8f) ui::hints({{BTN_A, "Продолжить"}}, 640, 470, 0, a);
    }

    // ====================================================================== effects
    void zapTo(float x, float y) {
        float ex = MEDX - 50 * MEDS, ey = MEDY - 20 * MEDS; // Zeus's eye
        bolts_.push_back({ex, ey, x, y, 0, 0.45f, (uint32_t)rng().range(1, 1 << 30), 1.f});
        audio::play(audio::SFX_ZAP, 0.7f, (x - 640) / 700.f);
        eyeGlow_ = 1;
        burst(x, y, Color::hex(0xb0d0ff), 14);
    }

    void burst(float x, float y, Color c, int n) {
        for (int i = 0; i < n; i++) {
            float a = rng().uniform(0, 2 * PI), sp = rng().uniform(80, 340);
            Particle p{x, y, std::cos(a) * sp, std::sin(a) * sp - 120, 0, rng().uniform(0.5f, 1.0f), rng().uniform(3, 7),
                       rng().uniform(0, 6.28f), rng().uniform(-8, 8), c, i % 3 == 0 ? 0 : 1};
            particles_.push_back(p);
        }
    }

    void coinShower(int n) {
        for (int i = 0; i < n; i++) {
            Particle p{rng().uniform(100, 1180), rng().uniform(-200, -20), rng().uniform(-60, 60), rng().uniform(80, 260), 0,
                       rng().uniform(2.5f, 3.6f), rng().uniform(0.6f, 1.1f), rng().uniform(0, 6.28f), rng().uniform(4, 10),
                       pal::gold, 2};
            particles_.push_back(p);
        }
    }

    void updateFx(float dt) {
        for (auto& p : particles_) {
            p.life += dt;
            p.vy += (p.kind == 2 ? 380 : 620) * dt;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.rot += p.vr * dt;
        }
        particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.life >= p.max || p.y > 800; }),
                         particles_.end());
        for (auto& d : dying_) d.t += dt;
        dying_.erase(std::remove_if(dying_.begin(), dying_.end(), [](const Dying& d) { return d.t > 0.35f; }), dying_.end());
        for (auto& b : bolts_) b.t += dt;
        bolts_.erase(std::remove_if(bolts_.begin(), bolts_.end(), [](const Bolt& b) { return b.t > b.dur; }), bolts_.end());
        for (auto& b : skyBolts_) b.t += dt;
        skyBolts_.erase(std::remove_if(skyBolts_.begin(), skyBolts_.end(), [](const Bolt& b) { return b.t > b.dur; }), skyBolts_.end());
        for (auto& f : floaters_) f.t += dt;
        floaters_.erase(std::remove_if(floaters_.begin(), floaters_.end(), [](const Floater& f) { return f.t > 1.4f; }), floaters_.end());
        flash_ = std::max(0.f, flash_ - dt * 2.5f);
        eyeGlow_ = std::max(0.f, eyeGlow_ - dt * 1.5f);
        // occasional lightning across the sky
        nextFlash_ -= dt;
        if (nextFlash_ <= 0) {
            nextFlash_ = rng().uniform(free_ ? 2.5f : 5.f, free_ ? 6.f : 12.f);
            float x = rng().chance(0.5) ? rng().uniform(120, 330) : rng().uniform(930, 1180);
            skyBolts_.push_back({x, 40, x + rng().uniform(-90, 90), rng().uniform(380, 520), 0, 0.35f, (uint32_t)rng().range(1, 1 << 30), 0.7f});
            flash_ = std::max(flash_, 0.45f);
            float pan = (x - 640) / 700.f;
            timers_.add(rng().uniform(0.2f, 0.8f), [pan] { audio::play(audio::SFX_THUNDER, 0.35f, pan); });
        }
        timers_.update(dt);
    }

    void updateSymbols(float dt) {
        const float G = 4200;
        int landedCol = -1;
        for (int c = 0; c < COLS; c++)
            for (int r = 0; r < ROWS; r++) {
                SymInst& s = grid_[c][r];
                if (s.pop > 0 && s.pop < 2) s.pop = std::max(0.f, s.pop - dt * 1.5f);
                if (s.hit) s.glow = std::min(1.f, s.glow + dt * 5);
                else s.glow = std::max(0.f, s.glow - dt * 4);
                if (s.delay > 0) { s.delay -= dt; continue; }
                if (s.y < s.ty || s.vy != 0) {
                    s.vy += G * dt;
                    s.y += s.vy * dt;
                    if (s.y >= s.ty) {
                        s.y = s.ty;
                        if (s.vy > 500) {
                            s.vy = -s.vy * 0.18f;
                            if (!s.landed) landedCol = c;
                            s.landed = true;
                        } else {
                            s.vy = 0;
                        }
                    }
                }
            }
        if (landedCol >= 0 && landedCol != landSoundCol_) {
            landSoundCol_ = landedCol;
            audio::play(audio::SFX_LAND, 0.55f, (cellX(landedCol) - 640) / 700.f, 0.9f + 0.05f * landedCol);
        }
    }

    // ====================================================================== drawing
    void drawBolt(const Bolt& b) {
        float a = 1 - b.t / b.dur;
        if (a <= 0) return;
        Rng r(b.seed + (uint32_t)(b.t * 30) * 7919u);
        std::vector<SDL_FPoint> pts = {{b.x1, b.y1}, {b.x2, b.y2}};
        float disp = std::hypot(b.x2 - b.x1, b.y2 - b.y1) * 0.22f;
        for (int it = 0; it < 5; it++) {
            std::vector<SDL_FPoint> n;
            for (size_t i = 0; i + 1 < pts.size(); i++) {
                n.push_back(pts[i]);
                float mx = (pts[i].x + pts[i + 1].x) / 2, my = (pts[i].y + pts[i + 1].y) / 2;
                float dx = pts[i + 1].x - pts[i].x, dy = pts[i + 1].y - pts[i].y;
                float l = std::sqrt(dx * dx + dy * dy) + 1e-3f;
                float o = r.uniform(-disp, disp);
                n.push_back({mx - dy / l * o, my + dx / l * o});
            }
            n.push_back(pts.back());
            pts = n;
            disp *= 0.55f;
        }
        Color glowC = Color::hex(0x7aa8ff);
        for (size_t i = 0; i + 1 < pts.size(); i++) {
            gfx::line(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, 9 * b.width, glowC.alpha(0.18f * a), true);
            gfx::line(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, 3.5f * b.width, Color::hex(0xc8dcff).alpha(0.55f * a), true);
            gfx::line(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, 1.3f * b.width, pal::white.alpha(a), true);
        }
        // a short side branch
        if (pts.size() > 10) {
            size_t k = pts.size() / 3;
            float bx = pts[k].x, by = pts[k].y;
            float ex = bx + r.uniform(-60, 60), ey = by + r.uniform(20, 70);
            gfx::line(bx, by, ex, ey, 1.4f * b.width, Color::hex(0xc8dcff).alpha(0.5f * a), true);
        }
        gfx::glow(b.x2, b.y2, 40 * b.width, glowC, 0.6f * a);
    }

    void drawMedallion() {
        float pulse = 0.5f + 0.5f * std::sin(time_ * 1.3f);
        gfx::glow(MEDX, MEDY, 190 * MEDS, Color::hex(0xffc35a), 0.18f + 0.08f * pulse);
        gfx::drawCentered(a_.medallion, MEDX, MEDY, MEDS);
        float eg = eyeGlow_ + (free_ ? 0.25f + 0.2f * pulse : 0.f);
        if (eg > 0.01f) {
            float ex = MEDX - 50 * MEDS, ey = MEDY - 20 * MEDS;
            gfx::glow(ex, ey, 26, Color::hex(0x9ac4ff), eg);
            gfx::glow(ex, ey, 8, pal::white, eg);
        }
        // multiplier tablet under the medallion
        float y = MEDY + 160 * MEDS + 26;
        ui::panel(MEDX - 120, y - 30, 240, 120, 0.95f);
        if (free_) {
            gfx::text("ОБЩИЙ МНОЖИТЕЛЬ", MEDX, y - 6, F_SANS_BOLD, 15, pal::muted, 0);
            int m = std::max(1, fsEngine_.fsMult);
            gfx::textGold("×" + std::to_string(m), MEDX, y + 38, F_TITLE, 46, 0);
        } else {
            gfx::text("МНОЖИТЕЛЬ СФЕР", MEDX, y - 6, F_SANS_BOLD, 15, pal::muted, 0);
            if (orbShown_ > 0) gfx::textGold("×" + std::to_string(orbShown_), MEDX, y + 38, F_TITLE, 46, 0);
            else gfx::text("—", MEDX, y + 38, F_TITLE, 36, pal::dim, 0);
        }
    }

    void drawGrid() {
        float w = CELL * COLS, h = CELL * ROWS;
        gfx::rectGrad(GX, GY, w, h, Color(6, 10, 34, 215), Color(14, 8, 30, 225));
        for (int c = 1; c < COLS; c++) gfx::rect(GX + c * CELL - 0.5f, GY + 6, 1, h - 12, Color(255, 255, 255, 12));
        gfx::glowEllipse(640, GY + h * 0.4f, w * 0.5f, h * 0.45f, Color::hex(0x3050c0), free_ ? 0.22f : 0.12f);
        gfx::clip(GX, GY, w, h);
        for (int c = 0; c < COLS; c++)
            for (int r = 0; r < ROWS; r++) {
                const SymInst& s = grid_[c][r];
                float x = cellX(c);
                float scale = 1.f;
                if (s.hit) scale = 1 + 0.08f * std::sin(time_ * 14) * s.glow + 0.06f * s.glow;
                if (s.glow > 0.01f && s.cell.s != ORB) {
                    gfx::drawCentered(a_.symGlow[s.cell.s], x, s.y, scale * 1.12f, 0, s.glow * 0.9f,
                                      slotart::symbolColor(s.cell.s), true);
                }
                drawSymbol(s.cell, x, s.y, scale, 1.f, s.pop);
            }
        for (auto& d : dying_) {
            float k = d.t / 0.35f;
            drawSymbol(d.cell, d.x, d.y, 1 + k * 0.5f, 1 - k, 0);
        }
        gfx::unclip();
        gfx::draw(a_.frame, GX - 20, GY - 20);
        if (!stepWin_.empty() && phase_ == SPINNING)
            gfx::textShadow(stepWin_, 640, GY + h + 36, F_SANS_BOLD, 17, pal::goldLight, 0);
    }

    void drawSymbol(const Cell& c, float x, float y, float scale, float alpha, float pop) {
        if (c.s == ORB) {
            auto it = a_.orbs.find(c.mult);
            if (it == a_.orbs.end()) return;
            float wob = 1 + 0.05f * std::sin(time_ * 4 + x * 0.05f) + 0.25f * std::max(0.f, pop);
            gfx::glow(x, y, 46 * scale * wob, slotart::orbColor(c.mult).scaled(1.6f), 0.45f * alpha);
            gfx::drawCentered(it->second, x, y, scale * wob, 0, alpha);
            return;
        }
        if (c.s == SCATTER) gfx::glow(x, y, 44 * scale, Color::hex(0x6a9aff), (0.35f + 0.15f * std::sin(time_ * 5)) * alpha);
        gfx::drawCentered(a_.sym[c.s], x, y, scale, 0, alpha);
    }

    void drawParticles(bool coins) {
        for (auto& p : particles_) {
            float a = 1 - p.life / p.max;
            if (coins != (p.kind == 2)) continue;
            if (p.kind == 0) gfx::glow(p.x, p.y, p.size * 3, p.c, a, true);
            else if (p.kind == 1) {
                float s = p.size;
                SDL_FPoint q[3];
                for (int k = 0; k < 3; k++) q[k] = {p.x + std::cos(p.rot + k * 2.1f) * s, p.y + std::sin(p.rot + k * 2.1f) * s};
                gfx::triangle(q[0], q[1], q[2], p.c.alpha(a), p.c.scaled(1.5f).alpha(a), p.c.scaled(0.6f).alpha(a));
            } else {
                gfx::drawCenteredXY(a_.coin, p.x, p.y, p.size * std::cos(p.rot), p.size, 0, std::min(1.f, a * 2));
            }
        }
    }

    void drawSidePanel() {
        float x = 92, y = 128, w = 250;
        if (free_) {
            ui::panel(x, y, w, 150);
            gfx::text("БЕСПЛАТНЫЕ ВРАЩЕНИЯ", x + w / 2, y + 26, F_SANS_BOLD, 15, pal::muted, 0);
            gfx::textGold(std::to_string(fsLeft_), x + w / 2, y + 76, F_TITLE, 54, 0);
            gfx::text("осталось", x + w / 2, y + 118, F_SANS, 15, pal::cream, 0);
            y += 166;
            ui::panel(x, y, w, 96);
            gfx::text("ВЫИГРЫШ БОНУСА", x + w / 2, y + 24, F_SANS_BOLD, 15, pal::muted, 0);
            gfx::textGold(fmtMoney((i64)winShown_), x + w / 2, y + 62, F_NUM, 34, 0);
            y += 112;
        } else {
            ui::panel(x, y, w, 128);
            gfx::text("КУПИТЬ БОНУС", x + w / 2, y + 26, F_SANS_BOLD, 15, pal::muted, 0);
            gfx::textGold(fmtMoney(bet() * BUY_COST), x + w / 2, y + 62, F_NUM, 30, 0);
            ui::hints({{BTN_ZR, "15 бесплатных"}}, x + w / 2, y + 100, 0, 1.f, 15);
            y += 144;
        }
        if (players_.size() > 1) {
            gfx::text("ИГРОКИ", x + 4, y + 8, F_SANS_BOLD, 13, pal::muted, -1);
            y += 22;
            for (size_t i = 0; i < players_.size() && i < 4; i++) {
                ui::playerTag(players_[i].profile, x, y, w, (int)i == turn_);
                y += 54;
            }
        } else {
            ui::panel(x, y, w, 110, 0.9f, false);
            gfx::text("4+ молнии — 15 бесплатных", x + 16, y + 26, F_SANS, 16, pal::cream, -1);
            gfx::text("8+ одинаковых — выигрыш", x + 16, y + 52, F_SANS, 16, pal::cream, -1);
            gfx::text("сферы умножают выигрыш", x + 16, y + 78, F_SANS, 16, pal::cream, -1);
        }
    }

    void drawHud() {
        float y = 600;
        gfx::rectGrad(0, y - 20, SCREEN_W, 140, Color(0, 0, 0, 0), Color(0, 0, 0, 230));
        Profile& p = save::player(cur().profile);
        auto box = [&](float cx, const char* label, const std::string& value, Color vc, float w) {
            gfx::roundRect(cx - w / 2, y, w, 62, 10, Color(8, 6, 14, 220));
            gfx::roundRectOutline(cx - w / 2, y, w, 62, 10, 1.2f, pal::gold.alpha(0.55f));
            gfx::text(label, cx, y + 16, F_SANS_BOLD, 13, pal::muted, 0);
            gfx::text(value, cx, y + 42, F_NUM, 26, vc, 0);
        };
        box(250, ("БАЛАНС · " + p.name).c_str(), fmtMoney(p.balance), pal::ivory, 300);
        box(530, "СТАВКА", fmtMoney(bet()), pal::goldLight, 200);
        if (phase_ == IDLE) {
            gfx::text("\xE2\x80\xB9", 446, y + 40, F_SANS_BOLD, 26, cur().bet > 0 ? pal::gold : pal::dim, 0);
            gfx::text("\xE2\x80\xBA", 614, y + 40, F_SANS_BOLD, 26, cur().bet < BET_LEVELS - 1 ? pal::gold : pal::dim, 0);
        }
        box(810, free_ ? "ВЫИГРЫШ БОНУСА" : "ВЫИГРЫШ", fmtMoney((i64)winShown_), winShown_ > 0 ? pal::greenBright : pal::dim, 260);
        // spin button
        float bx = 1100, by = y + 31;
        bool ready = phase_ == IDLE;
        float pulse = ready ? 0.5f + 0.5f * std::sin(time_ * 3) : 0;
        gfx::glow(bx, by, 70, Color::hex(0xffc35a), 0.2f + 0.2f * pulse);
        gfx::circle(bx, by, 40, Color::hex(0x1a1208));
        gfx::ring(bx, by, 40, 3, pal::gold);
        // circular "spin" arrow, turning while the reels run
        float rot = phase_ == SPINNING ? time_ * 7 : -0.6f;
        Color ac = ready ? pal::goldLight : pal::muted;
        const float ar = 21, sweep = 1.6f * PI;
        const int segs = 18;
        for (int k = 0; k < segs; k++) {
            float a0 = rot + sweep * k / segs, a1 = rot + sweep * (k + 1) / segs;
            gfx::line(bx + std::cos(a0) * ar, by + std::sin(a0) * ar, bx + std::cos(a1) * ar, by + std::sin(a1) * ar, 5, ac);
        }
        float ae = rot + sweep;
        float tx = -std::sin(ae), ty = std::cos(ae); // tangent (direction of travel)
        float hx = bx + std::cos(ae) * ar, hy = by + std::sin(ae) * ar;
        gfx::triangle({hx + tx * 10, hy + ty * 10}, {hx + std::cos(ae) * 9, hy + std::sin(ae) * 9},
                      {hx - std::cos(ae) * 9, hy - std::sin(ae) * 9}, ac, ac, ac);
        if (auto_) gfx::text("АВТО", bx, by + 56, F_SANS_BOLD, 13, pal::greenBright, 0);
        if (turbo_) gfx::text("ТУРБО", bx + 70, by - 30, F_SANS_BOLD, 12, pal::goldLight, 0);
        std::vector<std::pair<u32, std::string>> h;
        if (phase_ == IDLE) {
            h = {{BTN_A, "Вращать"}, {BTN_LEFT | BTN_RIGHT, "Ставка"}, {BTN_Y, auto_ ? "Стоп" : "Авто"}, {BTN_X, "Турбо"}};
            if (players_.size() > 1) h.push_back({BTN_MINUS, "Игрок"});
            h.push_back({BTN_PLUS, "Меню"});
        } else if (auto_) {
            h = {{BTN_Y, "Остановить авто"}};
        }
        ui::hints(h, 1262, 698, 1, 1.f, 15);
    }

    void openPause() {
        audio::play(audio::SFX_SELECT);
        pause_.rulesTitle = "ПРАВИЛА СЛОТА";
        pause_.rulesText = kRules;
        auto_ = false;
        std::vector<ui::MenuItem> items;
        items.push_back({"Продолжить", nullptr, nullptr, [this] { pause_.close(); }});
        items.push_back({"Правила и выплаты", nullptr, nullptr, [this] { pause_.rules = true; }});
        items.push_back({"Ход переходит дальше", [] { return std::string(save::data().settings.slotRotate ? "Вкл" : "Выкл"); },
                         [](int) { save::data().settings.slotRotate = !save::data().settings.slotRotate; save::store(); }, nullptr});
        items.push_back({"Вернуться в зал", nullptr, nullptr, [this] {
                             pause_.close();
                             save::data().settings.slotBet = players_[0].bet;
                             save::store();
                             app::go(SC_HALL);
                         }});
        pause_.show(items);
    }

    slotart::Assets a_;
    SymInst grid_[COLS][ROWS];
    std::vector<Dying> dying_;
    std::vector<Particle> particles_;
    std::vector<Bolt> bolts_, skyBolts_;
    std::vector<Floater> floaters_;
    std::vector<PlayerState> players_;
    int turn_ = 0;
    Engine baseEngine_, fsEngine_;
    Spin spin_;
    Phase phase_ = IDLE;
    bool free_ = false, auto_ = false, turbo_ = false;
    int fsLeft_ = 0, fsPlayed_ = 0;
    i64 fsTotal_ = 0, fsStake_ = 0, spent_ = 0, spinWin_ = 0, lastWin_ = 0, bigAmount_ = 0;
    int orbTotal_ = 0, orbShown_ = 0;
    float winShown_ = 0, winTarget_ = 0, bigShown_ = 0, bigT_ = 0, introT_ = 0;
    float time_ = 0, flash_ = 0, eyeGlow_ = 0, nextFlash_ = 3;
    int landSoundCol_ = -1;
    std::string stepWin_;
    Seq seq_;
    Timers timers_;
    ui::Banner banner_;
    ui::PauseMenu pause_;
    ui::Dialog dialog_;
};

} // namespace

std::unique_ptr<Scene> makeSlotsScene() { return std::make_unique<SlotsScene>(); }
