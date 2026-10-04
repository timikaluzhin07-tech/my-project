// Trophy cabinet: every achievement, who has it, and how far each player got.
#include "../art/backdrop.h"
#include "../core/app.h"
#include "../core/audio.h"
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/save.h"
#include "../core/trophy.h"
#include "../core/ui.h"

namespace {

constexpr int COLS = 5;

class TrophiesScene : public Scene {
public:
    TrophiesScene() { bg_ = gfx::upload(art::velvetBackdrop(Color::hex(0x141018), Color::hex(0xc9a24a), 13)); }

    void update(float dt) override {
        t_ += dt;
        int n = trophy::COUNT;
        if (input::repeat(ANY_PAD, BTN_LEFT) && sel_ % COLS > 0) { sel_--; audio::play(audio::SFX_NAV); }
        if (input::repeat(ANY_PAD, BTN_RIGHT) && sel_ % COLS < COLS - 1 && sel_ + 1 < n) { sel_++; audio::play(audio::SFX_NAV); }
        if (input::repeat(ANY_PAD, BTN_UP) && sel_ >= COLS) { sel_ -= COLS; audio::play(audio::SFX_NAV); }
        if (input::repeat(ANY_PAD, BTN_DOWN) && sel_ + COLS < n) { sel_ += COLS; audio::play(audio::SFX_NAV); }
        if (input::pressed(ANY_PAD, BTN_B | BTN_PLUS)) {
            audio::play(audio::SFX_BACK);
            app::go(SC_HALL);
        }
    }

    void render() override {
        gfx::draw(bg_, 0, 0);
        gfx::textGold("ТРОФЕИ", SCREEN_W / 2.f, 50, F_TITLE, 42);
        ui::rule(SCREEN_W / 2.f, 82, 420);
        // per-player tally
        auto act = save::active();
        float tx = SCREEN_W / 2.f - act.size() * 170 / 2.f;
        for (size_t k = 0; k < act.size(); k++) {
            const Profile& p = save::player(act[k]);
            float x = tx + k * 170;
            ui::chipDot(x + 12, 106, 8, playerColor(p.color));
            gfx::text(p.name, x + 26, 106, F_SANS_BOLD, 15, pal::ivory, -1);
            gfx::text(strf("%d/%d", trophy::count(act[k]), (int)trophy::COUNT), x + 150, 106, F_NUM, 15, pal::goldLight, 1);
        }
        const Tex& medal = trophy::medal();
        for (int i = 0; i < trophy::COUNT; i++) {
            int c = i % COLS, r = i / COLS;
            float x = 36 + c * 244, y = 128 + r * 112, w = 232, h = 102;
            bool any = false;
            for (int pr = 0; pr < MAX_PLAYERS; pr++) any |= trophy::has(pr, (trophy::Id)i);
            bool s = i == sel_;
            if (s) gfx::glowEllipse(x + w / 2, y + h / 2, w * 0.7f, h, pal::gold, 0.18f + 0.06f * std::sin(t_ * 4));
            ui::plate(x, y, w, h, s, 1.f);
            float bob = any ? 2 * std::sin(t_ * 2 + i) : 0;
            gfx::drawCentered(medal, x + 38, y + h / 2 - 2 + bob, 0.82f, 0, any ? 1.f : 0.5f,
                              any ? pal::white : Color(60, 56, 60));
            const trophy::Info& in = trophy::info((trophy::Id)i);
            float ts = 16;
            while (ts > 12 && gfx::textWidth(in.title, F_SERIF, ts) > w - 84) ts -= 0.5f;
            if (any) gfx::textGold(in.title, x + 76, y + 22, F_SERIF, ts, -1, 1.f, true);
            else gfx::text(in.title, x + 76, y + 22, F_SERIF, ts, pal::muted, -1);
            gfx::textWrapped(in.desc, x + 76, y + 34, w - 86, F_SANS, 12.5f, any ? pal::cream : pal::dim, 1.15f);
            // who owns it
            float dx = x + 82;
            for (int pr = 0; pr < MAX_PLAYERS; pr++)
                if (trophy::has(pr, (trophy::Id)i)) {
                    ui::chipDot(dx, y + h - 13, 6, playerColor(save::player(pr).color));
                    dx += 16;
                }
        }
        // details for the selected trophy
        const trophy::Info& in = trophy::info((trophy::Id)sel_);
        std::string who;
        for (int pr = 0; pr < MAX_PLAYERS; pr++)
            if (trophy::has(pr, (trophy::Id)sel_)) who += (who.empty() ? "" : ", ") + save::player(pr).name;
        gfx::text(std::string(in.title) + " — " + (who.empty() ? "пока ни у кого" : who), 40, 690, F_SANS_BOLD, 17,
                  who.empty() ? pal::muted : pal::goldLight, -1);
        ui::hints({{BTN_DIRS, "Выбор"}, {BTN_B, "Назад"}}, 1262, 690);
    }

private:
    Tex bg_;
    int sel_ = 0;
    float t_ = 0;
};

} // namespace

std::unique_ptr<Scene> makeTrophiesScene() { return std::make_unique<TrophiesScene>(); }
