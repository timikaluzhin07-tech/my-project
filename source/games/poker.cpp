// Texas Hold'em table: humans (shared or own controllers) and bots, up to six seats.
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
#include "poker_logic.h"

namespace {

using namespace poker;

struct Pt { float x, y; };

constexpr float TCX = 640, TCY = 318;
const Pt PLATE[SEATS] = {{640, 644}, {176, 520}, {176, 150}, {640, 64}, {1104, 150}, {1104, 520}};
const Pt BETPOS[SEATS] = {{640, 462}, {340, 444}, {350, 236}, {770, 186}, {930, 236}, {940, 444}};
const Pt POT = {640, 386};
const float BOARD_Y = 300;
const i64 BLINDS[4][2] = {{10, 20}, {25, 50}, {50, 100}, {100, 200}};
const i64 BUYINS[4] = {2000, 5000, 10000, 25000};
const char* BOT_NAMES[] = {"Виктор", "Алиса", "Марко", "Софи", "Дон Карлос", "Лео", "Ирина", "Хьюго", "Ната", "Борис",
                           "Эмиль", "Вера", "Рауль", "Кира"};
constexpr int BOT_NAME_COUNT = 14;

float panFor(float x) { return (x - 640) / 700.f; }

const char* kRules =
    "Каждый получает две закрытые карты, на стол выкладываются пять общих: флоп (три), тёрн и ривер. "
    "Побеждает лучшая комбинация из пяти карт среди семи доступных.\n\n"
    "Старшинство: старшая карта, пара, две пары, тройка, стрит, флеш, фулл-хаус, каре, стрит-флеш, роял-флеш.\n\n"
    "Ставки без лимита. Перед раздачей двое игроков слева от баттона ставят малый и большой блайнды. "
    "Фолд (B) — сбросить карты, Чек/Колл (A) — пропустить или уравнять, Рейз (X) — повысить, Олл-ин (Y) — поставить всё.\n\n"
    "Если игроков за столом несколько, карты скрыты: удерживайте R на своём контроллере, чтобы посмотреть их "
    "(остальные не подглядывают!). Фишки за столом — это ваш бай-ин; при выходе они возвращаются на баланс.";

Image buildTable() {
    const float S = TEX_SCALE;
    Image img = art::velvetBackdrop(Color::hex(0x1a0d10), Color::hex(0x7a5a2a), 21);
    std::string s = svg::open(SCREEN_W, SCREEN_H) + "<defs>" +
                    svg::radialU("rail", TCX, TCY - 40, 640,
                                 {{0.82f, Color::hex(0x2a2224)}, {0.92f, Color::hex(0x141012)}, {1, Color::hex(0x050404)}}) +
                    svg::radialU("felt", TCX, TCY - 30, 540, {{0, Color::hex(0x1c6f86)}, {1, Color::hex(0x092b37)}}) +
                    svg::linear("wood", 0, 0, 0, 1, {{0, Color::hex(0x8a5a2c)}, {1, Color::hex(0x3a210f)}}) + "</defs>";
    s += svg::ellipse(TCX + 6, TCY + 26, 612, 300, "#000", "fill-opacity=\"0.55\"");
    s += svg::ellipse(TCX, TCY, 600, 290, svg::url("rail"));
    s += svg::ellipse(TCX, TCY, 598, 288, "none", svg::stroke("#5a4a48", 1.2f, 0.6f));
    s += svg::ellipse(TCX, TCY, 572, 264, "none", svg::stroke("#6a5a58", 0.8f, 0.35f) + " stroke-dasharray=\"5 4\"");
    s += svg::ellipse(TCX, TCY, 548, 248, svg::url("wood"));
    s += svg::ellipse(TCX, TCY, 543, 243, "none", svg::stroke("#e0c070", 1.f, 0.6f));
    s += svg::ellipse(TCX, TCY, 536, 238, svg::url("felt"));
    s += svg::ellipse(TCX, TCY, 420, 172, "none", svg::stroke("#ffffff", 1.2f, 0.13f));
    for (int i = 0; i < 5; i++) {
        float x = 640 + (i - 2) * 82.f;
        float w = art::CARD_W * 0.74f, h = art::CARD_H * 0.74f;
        s += svg::rect(x - w / 2, BOARD_Y - h / 2, w, h, 5, "none", svg::stroke("#ffffff", 1.f, 0.16f));
    }
    s += svg::close();
    Image prints = gfx::rasterSvg(s);
    // felt fibres only inside the playing surface
    Image felt = art::feltImage(SCREEN_W, SCREEN_H, Color(255, 255, 255), Color(255, 255, 255), 5);
    for (int py = 0; py < prints.h; py++)
        for (int px = 0; px < prints.w; px++) {
            float x = px / S, y = py / S;
            float ex = (x - TCX) / 536, ey = (y - TCY) / 238;
            if (ex * ex + ey * ey < 1) {
                uint8_t* p = prints.at(px, py);
                float k = felt.at(px, py)[0] / 255.f;
                p[0] = (uint8_t)(p[0] * k); p[1] = (uint8_t)(p[1] * k); p[2] = (uint8_t)(p[2] * k);
            }
        }
    img.draw(prints, 0, 0);
    art::stampText(img, "GRAND CASINO", TCX, BOARD_Y - 8, F_TITLE, 30, Color::hex(0xe9d6a0, 46));
    art::stampText(img, "TEXAS HOLD'EM  ·  NO LIMIT", TCX, BOARD_Y + 22, F_SANS_BOLD, 12, Color::hex(0xe9d6a0, 40));
    return img;
}

enum Phase { LOBBY, DEALING, BETTING, RUNOUT, SHOWDOWN_PH, HAND_END, GAME_OVER };

struct HumanInfo {
    i64 boughtIn = 0;
};

class PokerScene : public Scene {
public:
    PokerScene() {
        table_ = gfx::upload(buildTable());
        art::ensureBuilt();
        buildLobby();
        phase_ = LOBBY;
    }

