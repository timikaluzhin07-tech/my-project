// Settings: volumes, poker privacy, dealing speed, resets, credits.
#include "../art/backdrop.h"
#include "../core/app.h"
#include "../core/audio.h"
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/save.h"
#include "../core/ui.h"

namespace {

std::string pct(float v) { return strf("%d%%", (int)std::lround(v * 100)); }
float step(float v, int dir) { return std::clamp(std::round((v + dir * 0.1f) * 10) / 10.f, 0.f, 1.f); }

class SettingsScene : public Scene {
public:
    SettingsScene() {
        bg_ = gfx::upload(art::velvetBackdrop(Color::hex(0x101820), Color::hex(0xc9a24a), 9));
        Settings& s = save::data().settings;
        auto vol = [](float& f) {
            return [&f](int d) { f = step(f, d); app::applyVolumes(); save::store(); };
        };
        menu_.items.push_back({"Общая громкость", [&s] { return pct(s.master); }, vol(s.master), nullptr});
        menu_.items.push_back({"Музыка", [&s] { return pct(s.music); }, vol(s.music), nullptr});
        menu_.items.push_back({"Звуковые эффекты", [&s] { return pct(s.sfx); }, vol(s.sfx), nullptr});
        menu_.items.push_back({"Скрывать карты в покере",
                               [&s] {
                                   static const char* v[] = {"Авто", "Всегда", "Никогда"};
                                   return std::string(v[s.hideCards]);
                               },
                               [&s](int d) { s.hideCards = (s.hideCards + d + 3) % 3; save::store(); }, nullptr});
        menu_.items.push_back({"Быстрая раздача", [&s] { return std::string(s.fastDeal ? "Вкл" : "Выкл"); },
                               [&s](int) { s.fastDeal = !s.fastDeal; save::store(); }, nullptr});
        menu_.items.push_back({"Слот: ход переходит дальше", [&s] { return std::string(s.slotRotate ? "Вкл" : "Выкл"); },
                               [&s](int) { s.slotRotate = !s.slotRotate; save::store(); }, nullptr});
        menu_.items.push_back({"Сбросить фишки всех игроков", nullptr, nullptr, [this] {
                                   dialog_.show("СБРОС", "У всех игроков снова будет по " + fmtMoney(START_BALANCE) +
                                                             " фишек,\nстатистика обнулится.",
                                                {"Отмена", "Сбросить"}, [](int pick) {
                                                    if (pick != 1) return;
                                                    for (int i = 0; i < MAX_PLAYERS; i++) {
                                                        Profile& p = save::player(i);
                                                        p.balance = START_BALANCE;
                                                        p.biggestWin = p.totalWon = p.totalLost = 0;
                                                        p.refills = 0;
                                                    }
                                                    save::store();
                                                    ui::toast("Фишки сброшены", pal::greenBright);
                                                });
                               }});
        menu_.items.push_back({"Назад в зал", nullptr, nullptr, [] { app::go(SC_HALL); }});
    }

    void update(float dt) override {
        if (dialog_.update(dt)) return;
        menu_.update(ANY_PAD, dt);
        if (input::pressed(ANY_PAD, BTN_B | BTN_PLUS)) {
            audio::play(audio::SFX_BACK);
            save::store();
            app::go(SC_HALL);
        }
    }

    void render() override {
        gfx::draw(bg_, 0, 0);
        gfx::textGold("НАСТРОЙКИ", SCREEN_W / 2.f, 62, F_TITLE, 42);
        ui::rule(SCREEN_W / 2.f, 94, 420);
        float w = 640, x = (SCREEN_W - w) / 2, y = 124;
        float h = menu_.items.size() * 50 + 30;
        ui::panel(x - 10, y - 14, w + 20, h, 1.f);
        menu_.render(x, y, w, 50);
        const char* notes[] = {
            "", "", "",
            "Авто — карты прячутся, когда за покерным столом больше одного человека. Подсмотреть: удерживайте R.",
            "Карты летят быстрее, дилер не делает пауз.",
            "После каждого вращения автомат переходит к следующему игроку.", "", ""};
        const char* note = notes[std::clamp(menu_.sel, 0, 7)];
        if (note[0]) gfx::textWrapped(note, x, y + h + 4, w, F_SANS, 17, pal::muted);
        gfx::text("Grand Casino NX 1.0 · только виртуальные фишки, без реальных денег", SCREEN_W / 2.f, 640, F_SANS, 15,
                  pal::dim, 0);
        gfx::text("Шрифты Playfair Display, PT Sans, Oswald (SIL OFL) · nanosvg (zlib) · SDL2", SCREEN_W / 2.f, 660,
                  F_SANS, 13, pal::dim, 0);
        ui::hints({{BTN_UP | BTN_DOWN, "Выбор"}, {BTN_LEFT | BTN_RIGHT, "Изменить"}, {BTN_B, "Назад"}}, 1262, 700);
        dialog_.render();
    }

private:
    Tex bg_;
    ui::Menu menu_;
    ui::Dialog dialog_;
};

} // namespace

std::unique_ptr<Scene> makeSettingsScene() { return std::make_unique<SettingsScene>(); }
