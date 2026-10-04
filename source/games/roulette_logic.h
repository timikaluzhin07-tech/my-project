// European single-zero roulette: wheel order, colours and every bet on the layout.
#pragma once

#include "../core/common.h"

namespace roulette {

constexpr int POCKETS = 37;
// Clockwise wheel order starting at zero.
inline const int* wheelOrder() {
    static const int order[POCKETS] = {0,  32, 15, 19, 4,  21, 2,  25, 17, 34, 6,  27, 13, 36, 11, 30, 8,  23, 10,
                                       5,  24, 16, 33, 1,  20, 14, 31, 9,  22, 18, 29, 7,  28, 12, 35, 3,  26};
    return order;
}
inline int pocketIndex(int number) {
    const int* o = wheelOrder();
    for (int i = 0; i < POCKETS; i++)
        if (o[i] == number) return i;
    return 0;
}
inline bool isRed(int n) {
    static const int reds[] = {1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36};
    return std::find(std::begin(reds), std::end(reds), n) != std::end(reds);
}
inline const char* colorName(int n) { return n == 0 ? "ЗЕРО" : (isRed(n) ? "КРАСНОЕ" : "ЧЁРНОЕ"); }

enum Kind { STRAIGHT, SPLIT, STREET, CORNER, SIXLINE, TRIO, BASKET, COLUMN, DOZEN, LOW, HIGH, EVEN, ODD, RED, BLACK };

struct Spot {
    Kind kind;
    std::vector<int> numbers;
    float x, y;    // layout position (logical)
    int payout;    // to 1
    std::string label;
};

inline int payoutFor(int count) { return 36 / count - 1; }

// Grid geometry shared by the layout art and the bet spots.
struct Grid {
    float x0 = 530, y0 = 136; // top-left of number 3
    float cw = 56, ch = 78;   // cell size
    float zeroW = 58;
    float dozenH = 54, evenH = 54, colW = 58;
    float cellX(int col) const { return x0 + col * cw; }
    float rowY(int row) const { return y0 + row * ch; }
    float bottom() const { return y0 + 3 * ch; }
    float right() const { return x0 + 12 * cw; }
};

inline int numberAt(int col, int row) { return col * 3 + (3 - row); }

inline std::vector<Spot> buildSpots(const Grid& g) {
    std::vector<Spot> s;
    auto add = [&](Kind k, std::vector<int> nums, float x, float y, std::string label) {
        Spot sp{k, nums, x, y, payoutFor((int)nums.size()), label};
        if (k >= COLUMN) sp.payout = (k == COLUMN || k == DOZEN) ? 2 : 1;
        s.push_back(sp);
    };
    auto join = [](const std::vector<int>& v, const char* sep) {
        std::string out;
        for (size_t i = 0; i < v.size(); i++) out += (i ? sep : "") + std::to_string(v[i]);
        return out;
    };
    // zero
    add(STRAIGHT, {0}, g.x0 - g.zeroW / 2, g.y0 + 1.5f * g.ch, "Зеро");
    for (int c = 0; c < 12; c++)
        for (int r = 0; r < 3; r++) {
            int n = numberAt(c, r);
            float cx = g.cellX(c) + g.cw / 2, cy = g.rowY(r) + g.ch / 2;
            add(STRAIGHT, {n}, cx, cy, "Номер " + std::to_string(n));
            if (c < 11) { // split to the right
                std::vector<int> v = {n, numberAt(c + 1, r)};
                add(SPLIT, v, g.cellX(c + 1), cy, "Сплит " + join(v, "–"));
            }
            if (r < 2) { // split downwards
                std::vector<int> v = {numberAt(c, r + 1), n};
                add(SPLIT, v, cx, g.rowY(r + 1), "Сплит " + join(v, "–"));
            }
            if (c < 11 && r < 2) {
                std::vector<int> v = {numberAt(c, r + 1), n, numberAt(c + 1, r + 1), numberAt(c + 1, r)};
                std::sort(v.begin(), v.end());
                add(CORNER, v, g.cellX(c + 1), g.rowY(r + 1), "Угол " + join(v, "-"));
            }
        }
    for (int c = 0; c < 12; c++) {
        std::vector<int> v = {c * 3 + 1, c * 3 + 2, c * 3 + 3};
        add(STREET, v, g.cellX(c) + g.cw / 2, g.bottom(), "Стрит " + join(v, "-"));
        if (c < 11) {
            std::vector<int> v6;
            for (int k = 1; k <= 6; k++) v6.push_back(c * 3 + k);
            add(SIXLINE, v6, g.cellX(c + 1), g.bottom(), "Сикслайн " + std::to_string(c * 3 + 1) + "–" + std::to_string(c * 3 + 6));
        }
    }
    // zero combinations along the zero border
    add(SPLIT, {0, 3}, g.x0, g.rowY(0) + g.ch / 2, "Сплит 0–3");
    add(SPLIT, {0, 2}, g.x0, g.rowY(1) + g.ch / 2, "Сплит 0–2");
    add(SPLIT, {0, 1}, g.x0, g.rowY(2) + g.ch / 2, "Сплит 0–1");
    add(TRIO, {0, 2, 3}, g.x0, g.rowY(1), "Трио 0-2-3");
    add(TRIO, {0, 1, 2}, g.x0, g.rowY(2), "Трио 0-1-2");
    add(BASKET, {0, 1, 2, 3}, g.x0, g.bottom(), "Первые четыре 0-1-2-3");
    // columns (2 to 1)
    for (int r = 0; r < 3; r++) {
        std::vector<int> v;
        for (int c = 0; c < 12; c++) v.push_back(numberAt(c, r));
        static const char* names[] = {"Третья колонна", "Вторая колонна", "Первая колонна"};
        add(COLUMN, v, g.right() + g.colW / 2, g.rowY(r) + g.ch / 2, names[r]);
    }
    // dozens
    for (int d = 0; d < 3; d++) {
        std::vector<int> v;
        for (int n = d * 12 + 1; n <= d * 12 + 12; n++) v.push_back(n);
        static const char* names[] = {"Первая дюжина 1–12", "Вторая дюжина 13–24", "Третья дюжина 25–36"};
        add(DOZEN, v, g.x0 + (d * 4 + 2) * g.cw, g.bottom() + g.dozenH / 2, names[d]);
    }
    // even-money row
    float ey = g.bottom() + g.dozenH + g.evenH / 2;
    auto range = [](int a, int b) { std::vector<int> v; for (int n = a; n <= b; n++) v.push_back(n); return v; };
    std::vector<int> ev, od, rd, bk;
    for (int n = 1; n <= 36; n++) {
        (n % 2 ? od : ev).push_back(n);
        (isRed(n) ? rd : bk).push_back(n);
    }
    add(LOW, range(1, 18), g.x0 + 1 * g.cw, ey, "Малые 1–18");
    add(EVEN, ev, g.x0 + 3 * g.cw, ey, "Чётное");
    add(RED, rd, g.x0 + 5 * g.cw, ey, "Красное");
    add(BLACK, bk, g.x0 + 7 * g.cw, ey, "Чёрное");
    add(ODD, od, g.x0 + 9 * g.cw, ey, "Нечётное");
    add(HIGH, range(19, 36), g.x0 + 11 * g.cw, ey, "Большие 19–36");
    return s;
}

// Total returned (stake included) for `amount` on `spot` when `result` comes up.
inline i64 settle(const Spot& spot, i64 amount, int result) {
    bool hit = std::find(spot.numbers.begin(), spot.numbers.end(), result) != spot.numbers.end();
    return hit ? amount * (spot.payout + 1) : 0;
}

// Picks the spot best matching a d-pad direction from the current one.
inline int navigate(const std::vector<Spot>& spots, int from, float dx, float dy) {
    const Spot& a = spots[from];
    int best = -1;
    float bestScore = 1e9f;
    for (int i = 0; i < (int)spots.size(); i++) {
        if (i == from) continue;
        float vx = spots[i].x - a.x, vy = spots[i].y - a.y;
        float along = vx * dx + vy * dy;
        if (along <= 1.f) continue;
        float perp = std::fabs(vx * dy - vy * dx);
        if (perp > along * 1.8f) continue;
        float score = along + perp * 2.6f;
        if (score < bestScore) { bestScore = score; best = i; }
    }
    return best < 0 ? from : best;
}

} // namespace roulette