    ~PokerScene() override { cashOutAll(); }

    void update(float dt) override {
        time_ += dt;
        banner_.update(dt);
        caption_.update(dt);
        for (auto& s : holes_) for (auto& c : s) c.update(dt);
        for (auto& c : board_) c.update(dt);
        for (auto& f : flying_) f.update(dt);
        pruneDone(flying_);
        timers_.update(dt);
        for (auto& a : actionFlash_) a = std::max(0.f, a - dt);
        if (dialog_.update(dt)) return;
        if (phase_ == LOBBY) { updateLobby(dt); return; }
        if (pause_.update(dt)) return;
        if (input::pressed(ANY_PAD, BTN_PLUS) && !raising_) { openPause(); return; }
        updatePeek();
        seq_.update(dt);
        if (seq_.busy()) return;
        switch (phase_) {
            case BETTING: updateBetting(dt); break;
            case HAND_END:
                endTimer_ += dt;
                if (endTimer_ > 3.0f || input::pressed(ANY_PAD, BTN_A)) afterHand();
                break;
            default: break;
        }
    }

    void render() override {
        gfx::draw(table_, 0, 0);
        gfx::glowEllipse(640, 300, 520, 230, Color::hex(0xfff0c8), 0.06f);
        if (phase_ == LOBBY) { renderLobby(); dialog_.render(); return; }
        // pot
        if (potShown_ > 0) {
            art::drawChipStack(potShown_, POT.x, POT.y, 0.66f, 1.f, 3);
            gfx::text("БАНК  " + fmtMoney(potShown_), POT.x, POT.y + 34, F_NUM, 17, pal::goldLight, 0);
        }
        // dealer button
        if (t_.button >= 0 && t_.seats[t_.button].used) {
            Pt b = BETPOS[t_.button];
            float bx = b.x + (b.x < 640 ? 52 : (b.x > 640 ? -52 : 58)), by = b.y + 4;
            gfx::circle(bx + 1, by + 3, 15, Color(0, 0, 0, 120));
            gfx::circle(bx, by, 15, Color::hex(0xf4f1e8));
            gfx::ring(bx, by, 12, 1, Color::hex(0x9a8a6a));
            gfx::text("D", bx, by, F_SERIF, 15, Color::hex(0x1a1a1e), 0);
        }
        // street bets
        for (int i = 0; i < SEATS; i++) {
            const Seat& s = t_.seats[i];
            if (!s.used || s.streetBet <= 0) continue;
            Pt b = BETPOS[i];
            art::drawChipStack(s.streetBet, b.x, b.y, 0.55f, 1.f, 2);
            gfx::text(fmtMoney(s.streetBet), b.x, b.y + 26, F_NUM, 15, pal::ivory, 0);
        }
        for (auto& c : board_) c.draw();
        for (int i = 0; i < SEATS; i++) drawSeat(i);
        for (auto& f : flying_) f.draw();
        caption_.render(640, 212);
        banner_.render(640, 300, 46);
        drawHud();
        pause_.render();
        dialog_.render();
    }

private:
    // ===================================================================== lobby
    void buildLobby() {
        Settings& st = save::data().settings;
        auto humans = eligibleHumans();
        int maxBots = SEATS - (int)std::min<size_t>(humans.size(), SEATS);
        st.pokerBots = std::clamp(st.pokerBots, humans.size() >= 2 ? 0 : 1, maxBots);
        lobby_.items.clear();
        lobby_.items.push_back({"Боты за столом", [&st] { return std::to_string(st.pokerBots); },
                                [this, &st](int d) {
                                    auto h = eligibleHumans();
                                    int mx = SEATS - (int)std::min<size_t>(h.size(), SEATS);
                                    int mn = h.size() >= 2 ? 0 : 1;
                                    st.pokerBots = std::clamp(st.pokerBots + d, mn, mx);
                                },
                                nullptr});
        lobby_.items.push_back({"Блайнды",
                                [&st] {
                                    return fmtMoney(BLINDS[st.pokerBlinds][0]) + " / " + fmtMoney(BLINDS[st.pokerBlinds][1]);
                                },
                                [&st](int d) { st.pokerBlinds = (st.pokerBlinds + d + 4) % 4; }, nullptr});
        lobby_.items.push_back({"Бай-ин", [&st] { return fmtMoney(BUYINS[st.pokerBuyIn]); },
                                [&st](int d) { st.pokerBuyIn = (st.pokerBuyIn + d + 4) % 4; }, nullptr});
        lobby_.items.push_back({"Скрывать карты",
                                [&st] {
                                    static const char* v[] = {"Авто", "Всегда", "Никогда"};
                                    return std::string(v[st.hideCards]);
                                },
                                [&st](int d) { st.hideCards = (st.hideCards + d + 3) % 3; }, nullptr});
        lobby_.items.push_back({"Сесть за стол", nullptr, nullptr, [this] { startGame(); }});
        lobby_.items.push_back({"Вернуться в зал", nullptr, nullptr, [] { app::go(SC_HALL); }});
        lobby_.sel = 4;
        lobby_.selAnim = 4;
    }

