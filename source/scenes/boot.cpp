// Splash screen: logo reveal while the shared deck and chips are painted.
#include "../art/art.h"
#include "../core/app.h"
#include "../core/audio.h"
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/ui.h"

namespace {

class BootScene : public Scene {
public:
    void update(float dt) override {
        t_ += dt;
        if (!chimed_ && t_ > 0.25f) {
            audio::play(audio::SFX_WIN, 0.5f);
            chimed_ = true;
        }
        int total = art::buildSteps();
        for (int k = 0; k < 4 && step_ < total; k++) art::buildStep(step_++);
        bool done = step_ >= total;
        if (done && !left_ && (t_ > 2.6f || input::pressed(ANY_PAD, BTN_A | BTN_B | BTN_PLUS))) {
            left_ = true;
            app::go(SC_HALL);
        }
    }

    audio::Music music() const override { return audio::MUS_NONE; }
    bool ambience() const override { return false; }

    void render() override {
        gfx::rect(0, 0, SCREEN_W, SCREEN_H, Color::hex(0x07060a));
        float a = ease::outCubic(clamp01((t_ - 0.1f) / 1.2f));
        float cx = SCREEN_W / 2.f, cy = 320;
        gfx::glowEllipse(cx, cy, 420 * a, 120, Color::hex(0xffb54a), 0.18f * a);
        gfx::textGold("GRAND CASINO", cx, cy, F_TITLE, 74, 0, a);
        float rw = 520 * ease::inOutCubic(clamp01((t_ - 0.5f) / 1.0f));
        ui::rule(cx, cy + 58, rw, a);
        gfx::text("N  X", cx, cy + 86, F_TITLE, 20, pal::goldLight, 0, a * 0.9f);
        // shimmer sweep across the logo
        float sweep = std::fmod(t_ * 0.6f, 1.6f) - 0.3f;
        gfx::glowEllipse(cx - 330 + sweep * 660, cy, 40, 50, pal::white, 0.25f * a);
        float p = (float)step_ / art::buildSteps();
        float bw = 260, bx = cx - bw / 2, by = 520;
        gfx::roundRect(bx, by, bw, 3, 1.5f, Color(255, 255, 255, 30));
        gfx::roundRect(bx, by, bw * p, 3, 1.5f, pal::gold.alpha(0.9f));
        gfx::text(p < 1 ? "Раскладываем колоды и фишки…" : "Добро пожаловать", cx, by + 24, F_SANS, 16, pal::muted, 0, a);
        gfx::text("Только виртуальные фишки. Игра не связана с реальными деньгами.", cx, 680, F_SANS, 14, pal::dim, 0, a);
    }

private:
    float t_ = 0;
    int step_ = 0;
    bool chimed_ = false, left_ = false;
};

} // namespace

std::unique_ptr<Scene> makeBootScene() { return std::make_unique<BootScene>(); }
