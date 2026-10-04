// Player profiles: who sits at the tables, names, colours, cashier.
#include "../art/art.h"
#include "../art/backdrop.h"
#include "../core/app.h"
#include "../core/audio.h"
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/platform.h"
#include "../core/save.h"
#include "../core/ui.h"

namespace {

class PlayersScene : public Scene {
public:
    PlayersScene() { bg_ = gfx::upload(art::velvetBackdrop(Color::hex(0x2a0a10), Color::hex(0xc9a24a), 5)); }

    void update(float dt) override {
        t_ += dt;
        selAnim_ = approach(selAnim_, (float)sel_, 16, dt);
        if (dialog_.update(dt)) return;
        int col = sel_ % 3, row = sel_ / 3;
        if (input::repeat(ANY_PAD, BTN_LEFT) && col > 0) { sel_--; audio::play(audio::SFX_NAV); }
        if (input::repeat(ANY_PAD, BTN_RIGHT) && col < 2) { sel_++; audio::play(audio::SFX_NAV); }
        if (input::repeat(ANY_PAD, BTN_UP) && row > 0) { sel_ -= 3; audio::play(audio::SFX_NAV); }
        if (input::repeat(ANY_PAD, BTN_DOWN) && row < 1) { sel_ += 3; audio::play(audio::SFX_NAV); }
        Profile& p = save::player(sel_);
        if (input::pressed(ANY_PAD, BTN_A)) {
            if (p.active && save::active().size() <= 1) {
                audio::play(audio::SFX_ERROR);
                ui::toast("За столом должен остаться хотя бы один игрок", pal::redBright);
            } else {
                p.active = !p.active;
                audio::play(p.active ? audio::SFX_CHIP : audio::SFX_BACK);
                save::store();
            }
        } else if (input::pressed(ANY_PAD, BTN_Y)) {
            std::string name;
            audio::play(audio::SFX_SELECT);
            if (platform::editText("Имя игрока", p.name, 14, name)) {
                p.name = name;
                save::store();
            }
            input::consume();
        } else if (input::pressed(ANY_PAD, BTN_X)) {
            int c = p.color;
            for (int k = 1; k <= PLAYER_COLORS; k++) {
                int cand = (p.color + k) % PLAYER_COLORS;
                bool used = false;
                for (int i = 0; i < MAX_PLAYERS; i++)
                    if (i != sel_ && save::player(i).color == cand) used = true;
                if (!used) { c = cand; break; }
            }
            p.color = c;
            audio::play(audio::SFX_CHIP);
            save::store();
        } else if (input::pressed(ANY_PAD, BTN_MINUS)) {
            cashier(p);
        } else if (input::pressed(ANY_PAD, BTN_R)) {
            audio::play(audio::SFX_SELECT);
            if (platform::isSwitch()) platform::controllerSetup((int)save::active().size());
            else ui::toast("Настройка контроллеров доступна на Switch");
            input::consume();
        } else if (input::pressed(ANY_PAD, BTN_B | BTN_PLUS)) {
            audio::play(audio::SFX_BACK);
            save::store();
            app::go(SC_HALL);
        }
    }