    // Humans with enough chips for at least 20 big blinds.
    std::vector<int> eligibleHumans() const {
        std::vector<int> out;
        auto act = save::active();
        i64 bb = BLINDS[save::data().settings.pokerBlinds][1];
        for (int p : act)
            if (save::player(p).balance >= bb * 20) out.push_back(p);
        return out;
    }

    void updateLobby(float dt) {
        lobby_.update(ANY_PAD, dt);
        if (input::pressed(ANY_PAD, BTN_B)) { audio::play(audio::SFX_BACK); app::go(SC_HALL); }
    }

    void renderLobby() {
        gfx::dim(0.45f);
        float w = 620, x = (SCREEN_W - w) / 2, y = 120;
        float h = 6 * 48 + 240;
        ui::panel(x - 20, y - 20, w + 40, h, 1.f);
        gfx::textGold("ТЕХАССКИЙ ХОЛДЕМ", 640, y + 22, F_TITLE, 34);
        ui::rule(640, y + 54, 360);
        lobby_.render(x, y + 74, w, 48);
        auto hum = eligibleHumans();
        auto act = save::active();
        std::string who;
        for (size_t k = 0; k < act.size() && k < SEATS; k++) {
            bool ok = std::find(hum.begin(), hum.end(), act[k]) != hum.end();
            if (!who.empty()) who += ",  ";
            who += save::player(act[k]).name + (ok ? "" : " (мало фишек)");
        }
        gfx::text("За столом: " + who, 640, y + 74 + 6 * 48 + 22, F_SANS, 17, pal::cream, 0);
        i64 bi = BUYINS[save::data().settings.pokerBuyIn];
        gfx::text("Каждый человек покупает фишки на " + fmtMoney(bi) + " (или на весь баланс, если меньше)", 640,
                  y + 74 + 6 * 48 + 50, F_SANS, 15, pal::muted, 0);
        ui::hints({{BTN_UP | BTN_DOWN, "Выбор"}, {BTN_LEFT | BTN_RIGHT, "Изменить"}, {BTN_A, "OK"}, {BTN_B, "В зал"}}, 1262, 700);
    }

    void startGame() {
        Settings& st = save::data().settings;
        auto hum = eligibleHumans();
        if (hum.empty()) {
            audio::play(audio::SFX_ERROR);
            ui::toast("Ни у кого нет фишек на бай-ин — загляните в кассу (меню Игроки)", pal::redBright, 3);
            return;
        }
        if (hum.size() + st.pokerBots < 2) {
            st.pokerBots = 1;
        }
        save::store();
        t_ = Table();
        t_.sb = BLINDS[st.pokerBlinds][0];
        t_.bb = BLINDS[st.pokerBlinds][1];
        static const int humanSeats[SEATS] = {0, 3, 1, 4, 5, 2};
        auto act = save::active();
        int nh = 0;
        for (int p : hum) {
            if (nh >= SEATS) break;
            int seat = humanSeats[nh++];
            Seat& s = t_.seats[seat];
            s.used = true;
            s.bot = false;
            s.profile = p;
            s.order = (int)(std::find(act.begin(), act.end(), p) - act.begin());
            s.name = save::player(p).name;
            s.color = save::player(p).color;
            i64 bi = std::min(BUYINS[st.pokerBuyIn], save::player(p).balance);
            Profile& pr = save::player(p);
            pr.balance -= bi;
            pr.stake = bi;
            s.stack = bi;
            boughtIn_[seat] = bi;
        }
        humansSeated_ = nh;
        static const int botSeats[SEATS] = {3, 1, 5, 2, 4, 0};
        int nb = 0;
        for (int k = 0; k < SEATS && nb < st.pokerBots; k++) {
            int seat = botSeats[k];
            if (t_.seats[seat].used) continue;
            seatBot(seat);
            nb++;
        }
        save::store();
        hide_ = st.hideCards == 1 || (st.hideCards == 0 && humansSeated_ >= 2);
        phase_ = DEALING;
        audio::play(audio::SFX_SHUFFLE, 0.8f);
        seq_.wait(0.8f);
        seq_.then(0.1f, [this] { newHand(); });
    }

    void seatBot(int seat) {
        Seat& s = t_.seats[seat];
        s = Seat();
        s.used = true;
        s.bot = true;
        // a name not already at the table
        for (int tries = 0; tries < 30; tries++) {
            std::string n = BOT_NAMES[rng().range(0, BOT_NAME_COUNT - 1)];
            bool used = false;
            for (auto& o : t_.seats) used |= o.used && o.name == n && &o != &s;
            if (!used) { s.name = n; break; }
        }
        if (s.name.empty()) s.name = "Гость";
        s.style = rng().range(0, BOT_STYLES - 1);
        s.color = rng().range(0, PLAYER_COLORS - 1);
        s.stack = BUYINS[save::data().settings.pokerBuyIn];
    }

