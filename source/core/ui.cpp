#include "ui.h"

#include <cstring>

#include "../art/shade.h"
#include "../art/svg.h"
#include "app.h"
#include "audio.h"
#include "fx.h"
#include "save.h"

namespace ui {

namespace {

struct Skin {
    Tex panel, plain, plate, plateHi;
    bool built = false;
};
Skin g_skin;

Image skinImage(float S, float r, Color body, float rimW, Color rim, const shade::Material& rimMat, bool corners) {
    std::vector<shade::Layer> L;
    shade::Layer b;
    b.svg = svg::open(S, S) + "<defs>" +
            svg::linear("bd", 0, 0, 0, 1, {{0, body.scaled(1.45f)}, {0.5f, body}, {1, body.scaled(0.7f)}}) + "</defs>" +
            svg::rect(1, 1, S - 2, S - 2, r, svg::url("bd")) + svg::close();
    b.mat = shade::mat::lacquer();
    b.mat.exposure = 0.9f;
    b.bevel = 3.5f;
    b.depth = 0.4f;
    b.grain = 0.025f;
    L.push_back(b);
    shade::Layer rimL;
    float in = rimW / 2 + 1.2f;
    rimL.svg = svg::open(S, S) + svg::rect(in, in, S - in * 2, S - in * 2, r - in + 1, "none", svg::stroke(rim.css(), rimW)) + svg::close();
    rimL.mat = rimMat;
    rimL.bevel = rimW * 0.55f;
    rimL.depth = 1.f;
    L.push_back(rimL);
    if (corners) {
        shade::Layer c;
        std::string d = svg::open(S, S);
        float o = 9;
        for (auto p : std::vector<std::pair<float, float>>{{o, o}, {S - o, o}, {o, S - o}, {S - o, S - o}})
            d += svg::path("M" + svg::num(p.first) + " " + svg::num(p.second - 3.2f) + " L" + svg::num(p.first + 3.2f) + " " +
                               svg::num(p.second) + " L" + svg::num(p.first) + " " + svg::num(p.second + 3.2f) + " L" +
                               svg::num(p.first - 3.2f) + " " + svg::num(p.second) + " Z",
                           rim.css());
        d += svg::rect(in + 4, in + 4, S - (in + 4) * 2, S - (in + 4) * 2, r - in - 3, "none", svg::stroke(rim.css(), 0.7f, 0.8f));
        c.svg = d + svg::close();
        c.mat = rimMat;
        c.bevel = 0.8f;
        L.push_back(c);
    }
    return shade::relief(S, S, L);
}

void buildSkin() {
    if (g_skin.built) return;
    g_skin.built = true;
    Color gold = Color::hex(0xe2b450);
    shade::Material g = shade::mat::gold();
    shade::Material steel = shade::mat::silver();
    steel.exposure = 0.55f;
    g_skin.panel = gfx::upload(skinImage(72, 11, Color::hex(0x16121a), 2.4f, gold, g, true));
    g_skin.plain = gfx::upload(skinImage(72, 11, Color::hex(0x141218), 1.6f, Color::hex(0x8a8790), steel, false));
    g_skin.plate = gfx::upload(skinImage(48, 9, Color::hex(0x121015), 1.5f, Color::hex(0x6f6a62), steel, false));
    g_skin.plateHi = gfx::upload(skinImage(48, 9, Color::hex(0x1c1610), 2.f, gold, g, false));
}

} // namespace

void releaseSkin() {
    g_skin = Skin();
}

void panel(float x, float y, float w, float h, float alpha, bool gold) {
    buildSkin();
    gfx::roundRect(x + 3, y + 7, w, h, 12, Color(0, 0, 0, 110).alpha(alpha));
    gfx::nine(gold ? g_skin.panel : g_skin.plain, x, y, w, h, 20, alpha);
    // glassy sheen on the upper half
    gfx::rectGrad(x + 10, y + 5, w - 20, std::min(h * 0.45f, 54.f), Color(255, 255, 255, 16).alpha(alpha), Color(255, 255, 255, 0));
}

void plate(float x, float y, float w, float h, bool highlight, float alpha) {
    buildSkin();
    gfx::roundRect(x + 2, y + 4, w, h, 9, Color(0, 0, 0, 100).alpha(alpha));
    gfx::nine(highlight ? g_skin.plateHi : g_skin.plate, x, y, w, h, 14, alpha);
    gfx::rectGrad(x + 8, y + 3, w - 16, h * 0.42f, Color(255, 255, 255, 14).alpha(alpha), Color(255, 255, 255, 0));
}

void rule(float cx, float y, float w, float alpha) {
    Color c = pal::gold.alpha(0.8f * alpha);
    gfx::rectGradH(cx - w / 2, y - 0.6f, w / 2 - 8, 1.2f, c.withA(0), c);
    gfx::rectGradH(cx + 8, y - 0.6f, w / 2 - 8, 1.2f, c, c.withA(0));
    SDL_FPoint t = {cx, y - 4}, r = {cx + 4, y}, b = {cx, y + 4}, l = {cx - 4, y};
    gfx::quad(t, r, b, l, c, c, c, c);
}

void buttonGlyph(u32 btn, float cx, float cy, float size, float alpha) {
    const char* label = "";
    bool pill = false, cross = false;
    switch (btn) {
        case BTN_A: label = "A"; break;
        case BTN_B: label = "B"; break;
        case BTN_X: label = "X"; break;
        case BTN_Y: label = "Y"; break;
        case BTN_L: label = "L"; pill = true; break;
        case BTN_R: label = "R"; pill = true; break;
        case BTN_ZL: label = "ZL"; pill = true; break;
        case BTN_ZR: label = "ZR"; pill = true; break;
        case BTN_PLUS: label = "+"; break;
        case BTN_MINUS: label = "\xE2\x88\x92"; break;
        default: cross = true; break;
    }
    Color bg = Color(232, 226, 214).alpha(alpha), fg = Color(18, 16, 20).alpha(alpha);
    float r = size * 0.5f;
    if (cross) {
        float a = size * 0.36f, t = size * 0.3f;
        gfx::roundRect(cx - t / 2, cy - a - t / 2, t, a * 2 + t, 2, bg);
        gfx::roundRect(cx - a - t / 2, cy - t / 2, a * 2 + t, t, 2, bg);
        Color hi = pal::gold.alpha(alpha);
        if (btn == BTN_UP || btn == (BTN_UP | BTN_DOWN)) gfx::rect(cx - t / 2 + 1, cy - a - t / 2 + 1, t - 2, a - 1, hi);
        if (btn == BTN_DOWN || btn == (BTN_UP | BTN_DOWN)) gfx::rect(cx - t / 2 + 1, cy + t / 2, t - 2, a - 1, hi);
        if (btn == BTN_LEFT || btn == (BTN_LEFT | BTN_RIGHT)) gfx::rect(cx - a - t / 2 + 1, cy - t / 2 + 1, a - 1, t - 2, hi);
        if (btn == BTN_RIGHT || btn == (BTN_LEFT | BTN_RIGHT)) gfx::rect(cx + t / 2, cy - t / 2 + 1, a - 1, t - 2, hi);
        return;
    }
    if (pill) {
        float w = size * (strlen(label) > 1 ? 1.55f : 1.25f);
        gfx::roundRect(cx - w / 2, cy - r * 0.82f, w, size * 0.82f, r * 0.5f, bg);
        gfx::text(label, cx, cy, F_SANS_BOLD, size * 0.62f, fg, 0);
    } else {
        gfx::circle(cx, cy, r, bg);
        gfx::text(label, cx, cy + (btn == BTN_PLUS ? -0.5f : 0.f), F_SANS_BOLD, size * 0.68f, fg, 0);
    }
}

static float glyphWidth(u32 btn, float size) {
    if (btn == BTN_ZL || btn == BTN_ZR) return size * 1.55f;
    if (btn == BTN_L || btn == BTN_R) return size * 1.25f;
    return size;
}

float hints(const std::vector<std::pair<u32, std::string>>& items, float x, float y, int align, float alpha,
            float size) {
    float gap = 22, total = 0;
    float gs = size * 1.25f;
    std::vector<float> widths;
    for (auto& [b, s] : items) {
        float w = glyphWidth(b, gs) + 7 + gfx::textWidth(s, F_SANS, size);
        widths.push_back(w);
        total += w;
    }
    total += gap * (items.empty() ? 0 : items.size() - 1);
    float cx = align > 0 ? x - total : (align == 0 ? x - total / 2 : x);
    for (size_t i = 0; i < items.size(); i++) {
        float gw = glyphWidth(items[i].first, gs);
        buttonGlyph(items[i].first, cx + gw / 2, y, gs, alpha);
        gfx::text(items[i].second, cx + gw + 7, y, F_SANS, size, pal::cream, -1, alpha);
        cx += widths[i] + gap;
    }
    return total;
}

void chipDot(float cx, float cy, float r, Color c) {
    gfx::circle(cx, cy + 1, r, Color(0, 0, 0, 120));
    gfx::circle(cx, cy, r, c);
    for (int i = 0; i < 6; i++) {
        float a = i * PI / 3;
        gfx::line(cx + std::cos(a) * r * 0.62f, cy + std::sin(a) * r * 0.62f, cx + std::cos(a) * r * 0.95f,
                  cy + std::sin(a) * r * 0.95f, r * 0.32f, Color(255, 255, 255, 200));
    }
    gfx::ring(cx, cy, r * 0.55f, 1.f, Color(255, 255, 255, 160));
}

void playerTag(int profile, float x, float y, float w, bool highlight, float alpha, const std::string& sub,
               bool showBalance) {
    const Profile& p = save::player(profile);
    float h = 48;
    if (highlight) gfx::glowEllipse(x + w / 2, y + h / 2, w * 0.7f, h * 1.2f, pal::gold, 0.14f * alpha);
    plate(x, y, w, h, highlight, alpha);
    chipDot(x + 20, y + h / 2, 10, playerColor(p.color).alpha(alpha));
    float tx = x + 38;
    gfx::text(p.name, tx, y + 16, F_SANS_BOLD, 17, pal::ivory, -1, alpha);
    std::string line2 = sub.empty() ? (showBalance ? fmtMoney(p.balance) : "") : sub;
    if (!line2.empty()) gfx::text(line2, tx, y + 34, F_NUM, 14, highlight ? pal::goldLight : pal::muted, -1, alpha);
}

void spinner(float cx, float cy, float r, float t) {
    for (int i = 0; i < 12; i++) {
        float a = i * PI / 6 + t * 4;
        float k = std::fmod(i / 12.f + t * 0.8f, 1.f);
        gfx::circle(cx + std::cos(a) * r, cy + std::sin(a) * r, 3, pal::gold.alpha(0.2f + 0.8f * k));
    }
}

// ---------------------------------------------------------------------------
bool Menu::update(int pad, float dt) {
    selAnim = approach(selAnim, (float)sel, 18, dt);
    if (items.empty()) return false;
    int n = (int)items.size();
    if (input::repeat(pad, BTN_UP)) {
        do sel = (sel + n - 1) % n; while (!items[sel].enabled);
        audio::play(audio::SFX_NAV);
        return true;
    }
    if (input::repeat(pad, BTN_DOWN)) {
        do sel = (sel + 1) % n; while (!items[sel].enabled);
        audio::play(audio::SFX_NAV);
        return true;
    }
    MenuItem& it = items[sel];
    if (it.change) {
        if (input::repeat(pad, BTN_LEFT)) { it.change(-1); audio::play(audio::SFX_NAV, 0.8f, -0.3f); return true; }
        if (input::repeat(pad, BTN_RIGHT)) { it.change(1); audio::play(audio::SFX_NAV, 0.8f, 0.3f); return true; }
    }
    if (input::pressed(pad, BTN_A)) {
        if (it.activate) { audio::play(audio::SFX_SELECT); it.activate(); return true; }
        if (it.change) { it.change(1); audio::play(audio::SFX_NAV); return true; }
    }
    return false;
}

void Menu::render(float x, float y, float w, float rowH, float alpha) {
    float hy = y + selAnim * rowH;
    gfx::roundRect(x, hy + 3, w, rowH - 6, 6, Color(212, 175, 55, 34).alpha(alpha));
    gfx::roundRectOutline(x, hy + 3, w, rowH - 6, 6, 1.2f, pal::gold.alpha(0.75f * alpha));
    for (size_t i = 0; i < items.size(); i++) {
        const MenuItem& it = items[i];
        float cy = y + i * rowH + rowH / 2;
        bool s = (int)i == sel;
        Color c = !it.enabled ? pal::dim : (s ? pal::ivory : pal::cream.scaled(0.85f));
        gfx::text(it.label, x + 18, cy, F_SANS_BOLD, 20, c, -1, alpha);
        if (it.value) {
            std::string v = it.value();
            float vx = x + w - 18;
            if (it.change && s) {
                gfx::text("\xE2\x80\xB9", vx - gfx::textWidth(v, F_NUM, 19) - 16, cy, F_SANS, 18, pal::gold, 0, alpha);
                gfx::text("\xE2\x80\xBA", vx + 2, cy, F_SANS, 18, pal::gold, 0, alpha);
                vx -= 10;
            }
            gfx::text(v, vx, cy, F_NUM, 19, s ? pal::goldLight : pal::muted, 1, alpha);
        }
    }
}

// ---------------------------------------------------------------------------
void Dialog::show(const std::string& t, const std::string& b, std::vector<std::string> opts,
                  std::function<void(int)> cb, int fromPad, int defaultSel) {
    title = t;
    body = b;
    options = std::move(opts);
    onPick = std::move(cb);
    sel = std::clamp(defaultSel, 0, (int)options.size() - 1);
    open = true;
    anim = 0;
    pad = fromPad;
    input::consume();
}

bool Dialog::update(float dt) {
    if (!open) return false;
    anim = std::min(1.f, anim + dt * 6);
    int n = (int)options.size();
    if (input::repeat(pad, BTN_LEFT | BTN_UP)) { sel = (sel + n - 1) % n; audio::play(audio::SFX_NAV); }
    if (input::repeat(pad, BTN_RIGHT | BTN_DOWN)) { sel = (sel + 1) % n; audio::play(audio::SFX_NAV); }
    if (input::pressed(pad, BTN_A)) {
        open = false;
        audio::play(audio::SFX_SELECT);
        input::consume();
        if (onPick) onPick(sel);
    } else if (input::pressed(pad, BTN_B)) {
        open = false;
        audio::play(audio::SFX_BACK);
        input::consume();
        if (onPick) onPick(-1);
    }
    return true;
}

void Dialog::render() {
    if (!open) return;
    float a = ease::outCubic(anim);
    gfx::dim(0.55f * a);
    float w = 560, h = 220;
    float bodyH = 0;
    if (!body.empty()) bodyH = 26 * (1 + (float)std::count(body.begin(), body.end(), '\n'));
    h = 150 + bodyH;
    float x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2 + (1 - a) * 20;
    panel(x, y, w, h, a);
    gfx::textGold(title, SCREEN_W / 2.f, y + 40, F_TITLE, 26, 0, a);
    rule(SCREEN_W / 2.f, y + 64, 300, a);
    if (!body.empty()) gfx::textWrapped(body, x + 40, y + 78, w - 80, F_SANS, 19, pal::cream, 1.25f, a);
    float bw = 160, gap = 20;
    float total = options.size() * bw + (options.size() - 1) * gap;
    float bx = SCREEN_W / 2.f - total / 2, by = y + h - 58;
    for (size_t i = 0; i < options.size(); i++) {
        bool s = (int)i == sel;
        gfx::roundRect(bx, by, bw, 40, 6, s ? Color(212, 175, 55, 220).alpha(a) : Color(255, 255, 255, 20).alpha(a));
        if (!s) gfx::roundRectOutline(bx, by, bw, 40, 6, 1, pal::gold.alpha(0.5f * a));
        gfx::text(options[i], bx + bw / 2, by + 20, F_SANS_BOLD, 19, s ? pal::ink : pal::cream, 0, a);
        bx += bw + gap;
    }
}

// ---------------------------------------------------------------------------
void PauseMenu::show(std::vector<MenuItem> items) {
    menu.items = std::move(items);
    menu.sel = 0;
    menu.selAnim = 0;
    open = true;
    rules = false;
    anim = 0;
    input::consume();
}

bool PauseMenu::update(float dt) {
    if (!open) return false;
    anim = std::min(1.f, anim + dt * 6);
    if (rules) {
        if (input::pressed(ANY_PAD, BTN_B | BTN_A | BTN_PLUS)) { rules = false; audio::play(audio::SFX_BACK); }
        return true;
    }
    if (input::pressed(ANY_PAD, BTN_B | BTN_PLUS)) {
        close();
        audio::play(audio::SFX_BACK);
        input::consume();
        return true;
    }
    menu.update(ANY_PAD, dt);
    return true;
}

void PauseMenu::render() {
    if (!open) return;
    float a = ease::outCubic(anim);
    gfx::dim(0.6f * a);
    if (rules) {
        float w = 900, h = 560, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
        panel(x, y, w, h, a);
        gfx::textGold(rulesTitle, SCREEN_W / 2.f, y + 42, F_TITLE, 30, 0, a);
        rule(SCREEN_W / 2.f, y + 72, 360, a);
        gfx::textWrapped(rulesText, x + 44, y + 92, w - 88, F_SANS, 18, pal::cream, 1.25f, a);
        hints({{BTN_B, "Назад"}}, x + w - 24, y + h - 26, 1, a);
        return;
    }
    float w = 460, rowH = 50, h = menu.items.size() * rowH + 110;
    float x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
    panel(x, y, w, h, a);
    gfx::textGold(title, SCREEN_W / 2.f, y + 40, F_TITLE, 30, 0, a);
    rule(SCREEN_W / 2.f, y + 68, 260, a);
    menu.render(x + 24, y + 86, w - 48, rowH, a);
}

// ---------------------------------------------------------------------------
struct Toast { std::string text; Color c; float t, dur; };
static std::vector<Toast> g_toasts;

void toast(const std::string& text, Color c, float seconds) {
    g_toasts.push_back({text, c, 0, seconds});
    if (g_toasts.size() > 4) g_toasts.erase(g_toasts.begin());
}

void updateToasts(float dt) {
    for (auto& t : g_toasts) t.t += dt;
    g_toasts.erase(std::remove_if(g_toasts.begin(), g_toasts.end(), [](const Toast& t) { return t.t >= t.dur; }),
                   g_toasts.end());
}

void renderToasts() {
    float y = 92;
    for (auto& t : g_toasts) {
        float a = std::min(1.f, t.t * 5) * std::min(1.f, (t.dur - t.t) * 3);
        float w = gfx::textWidth(t.text, F_SANS_BOLD, 20) + 48;
        float x = SCREEN_W / 2.f - w / 2;
        float yy = y - (1 - std::min(1.f, t.t * 5)) * 12;
        gfx::roundRect(x, yy, w, 40, 20, Color(12, 10, 14, 230).alpha(a));
        gfx::roundRectOutline(x, yy, w, 40, 20, 1.2f, t.c.alpha(0.8f * a));
        gfx::text(t.text, SCREEN_W / 2.f, yy + 20, F_SANS_BOLD, 20, t.c, 0, a);
        y += 48;
    }
}

// ---------------------------------------------------------------------------
void Banner::show(const std::string& s, const std::string& subText, float seconds, bool goldStyle, Color c) {
    text = s;
    sub = subText;
    t = 0;
    dur = seconds;
    gold = goldStyle;
    color = c;
}

void Banner::render(float cx, float cy, float size) {
    if (!active()) return;
    float in = ease::outBack(std::min(1.f, t / 0.35f), 2.2f);
    float out = std::min(1.f, (dur - t) / 0.35f);
    float a = std::min(1.f, t / 0.15f) * out;
    float sc = 0.6f + 0.4f * in;
    float w = std::max(gfx::textWidth(text, F_TITLE, size), gfx::textWidth(sub, F_SANS_BOLD, size * 0.38f)) + 120;
    gfx::rectGradH(cx - w / 2, cy - size * 0.75f, w / 2, size * (sub.empty() ? 1.5f : 2.0f), Color(0, 0, 0, 0),
                   Color(0, 0, 0, 170).alpha(a));
    gfx::rectGradH(cx, cy - size * 0.75f, w / 2, size * (sub.empty() ? 1.5f : 2.0f), Color(0, 0, 0, 170).alpha(a),
                   Color(0, 0, 0, 0));
    gfx::glowEllipse(cx, cy, w * 0.45f * sc, size * 1.1f, color, 0.25f * a);
    float s = size * sc;
    if (gold) gfx::textGold(text, cx, cy, F_TITLE, s, 0, a);
    else gfx::textShadow(text, cx, cy, F_TITLE, s, color, 0, a);
    if (gold) {
        float tw = gfx::textWidth(text, F_TITLE, s);
        fx::shine(cx - tw / 2 - 10, cy - s * 0.5f, tw + 20, s, clamp01((t - 0.3f) / 0.7f), 0.4f * a);
    }
    if (!sub.empty()) gfx::textShadow(sub, cx, cy + size * 0.78f, F_SANS_BOLD, size * 0.38f, pal::ivory, 0, a);
}

} // namespace ui
