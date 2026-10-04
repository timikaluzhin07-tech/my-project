#include "trophy.h"

#include "../art/shade.h"
#include "../art/svg.h"
#include "audio.h"
#include "fx.h"
#include "anim.h"
#include "gfx.h"
#include "save.h"

namespace trophy {

namespace {

const Info kInfo[COUNT] = {
    {"Добро пожаловать", "Сыграть первую партию в любом зале"},
    {"Натуральный", "Собрать блэкджек с первых двух карт"},
    {"Пять карт", "Выиграть рукой из пяти и более карт в блэкджеке"},
    {"Удвоение удачи", "Удвоить ставку и выиграть"},
    {"Разделяй и властвуй", "Выиграть обе руки после сплита"},
    {"Дилер, ты чего?", "Увидеть, как у дилера перебор"},
    {"Покерфейс", "Забрать банк, когда все сбросили карты после вашей ставки"},
    {"Ва-банк", "Пойти олл-ин и выиграть вскрытие"},
    {"Каре", "Выиграть банк с каре или сильнее"},
    {"Стрит-флеш", "Выиграть банк со стрит-флешем"},
    {"Зеро!", "Выиграть ставкой на зеро"},
    {"В яблочко", "Угадать номер в рулетке ставкой на число"},
    {"Гнев Зевса", "Запустить бесплатные вращения"},
    {"Олимп", "Выиграть в слоте ×100 ставки и больше"},
    {"Джекпот", "Сорвать прогрессивный джекпот"},
    {"Хайроллер", "Накопить 100 000 фишек"},
    {"Миллионер", "Накопить 1 000 000 фишек"},
    {"Бывает и такое", "Остаться без фишек"},
    {"Компания", "Сыграть за одним столом втроём или больше"},
    {"Секрет", "Найти пасхалку в главном зале"},
};

struct Popup {
    int profile;
    Id id;
    float t = 0;
};
std::vector<Popup> g_queue;
Tex g_medal;

// Gold medal with a star, shaded like the other metal art.
Image buildMedal() {
    const float D = 64, c = D / 2;
    std::vector<shade::Layer> L(4);
    std::string ribbon = svg::path("M18 2 L30 2 L36 26 L24 26 Z", "#8a1020") + svg::path("M46 2 L34 2 L28 26 L40 26 Z", "#1a3a8a");
    L[0].svg = svg::open(D, D) + ribbon + svg::close();
    L[0].mat = shade::mat::leather();
    L[0].bevel = 1.5f;
    L[1].svg = svg::open(D, D) + svg::circle(c, c + 8, 22, "#e6b850") + svg::close();
    L[1].mat = shade::mat::gold();
    L[1].bevel = 4;
    L[1].shadow = 2;
    L[2].svg = svg::open(D, D) + svg::circle(c, c + 8, 16, "#d9a840") + svg::close();
    L[2].mat = shade::mat::gold();
    L[2].bevel = 1.5f;
    L[2].depth = -0.7f;
    std::string star = "M";
    for (int i = 0; i < 10; i++) {
        float a = -PI / 2 + i * PI / 5, rr = i % 2 ? 5.f : 12.f;
        star += svg::num(c + std::cos(a) * rr) + " " + svg::num(c + 8 + std::sin(a) * rr) + (i < 9 ? " L" : " Z");
    }
    L[3].svg = svg::open(D, D) + svg::path(star, "#f3d070") + svg::close();
    L[3].mat = shade::mat::gold();
    L[3].bevel = 2;
    Image img = shade::relief(D, D, L);
    shade::glints(img, 2, 9, 6);
    return img;
}

} // namespace

const Info& info(Id id) { return kInfo[std::clamp((int)id, 0, COUNT - 1)]; }

bool has(int profile, Id id) {
    if (profile < 0 || profile >= MAX_PLAYERS) return false;
    return (save::player(profile).trophies >> id) & 1u;
}

int count(int profile) {
    int n = 0;
    for (int i = 0; i < COUNT; i++) n += has(profile, (Id)i);
    return n;
}

void unlock(int profile, Id id) {
    if (profile < 0 || profile >= MAX_PLAYERS || has(profile, id)) return;
    save::player(profile).trophies |= 1u << id;
    save::store();
    g_queue.push_back({profile, id});
}

void checkBalance(int profile) {
    if (profile < 0 || profile >= MAX_PLAYERS) return;
    const Profile& p = save::player(profile);
    i64 total = p.balance + p.stake;
    if (total >= 100000) unlock(profile, HIGH_ROLLER);
    if (total >= 1000000) unlock(profile, MILLIONAIRE);
    if (total <= 0) unlock(profile, BROKE);
}

void update(float dt) {
    if (g_queue.empty()) return;
    Popup& p = g_queue.front();
    if (p.t == 0) {
        audio::play(audio::SFX_ACHIEVE, 0.8f);
        fx::burst(SCREEN_W - 200, 60, 24, Color::hex(0xffd36a), 300, 300);
    }
    p.t += dt;
    if (p.t > 3.4f) g_queue.erase(g_queue.begin());
}

const Tex& medal() {
    if (!g_medal) g_medal = gfx::upload(buildMedal());
    return g_medal;
}

void render() {
    if (g_queue.empty()) return;
    medal();
    const Popup& p = g_queue.front();
    float in = ease::outBack(clamp01(p.t / 0.45f), 1.6f), out = clamp01((3.4f - p.t) / 0.35f);
    float w = 380, h = 84;
    float x = SCREEN_W - w - 20 + (1 - in) * (w + 40), y = 20;
    float a = out;
    gfx::glowEllipse(x + w / 2, y + h / 2, w * 0.7f, h, Color::hex(0xffc04a), 0.18f * a);
    gfx::roundRect(x, y, w, h, 14, Color(14, 10, 12, 236).alpha(a));
    gfx::roundRectOutline(x, y, w, h, 14, 1.6f, pal::gold.alpha(a));
    gfx::drawCentered(g_medal, x + 44, y + h / 2 - 4, 1.f, 8 * std::sin(p.t * 3) * (1 - clamp01(p.t / 1.5f)), a);
    const Profile& pr = save::player(p.profile);
    gfx::text("ТРОФЕЙ · " + pr.name, x + 84, y + 20, F_SANS_BOLD, 14, playerColor(pr.color).scaled(1.3f), -1, a);
    gfx::textGold(info(p.id).title, x + 84, y + 44, F_TITLE, 22, -1, a, true);
    gfx::text(info(p.id).desc, x + 84, y + 66, F_SANS, 13, pal::cream, -1, a);
    fx::shine(x, y, w, h, clamp01((p.t - 0.4f) / 0.9f), 0.35f * a);
}

void shutdown() {
    g_medal = Tex();
    g_queue.clear();
}

} // namespace trophy

namespace jackpot {

i64 value() { return std::max(SEED, save::data().jackpot); }

void feed(i64 bet) {
    if (bet <= 0) return;
    save::data().jackpot = value() + std::max<i64>(1, bet / 100);
}

namespace {
bool g_forced = false;
}
void forceNext() { g_forced = true; }
bool consumeForced() {
    bool f = g_forced;
    g_forced = false;
    return f;
}

i64 take(int profile) {
    i64 v = value();
    save::player(profile).balance += v;
    save::data().jackpot = SEED;
    save::store();
    return v;
}

} // namespace jackpot