    // ===================================================================== hand flow
    void newHand() {
        board_.clear();
        for (auto& h : holes_) h.clear();
        revealed_.assign(SEATS, false);
        winners_.clear();
        handNames_.assign(SEATS, "");
        bestCards_.clear();
        potShown_ = 0;
        raising_ = false;
        if (!t_.startHand()) {
            gameOver();
            return;
        }
        phase_ = DEALING;
        // blinds slide out
        for (int i : {t_.sbSeat, t_.bbSeat}) {
            if (i < 0) continue;
            flash(i);
            fly(t_.seats[i].streetBet, PLATE[i], BETPOS[i], 0, 0.35f);
        }
        audio::play(audio::SFX_CHIP, 0.8f);
        float g = save::data().settings.fastDeal ? 0.09f : 0.16f;
        float d = 0.3f;
        for (int round = 0; round < 2; round++) {
            int i = t_.sbSeat;
            for (int k = 0; k < SEATS; k++, i = (i + 1) % SEATS) {
                if (!t_.seats[i].inHand) continue;
                int seat = i, r = round;
                timers_.add(d, [this, seat, r] { dealHole(seat, r); });
                d += g;
            }
        }
        seq_.wait(d + 0.35f);
        seq_.then(0.01f, [this] {
            phase_ = BETTING;
            beginTurn();
        });
    }

    Pt holePos(int seat, int k, float* angle, float* scale) const {
        Pt p = PLATE[seat];
        bool top = p.y < 300;
        float sc = seat == 0 ? 0.74f : 0.56f;
        float spread = seat == 0 ? 30.f : 20.f;
        float y = top ? p.y + 70 : p.y - (seat == 0 ? 92 : 64);
        if (angle) *angle = (k == 0 ? -6.f : 6.f);
        if (scale) *scale = sc;
        return {p.x + (k == 0 ? -spread / 2 : spread / 2), y};
    }

    void dealHole(int seat, int k) {
        CardSprite c;
        c.card = t_.seats[seat].hole[k];
        c.place(640, 250, 0, 0, 0.5f);
        float a, sc;
        Pt p = holePos(seat, k, &a, &sc);
        c.moveTo(p.x, p.y, a, 0, sc, 0.3f);
        holes_[seat].push_back(c);
        audio::play(audio::SFX_CARD_SLIDE, 0.6f, panFor(p.x), 1.05f);
    }

    bool showOwnCards(int seat) const {
        const Seat& s = t_.seats[seat];
        if (s.bot) return false;
        if (!hide_) return true;
        return peek_[seat];
    }

    void updatePeek() {
        for (int i = 0; i < SEATS; i++) {
            peek_[i] = false;
            const Seat& s = t_.seats[i];
            if (!s.used || s.bot) continue;
            int pad = input::padForSeat(s.order);
            bool mine = pad != ANY_PAD || t_.toAct == i;
            peek_[i] = mine && input::held(pad, BTN_R | BTN_ZR);
        }
        // apply desired card faces
        for (int i = 0; i < SEATS; i++) {
            float want = (revealed_[i] || showOwnCards(i)) ? 1.f : 0.f;
            for (auto& c : holes_[i])
                if (c.tf != want && !c.moving()) c.moveTo(c.tx, c.ty, c.ta, want, c.ts, 0.16f);
        }
    }

    void beginTurn() {
        if (t_.onlyOneLeft()) { endUncontested(); return; }
        if (t_.toAct < 0) { endStreet(); return; }
        int i = t_.toAct;
        if (t_.seats[i].bot) {
            float think = save::data().settings.fastDeal ? rng().uniform(0.35f, 0.7f) : rng().uniform(0.7f, 1.6f);
            thinking_ = i;
            Action a = botDecide(t_, i, rng());
            seq_.then(think, [this, i, a] {
                thinking_ = -1;
                act(i, a);
            });
        } else {
            raising_ = false;
            Legal l = t_.legal(i);
            raiseTo_ = l.minRaiseTo;
            audio::play(audio::SFX_KNOCK, 0.5f, panFor(PLATE[i].x));
        }
    }

    void act(int i, Action a) {
        i64 before = t_.seats[i].streetBet;
        t_.apply(i, a);
        Seat& s = t_.seats[i];
        flash(i);
        if (s.folded) {
            for (auto& c : holes_[i]) c.moveTo(640, 260, rng().uniform(-30, 30), 0, 0.45f, 0.35f);
            timers_.add(0.36f, [this, i] { holes_[i].clear(); });
            audio::play(audio::SFX_CARD_SLIDE, 0.6f, panFor(PLATE[i].x), 0.8f);
        } else if (s.streetBet > before) {
            fly(s.streetBet - before, PLATE[i], BETPOS[i], 0, 0.3f);
            audio::play(s.allIn ? audio::SFX_CHIPS : audio::SFX_CHIP, 0.9f, panFor(PLATE[i].x));
            if (s.allIn) caption_.say(s.name + " идёт олл-ин!", 1.8f);
        } else {
            audio::play(audio::SFX_KNOCK, 0.8f, panFor(PLATE[i].x));
        }
        seq_.then(0.35f, [this] { beginTurn(); });
    }