    void render() override {
        gfx::draw(bg_, 0, 0);
        gfx::textGold("ИГРОКИ", SCREEN_W / 2.f, 56, F_TITLE, 42);
        ui::rule(SCREEN_W / 2.f, 88, 420);
        gfx::text("Посадите за стол до шести человек — каждому свои фишки и свой цвет", SCREEN_W / 2.f, 108,
                  F_SANS, 18, pal::cream, 0, 0.85f);
        auto act = save::active();
        for (int i = 0; i < MAX_PLAYERS; i++) {
            int seat = -1;
            for (size_t k = 0; k < act.size(); k++)
                if (act[k] == i) seat = (int)k;
            drawCard(i, seat);
        }
        // explanation of controller mapping
        float y = 528;
        ui::panel(50, y, SCREEN_W - 100, 108, 0.9f, false);
        gfx::text("Как играть вдвоём и больше", 74, y + 24, F_SANS_BOLD, 19, pal::goldLight);
        gfx::textWrapped(
            "Игрок №1 управляет контроллером с первой лампочкой, игрок №2 — со второй и так далее. "
            "Один Joy-Con, повёрнутый боком, — тоже полноценный контроллер (R — экран настройки Joy-Con). "
            "Если контроллер один, передавайте его по очереди: игра всегда подскажет, чей ход.",
            74, y + 40, SCREEN_W - 150, F_SANS, 17, pal::cream);
        ui::hints({{BTN_A, "Сесть / встать"}, {BTN_Y, "Имя"}, {BTN_X, "Цвет"}, {BTN_MINUS, "Касса"},
                   {BTN_R, "Контроллеры"}, {BTN_B, "Назад"}},
                  1262, 692);
        dialog_.render();
    }

private:
    void cashier(Profile& p) {
        if (p.balance >= START_BALANCE) {
            audio::play(audio::SFX_ERROR);
            ui::toast("Касса пополняет баланс, когда фишек меньше " + fmtMoney(START_BALANCE));
            return;
        }
        i64 add = START_BALANCE - p.balance;
        dialog_.show("КАССА", p.name + " получит " + fmtMoney(add) + " фишек.\nБаланс станет " + fmtMoney(START_BALANCE) + ".",
                     {"Отмена", "Получить"}, [this, add](int pick) {
                         if (pick != 1) return;
                         Profile& pp = save::player(sel_);
                         pp.balance += add;
                         pp.refills++;
                         save::store();
                         audio::play(audio::SFX_CHIPS);
                         ui::toast("+" + fmtMoney(add) + " фишек", pal::greenBright);
                     });
    }

    void drawCard(int i, int seat) {
        const Profile& p = save::player(i);
        float w = 380, h = 170;
        float x = 50 + (i % 3) * (w + 20), y = 136 + (i / 3) * (h + 22);
        bool s = i == sel_;
        float a = p.active ? 1.f : 0.62f;
        if (s) gfx::glowEllipse(x + w / 2, y + h / 2, w * 0.65f, h * 0.9f, pal::gold, 0.16f);
        ui::panel(x, y, w, h, 1.f, s);
        if (!s) gfx::roundRectOutline(x, y, w, h, 9, 1, Color(255, 255, 255, 30));
        float bob = s ? std::sin(t_ * 3) * 2 : 0;
        art::drawPlayerChipStack(p.color, p.active ? 4 : 1, x + 56, y + 78 + bob, 1.45f, a);
        gfx::text(p.name, x + 112, y + 40, F_SERIF, 28, pal::ivory, -1, a);
        if (seat >= 0) {
            gfx::text(strf("ЗА СТОЛОМ  ·  МЕСТО %d", seat + 1), x + 112, y + 70, F_SANS_BOLD, 15, pal::greenBright, -1);
            if (input::connected(seat) && input::connectedCount() > 1)
                gfx::text(strf("контроллер %d", seat + 1), x + w - 16, y + 70, F_SANS, 15, pal::muted, 1);
        } else {
            gfx::text("ОТДЫХАЕТ", x + 112, y + 70, F_SANS_BOLD, 15, pal::muted, -1);
        }
        gfx::text(fmtMoney(p.balance), x + 112, y + 104, F_NUM, 30, pal::goldLight, -1, a);
        float bw = gfx::textWidth(fmtMoney(p.balance), F_NUM, 30);
        gfx::text("фишек", x + 118 + bw, y + 108, F_SANS, 16, pal::muted, -1, a);
        std::string stats = "Лучший выигрыш: " + fmtMoney(p.biggestWin);
        if (p.refills) stats += strf("  ·  Касса: %d", p.refills);
        gfx::text(stats, x + 112, y + 142, F_SANS, 15, pal::muted, -1, a);
        gfx::text(playerColorName(p.color), x + w - 16, y + 40, F_SANS, 14, playerColor(p.color).scaled(1.3f), 1, a);
    }

    Tex bg_;
    int sel_ = 0;
    float selAnim_ = 0, t_ = 0;
    ui::Dialog dialog_;
};

} // namespace

std::unique_ptr<Scene> makePlayersScene() { return std::make_unique<PlayersScene>(); }
