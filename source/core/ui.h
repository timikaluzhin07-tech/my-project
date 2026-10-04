// Shared UI pieces: panels, button prompts, menus, dialogs, toasts.
#pragma once

#include "gfx.h"
#include "input.h"

namespace ui {

void panel(float x, float y, float w, float h, float alpha = 1.f, bool gold = true);
// Thin double gold rule with diamond ends — section separators.
void rule(float cx, float y, float w, float alpha = 1.f);
void buttonGlyph(u32 btn, float cx, float cy, float size = 22.f, float alpha = 1.f);
// Row of "[glyph] label" prompts. align 1 = right edge at x, -1 = left edge, 0 centred.
float hints(const std::vector<std::pair<u32, std::string>>& items, float x, float y, int align = 1,
            float alpha = 1.f, float size = 17.f);
void playerTag(int profile, float x, float y, float w, bool highlight, float alpha = 1.f,
               const std::string& sub = "", bool showBalance = true);
void chipDot(float cx, float cy, float r, Color c);
void spinner(float cx, float cy, float r, float t);

struct MenuItem {
    std::string label;
    std::function<std::string()> value;
    std::function<void(int)> change;
    std::function<void()> activate;
    bool enabled = true;
    std::string note;
};

class Menu {
public:
    std::vector<MenuItem> items;
    int sel = 0;
    float selAnim = 0;
    // Returns true if the menu handled a button this frame.
    bool update(int pad, float dt);
    void render(float x, float y, float w, float rowH = 46.f, float alpha = 1.f);
};

class Dialog {
public:
    bool open = false;
    std::string title, body;
    std::vector<std::string> options;
    int sel = 0;
    float anim = 0;
    int pad = ANY_PAD;
    std::function<void(int)> onPick; // -1 = cancelled

    void show(const std::string& t, const std::string& b, std::vector<std::string> opts,
              std::function<void(int)> cb, int fromPad = ANY_PAD, int defaultSel = 0);
    // Returns true while the dialog owns the input.
    bool update(float dt);
    void render();
};

// In-game pause overlay with a rules page.
class PauseMenu {
public:
    bool open = false;
    bool rules = false;
    std::string title = "ПАУЗА";
    std::string rulesTitle, rulesText;
    Menu menu;
    float anim = 0;
    void show(std::vector<MenuItem> items);
    void close() { open = false; rules = false; }
    // Returns true while the overlay owns the input.
    bool update(float dt);
    void render();
};

void toast(const std::string& text, Color c = pal::goldLight, float seconds = 2.2f);
void updateToasts(float dt);
void renderToasts();

// Large centred announcement ("БЛЭКДЖЕК!", "ПОБЕДА") that scales in and fades.
struct Banner {
    std::string text, sub;
    Color color = pal::goldLight;
    float t = 0, dur = 0;
    bool gold = true;
    void show(const std::string& s, const std::string& subText = "", float seconds = 1.8f, bool goldStyle = true,
              Color c = pal::goldLight);
    void update(float dt) { if (t < dur) t += dt; }
    bool active() const { return t < dur; }
    void render(float cx, float cy, float size = 64.f);
};

} // namespace ui