    void endStreet() {
        // collect bets into the pot
        bool any = false;
        for (int i = 0; i < SEATS; i++) {
            if (t_.seats[i].streetBet > 0) {
                fly(t_.seats[i].streetBet, BETPOS[i], POT, 0, 0.35f);
                any = true;
            }
        }
        if (any) audio::play(audio::SFX_CHIPS, 0.5f);
        i64 pot = t_.potTotal();
        Street prev = t_.street;
        if (prev == RIVER) {
            for (auto& s : t_.seats) s.streetBet = 0; // shown as collected; totals stay for pots
            seq_.then(any ? 0.4f : 0.1f, [this, pot] { potShown_ = pot; showdown(); });
            return;
        }
        Street st = t_.nextStreet();
        int from = (int)t_.board.size() - (st == FLOP ? 3 : 1);
        seq_.then(any ? 0.4f : 0.15f, [this, pot] { potShown_ = pot; });
        for (int k = from; k < (int)t_.board.size(); k++) {
            seq_.then(k == from ? 0.25f : 0.22f, [this, k] { dealBoard(k); });
        }
        seq_.wait(0.45f);
        static const char* names[] = {"", "Флоп", "Тёрн", "Ривер"};
        caption_.say(names[st], 1.4f);
        if (t_.canActCount() < 2) {
            // everyone is all-in: reveal and run the board out
            phase_ = RUNOUT;
            seq_.then(0.2f, [this] { revealAll(); });
            seq_.then(1.0f, [this] { endStreet(); });
        } else {
            seq_.then(0.05f, [this] {
                phase_ = BETTING;
                beginTurn();
            });
        }
    }

    void dealBoard(int k) {
        CardSprite c;
        c.card = t_.board[k];
        c.place(640, 210, 0, 0, 0.5f);
        c.moveTo(640 + (k - 2) * 82.f, BOARD_Y, 0, 1, 0.74f, 0.3f);
        board_.push_back(c);
        audio::play(audio::SFX_CARD_SLIDE, 0.7f, (k - 2) * 0.15f);
        timers_.add(0.22f, [] { audio::play(audio::SFX_CARD_FLIP, 0.6f); });
    }

    void revealAll() {
        for (int i = 0; i < SEATS; i++)
            if (t_.seats[i].live()) revealed_[i] = true;
        audio::play(audio::SFX_CARD_FLIP, 0.8f);
    }

    void showdown() {
        phase_ = SHOWDOWN_PH;
        revealAll();
        for (int i = 0; i < SEATS; i++) {
            if (!t_.seats[i].live()) continue;
            std::vector<Card> c(t_.board.begin(), t_.board.end());
            c.push_back(t_.seats[i].hole[0]);
            c.push_back(t_.seats[i].hole[1]);
            handNames_[i] = describe(evaluate(c.data(), (int)c.size()));
        }
        seq_.wait(1.1f);
        seq_.then(0.01f, [this] { award(); });
    }

    void endUncontested() {
        for (int i = 0; i < SEATS; i++)
            if (t_.seats[i].streetBet > 0) fly(t_.seats[i].streetBet, BETPOS[i], POT, 0, 0.3f);
        i64 pot = t_.potTotal();
        for (auto& s : t_.seats) s.streetBet = 0;
        seq_.then(0.4f, [this, pot] { potShown_ = pot; award(); });
    }

    void award() {
        bool showdown = !t_.onlyOneLeft();
        auto awards = t_.finish();
        std::vector<i64> won(SEATS, 0);
        for (auto& a : awards) won[a.seat] += a.amount;
        float d = 0;
        int best = -1;
        for (int i = 0; i < SEATS; i++) {
            if (won[i] <= 0) continue;
            winners_.push_back(i);
            fly(won[i], POT, PLATE[i], d, 0.55f);
            d += 0.12f;
            if (best < 0 || won[i] > won[best]) best = i;
        }
        timers_.add(0.1f, [this] { potShown_ = 0; });
        audio::play(audio::SFX_CHIPS, 0.9f);
        if (best >= 0) {
            const Seat& w = t_.seats[best];
            std::string sub = showdown ? handNames_[best] : "все сбросили карты";
            banner_.show(w.name + " +" + fmtMoney(won[best]), sub, 2.6f, true);
            if (!w.bot) audio::play(audio::SFX_WIN, 0.8f);
            if (showdown) {
                std::vector<Card> c(t_.board.begin(), t_.board.end());
                c.push_back(w.hole[0]);
                c.push_back(w.hole[1]);
                bestCards_ = bestFive(c.data(), (int)c.size());
            }
        }
        phase_ = HAND_END;
        endTimer_ = 0;
        // persist human stacks after every hand
        for (int i = 0; i < SEATS; i++) {
            Seat& s = t_.seats[i];
            if (s.used && !s.bot) save::player(s.profile).stake = s.stack;
        }
        save::store();
    }

    void afterHand() {
        phase_ = DEALING;
        // busted bots leave, new ones sit down; busted humans may rebuy
        std::vector<int> bustedHumans;
        for (int i = 0; i < SEATS; i++) {
            Seat& s = t_.seats[i];
            if (!s.used || s.stack > 0) continue;
            if (s.bot) {
                std::string old = s.name;
                seatBot(i);
                ui::toast(old + " покидает стол, садится " + t_.seats[i].name);
            } else {
                bustedHumans.push_back(i);
            }
        }
        if (leaveAfter_) { leave(); return; }
        askRebuy(bustedHumans, 0);
    }

    void askRebuy(std::vector<int> list, size_t k) {
        while (k < list.size()) {
            int i = list[k];
            Seat& s = t_.seats[i];
            Profile& p = save::player(s.profile);
            i64 bi = std::min(BUYINS[save::data().settings.pokerBuyIn], p.balance);
            if (s.used && !s.bot && s.stack == 0 && bi >= t_.bb * 10) {
                dialog_.show("ДОКУПИТЬ ФИШКИ?", s.name + ", фишки закончились.\nКупить ещё " + fmtMoney(bi) + "?",
                             {"Встать", "Докупить"},
                             [this, list, k, i, bi](int pick) {
                                 Seat& ss = t_.seats[i];
                                 if (pick == 1) {
                                     Profile& pp = save::player(ss.profile);
                                     pp.balance -= bi;
                                     ss.stack = bi;
                                     pp.stake = bi;
                                     boughtIn_[i] += bi;
                                     audio::play(audio::SFX_CHIPS);
                                 } else {
                                     standUp(i);
                                 }
                                 save::store();
                                 askRebuy(list, k + 1);
                             },
                             input::padForSeat(s.order), 1);
                return;
            }
            if (s.used && !s.bot && s.stack == 0) standUp(i);
            k++;
        }
        int humans = 0;
        for (auto& s : t_.seats) humans += s.used && !s.bot && s.stack > 0;
        if (humans == 0) { gameOver(); return; }
        seq_.then(0.3f, [this] { newHand(); });
    }

    void standUp(int i) {
        Seat& s = t_.seats[i];
        if (!s.used || s.bot) return;
        Profile& p = save::player(s.profile);
        p.balance += s.stack;
        save::recordResult(s.profile, s.stack - boughtIn_[i]);
        p.stake = 0;
        boughtIn_[i] = 0;
        s = Seat();
    }

    void cashOutAll() {
        for (int i = 0; i < SEATS; i++) standUp(i);
        save::store();
    }

    void leave() {
        cashOutAll();
        app::go(SC_HALL);
    }

    void gameOver() {
        phase_ = GAME_OVER;
        cashOutAll();
        dialog_.show("ИГРА ОКОНЧЕНА", "За столом не осталось игроков с фишками.", {"В зал", "Новая игра"}, [this](int pick) {
            if (pick == 1) {
                t_ = Table();
                buildLobby();
                phase_ = LOBBY;
            } else {
                app::go(SC_HALL);
            }
        });
    }

    // ===================================================================== human input
    void updateBetting(float dt) {
        int i = t_.toAct;
        if (i < 0 || t_.seats[i].bot) return;
        Seat& s = t_.seats[i];
        Legal l = t_.legal(i);
        int pad = input::padForSeat(s.order);
        if (raising_) {
            i64 step = t_.bb;
            i64 potStep = std::max(step, (t_.potTotal() + l.toCall) / 4 / step * step);
            if (input::repeat(pad, BTN_RIGHT)) raiseTo_ += step;
            if (input::repeat(pad, BTN_LEFT)) raiseTo_ -= step;
            if (input::repeat(pad, BTN_UP)) raiseTo_ += potStep;
            if (input::repeat(pad, BTN_DOWN)) raiseTo_ -= potStep;
            if (input::pressed(pad, BTN_Y)) raiseTo_ = l.maxRaiseTo;
            i64 clamped = std::clamp(raiseTo_, l.minRaiseTo, l.maxRaiseTo);
            if (clamped != raiseTo_ || input::repeat(pad, BTN_DIRS)) {
                if (clamped == raiseTo_) audio::play(audio::SFX_TICK, 0.6f);
            }
            raiseTo_ = clamped;
            if (input::pressed(pad, BTN_A)) {
                raising_ = false;
                act(i, {raiseTo_ >= l.maxRaiseTo ? ALLIN : RAISE, raiseTo_});
            } else if (input::pressed(pad, BTN_B)) {
                raising_ = false;
                audio::play(audio::SFX_BACK);
            }
            return;
        }
        (void)dt;
        if (input::pressed(pad, BTN_A)) {
            act(i, {l.canCheck ? CHECK : CALL, 0});
        } else if (input::pressed(pad, BTN_B)) {
            if (l.canCheck) {
                dialog_.show("СБРОСИТЬ КАРТЫ?", "Можно бесплатно сказать «чек».", {"Чек", "Фолд"},
                             [this, i](int pick) {
                                 if (pick == 1) act(i, {FOLD, 0});
                                 else if (pick == 0) act(i, {CHECK, 0});
                             },
                             pad);
            } else {
                act(i, {FOLD, 0});
            }
        } else if (input::pressed(pad, BTN_X) && l.canRaise) {
            raising_ = true;
            raiseTo_ = l.minRaiseTo;
            audio::play(audio::SFX_SELECT);
        } else if (input::pressed(pad, BTN_Y)) {
            dialog_.show("ОЛЛ-ИН?", s.name + " ставит всё: " + fmtMoney(s.stack + s.streetBet), {"Отмена", "Олл-ин"},
                         [this, i](int pick) {
                             if (pick == 1) act(i, {ALLIN, 0});
                         },
                         pad);
        }
    }

    // ===================================================================== drawing
    void fly(i64 amount, Pt from, Pt to, float delay, float dur) {
        if (amount <= 0) return;
        FlyingChips f;
        f.amount = amount;
        f.fx = from.x; f.fy = from.y; f.tx = to.x; f.ty = to.y;
        f.delay = delay;
        f.dur = dur;
        f.scale = 0.55f;
        flying_.push_back(f);
    }

    void flash(int i) { actionFlash_[i] = 2.2f; }

    void drawSeat(int i) {
        const Seat& s = t_.seats[i];
        Pt p = PLATE[i];
        if (!s.used) {
            gfx::roundRect(p.x - 70, p.y - 24, 140, 48, 10, Color(0, 0, 0, 90));
            gfx::text("свободно", p.x, p.y, F_SANS, 15, pal::dim, 0);
            return;
        }
        // hole cards behind the plate for bottom seats, in front for top seats
        bool top = p.y < 300;
        if (!top) for (auto& c : holes_[i]) drawHole(i, c);
        bool acting = t_.toAct == i && phase_ == BETTING && !seq_.busy();
        bool isWinner = std::find(winners_.begin(), winners_.end(), i) != winners_.end() && phase_ == HAND_END;
        float a = (s.inHand && s.folded) || (!s.inHand && phase_ != LOBBY) ? 0.55f : 1.f;
        float w = 196, h = 58;
        float pulse = 0.5f + 0.5f * std::sin(time_ * 5);
        if (acting || thinking_ == i) gfx::glowEllipse(p.x, p.y, w * 0.8f, h * 1.4f, pal::gold, 0.2f + 0.12f * pulse);
        if (isWinner) gfx::glowEllipse(p.x, p.y, w, h * 1.8f, Color::hex(0xffd36a), 0.35f + 0.15f * pulse);
        gfx::roundRect(p.x - w / 2, p.y - h / 2, w, h, 10, Color(10, 8, 11, (uint8_t)(235 * a)));
        gfx::roundRectOutline(p.x - w / 2, p.y - h / 2, w, h, 10, acting ? 2.f : 1.f,
                              acting ? pal::gold : Color(255, 255, 255, (uint8_t)(50 * a)));
        float ax = p.x - w / 2 + 30;
        if (s.bot) {
            Color c = playerColor(s.color).scaled(0.7f);
            gfx::circle(ax, p.y, 21, Color(0, 0, 0, 160));
            gfx::circle(ax, p.y, 19, c.alpha(a));
            gfx::ring(ax, p.y, 19, 1.4f, pal::gold.alpha(0.7f * a));
            std::string ini = s.name.substr(0, (unsigned char)s.name[0] >= 0xC0 ? 2 : 1);
            gfx::text(ini, ax, p.y, F_SERIF, 20, pal::ivory, 0, a);
        } else {
            gfx::drawCentered(art::playerChip(s.color), ax, p.y, 0.86f, 0, a);
        }
        gfx::text(s.name, ax + 28, p.y - 11, F_SANS_BOLD, 17, pal::ivory, -1, a);
        std::string stack = s.stack > 0 || s.inHand ? fmtMoney(s.stack) : "нет фишек";
        if (s.allIn && s.inHand && !s.folded) stack = "ОЛЛ-ИН";
        gfx::text(stack, ax + 28, p.y + 11, F_NUM, 16, s.allIn ? pal::redBright : pal::goldLight, -1, a);
        if (s.bot) gfx::text(botStyle(s.style).label, p.x + w / 2 - 10, p.y + 12, F_SANS, 12, pal::dim, 1, a);
        else if (input::connectedCount() > 1 && input::connected(s.order))
            gfx::text(strf("%d", s.order + 1), p.x + w / 2 - 10, p.y - 13, F_NUM, 12, pal::muted, 1);
        // action bubble
        std::string bubble;
        Color bc = pal::ivory;
        if (thinking_ == i) {
            int dots = (int)(time_ * 3) % 4;
            bubble = "думает" + std::string(dots, '.');
            bc = pal::muted;
        } else if (!s.lastAction.empty() && (actionFlash_[i] > 0 || s.folded || s.allIn) && s.inHand) {
            bubble = s.lastAction;
            if (s.folded) bc = pal::muted;
            else if (s.allIn) bc = pal::redBright;
        }
        if (phase_ == SHOWDOWN_PH || phase_ == HAND_END) {
            if (!handNames_[i].empty()) { bubble = handNames_[i]; bc = isWinner ? pal::goldLight : pal::cream; }
        }
        if (!bubble.empty()) {
            float bw = gfx::textWidth(bubble, F_SANS_BOLD, 14) + 20;
            float by = top ? p.y - h / 2 - 14 : p.y + h / 2 + 12;
            if (i == 0) by = p.y - h / 2 - 12;
            gfx::roundRect(p.x - bw / 2, by - 11, bw, 22, 11, Color(0, 0, 0, 210));
            gfx::text(bubble, p.x, by, F_SANS_BOLD, 14, bc, 0);
        }
        if (top) for (auto& c : holes_[i]) drawHole(i, c);
    }

    void drawHole(int seat, const CardSprite& c) {
        c.draw();
        // highlight the winning five
        if (phase_ == HAND_END && !bestCards_.empty() && c.face > 0.5f &&
            std::find(bestCards_.begin(), bestCards_.end(), c.card) != bestCards_.end() &&
            std::find(winners_.begin(), winners_.end(), seat) != winners_.end())
            art::cardHighlight(c.x, c.y, c.scale, pal::goldLight, 0.8f);
    }

    void drawHud() {
        gfx::rectGrad(0, 660, SCREEN_W, 60, Color(0, 0, 0, 0), Color(0, 0, 0, 170));
        // winning board cards
        if (phase_ == HAND_END && !bestCards_.empty())
            for (auto& c : board_)
                if (std::find(bestCards_.begin(), bestCards_.end(), c.card) != bestCards_.end())
                    art::cardHighlight(c.x, c.y, c.scale, pal::goldLight, 0.8f);
        if (pause_.open || dialog_.open) return;
        if (phase_ == HAND_END) {
            ui::hints({{BTN_A, "Следующая раздача"}, {BTN_PLUS, "Меню"}}, 1262, 700);
            return;
        }
        int i = t_.toAct;
        if (phase_ != BETTING || seq_.busy() || i < 0 || t_.seats[i].bot) {
            if (hide_) ui::hints({{BTN_R, "Удерживать — свои карты"}, {BTN_PLUS, "Меню"}}, 1262, 700);
            return;
        }
        const Seat& s = t_.seats[i];
        Legal l = t_.legal(i);
        // who is acting + private hand strength
        float lx = 24;
        gfx::textShadow(s.name + ", ваш ход", lx, 676, F_SANS_BOLD, 21, pal::ivory, -1);
        if (showOwnCards(i)) {
            std::vector<Card> c(t_.board.begin(), t_.board.end());
            c.push_back(s.hole[0]);
            c.push_back(s.hole[1]);
            std::string desc = c.size() >= 5 ? describe(evaluate(c.data(), (int)c.size()))
                                             : (s.hole[0].rank == s.hole[1].rank ? std::string("Карманная пара") : "");
            if (!desc.empty()) gfx::text(desc, lx, 700, F_SANS, 16, pal::goldLight, -1);
        } else if (hide_) {
            gfx::text("Удерживайте R, чтобы посмотреть свои карты", lx, 700, F_SANS, 15, pal::muted, -1);
        }
        if (raising_) {
            float pw = 420, ph = 112, px = 1262 - pw, py = 520;
            ui::panel(px, py, pw, ph, 1.f);
            gfx::text(l.maxRaiseTo == raiseTo_ ? "ОЛЛ-ИН" : (t_.currentBet == 0 ? "БЕТ" : "РЕЙЗ ДО"), px + 22, py + 28,
                      F_SANS_BOLD, 16, pal::muted, -1);
            gfx::text(fmtMoney(raiseTo_), px + pw - 22, py + 30, F_NUM, 30, pal::goldLight, 1);
            float bx = px + 22, bw = pw - 44, by = py + 62;
            float k = l.maxRaiseTo > l.minRaiseTo ? (float)(raiseTo_ - l.minRaiseTo) / (l.maxRaiseTo - l.minRaiseTo) : 1.f;
            gfx::roundRect(bx, by - 3, bw, 6, 3, Color(255, 255, 255, 30));
            gfx::roundRect(bx, by - 3, bw * k, 6, 3, pal::gold);
            gfx::circle(bx + bw * k, by, 9, pal::goldLight);
            gfx::text(fmtMoney(l.minRaiseTo), bx, by + 22, F_NUM, 13, pal::muted, -1);
            gfx::text(fmtMoney(l.maxRaiseTo), bx + bw, by + 22, F_NUM, 13, pal::muted, 1);
            ui::hints({{BTN_LEFT | BTN_RIGHT, "±BB"}, {BTN_UP | BTN_DOWN, "±¼ банка"}, {BTN_Y, "Всё"}, {BTN_A, "OK"},
                       {BTN_B, "Отмена"}},
                      1262, 676, 1, 1.f, 15);
            return;
        }
        std::vector<std::pair<u32, std::string>> h;
        h.push_back({BTN_B, "Фолд"});
        h.push_back({BTN_A, l.canCheck ? std::string("Чек") : "Колл " + fmtMoney(l.toCall)});
        if (l.canRaise) h.push_back({BTN_X, t_.currentBet == 0 ? "Бет" : "Рейз"});
        h.push_back({BTN_Y, "Олл-ин"});
        if (hide_) h.push_back({BTN_R, "Карты"});
        ui::hints(h, 1262, 676, 1, 1.f, 17);
    }

    void openPause() {
        audio::play(audio::SFX_SELECT);
        pause_.rulesTitle = "ПРАВИЛА ХОЛДЕМА";
        pause_.rulesText = kRules;
        std::vector<ui::MenuItem> items;
        items.push_back({"Продолжить", nullptr, nullptr, [this] { pause_.close(); }});
        items.push_back({"Правила", nullptr, nullptr, [this] { pause_.rules = true; }});
        items.push_back({"Быстрая игра", [] { return std::string(save::data().settings.fastDeal ? "Вкл" : "Выкл"); },
                         [](int) { save::data().settings.fastDeal = !save::data().settings.fastDeal; save::store(); }, nullptr});
        items.push_back({"Покинуть стол", nullptr, nullptr, [this] {
                             pause_.close();
                             if (phase_ == HAND_END || phase_ == GAME_OVER) leave();
                             else {
                                 leaveAfter_ = true;
                                 ui::toast("Выйдем из-за стола после этой раздачи");
                             }
                         }});
        pause_.show(items);
    }

    Tex table_;
    Table t_;
    Phase phase_ = LOBBY;
    ui::Menu lobby_;
    std::vector<CardSprite> holes_[SEATS];
    std::vector<CardSprite> board_;
    std::vector<FlyingChips> flying_;
    std::vector<bool> revealed_ = std::vector<bool>(SEATS, false);
    std::vector<std::string> handNames_ = std::vector<std::string>(SEATS);
    std::vector<int> winners_;
    std::vector<Card> bestCards_;
    i64 boughtIn_[SEATS] = {};
    bool peek_[SEATS] = {};
    float actionFlash_[SEATS] = {};
    i64 potShown_ = 0;
    i64 raiseTo_ = 0;
    bool raising_ = false;
    bool hide_ = false;
    bool leaveAfter_ = false;
    int humansSeated_ = 0;
    int thinking_ = -1;
    float time_ = 0, endTimer_ = 0;
    Seq seq_;
    Timers timers_;
    art::Caption caption_;
    ui::Banner banner_;
    ui::PauseMenu pause_;
    ui::Dialog dialog_;
};

} // namespace

std::unique_ptr<Scene> makePokerScene() { return std::make_unique<PokerScene>(); }
