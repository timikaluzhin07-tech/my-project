#include "slot_logic.h"

namespace slot {

const int (&payTable())[PAYING][3] {
    static const int t[PAYING][3] = {
        {25, 75, 200},      // sapphire
        {40, 90, 400},      // emerald
        {50, 100, 500},     // topaz
        {80, 120, 800},     // amethyst
        {100, 150, 1000},   // ruby
        {150, 200, 1200},   // chalice
        {200, 500, 1500},   // ring
        {250, 1000, 2500},  // hourglass
        {1000, 2500, 5000}, // crown
    };
    return t;
}

const char* symbolName(Sym s) {
    static const char* n[SYM_COUNT] = {"Сапфир", "Изумруд", "Топаз", "Аметист", "Рубин", "Кубок",
                                       "Перстень", "Песочные часы", "Корона", "Молния Зевса", "Сфера"};
    return n[s];
}

namespace {

// Symbol weights for the base game and the free games (tuned with tools/slot_sim).
const int W_BASE[SYM_COUNT] = {326, 298, 272, 240, 206, 154, 128, 104, 82, 30, 11};
const int W_FREE[SYM_COUNT] = {380, 330, 290, 250, 210, 140, 110, 85, 62, 22, 50};

Cell roll(Rng& r, bool fs) {
    const int* w = fs ? W_FREE : W_BASE;
    int total = 0;
    for (int i = 0; i < SYM_COUNT; i++) total += w[i];
    int x = r.range(0, total - 1);
    for (int i = 0; i < SYM_COUNT; i++) {
        if (x < w[i]) {
            Cell c;
            c.s = (Sym)i;
            if (c.s == ORB) c.mult = (int16_t)randomOrb(r, fs);
            return c;
        }
        x -= w[i];
    }
    return Cell();
}

int countScatters(const Cell g[COLS][ROWS]) {
    int n = 0;
    for (int c = 0; c < COLS; c++)
        for (int r = 0; r < ROWS; r++) n += g[c][r].s == SCATTER;
    return n;
}

} // namespace

int randomOrb(Rng& r, bool fs) {
    static const int vals[] = {2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 50, 100, 250, 500};
    static const int wb[] = {300, 220, 160, 120, 90, 60, 45, 30, 20, 12, 8, 3, 1, 0, 0};
    static const int wf[] = {280, 210, 160, 120, 90, 65, 50, 35, 24, 15, 10, 4, 1, 0, 0};
    const int* w = fs ? wf : wb;
    int total = 0;
    for (int i = 0; i < 15; i++) total += w[i];
    int x = r.range(0, total - 1);
    for (int i = 0; i < 15; i++) {
        if (x < w[i]) {
            // the very top values are a rare upgrade of the biggest bucket
            if (vals[i] == 100 && r.chance(0.12)) return r.chance(0.2) ? 500 : 250;
            return vals[i];
        }
        x -= w[i];
    }
    return 2;
}

Spin Engine::play(i64 bet, Rng& r, bool forceTrigger) {
    Spin sp;
    Cell g[COLS][ROWS];
    for (int c = 0; c < COLS; c++)
        for (int row = 0; row < ROWS; row++) g[c][row] = roll(r, freeGame);
    if (forceTrigger) {
        int want = r.chance(0.12) ? 5 : 4;
        int guard = 0;
        while (countScatters(g) < want && guard++ < 100) {
            int c = r.range(0, COLS - 1), row = r.range(0, ROWS - 1);
            bool colHas = false;
            for (int k = 0; k < ROWS; k++) colHas |= g[c][k].s == SCATTER;
            if (colHas && guard < 60) continue;
            g[c][row] = Cell{SCATTER, 0};
        }
    }
    std::copy(&g[0][0], &g[0][0] + COLS * ROWS, &sp.initial[0][0]);
    const auto& pays = payTable();
    for (int guard = 0; guard < 40; guard++) {
        int counts[PAYING] = {0};
        for (int c = 0; c < COLS; c++)
            for (int row = 0; row < ROWS; row++)
                if (g[c][row].s < PAYING) counts[g[c][row].s]++;
        Step st;
        for (int s = 0; s < PAYING; s++) {
            if (counts[s] < MIN_COUNT) continue;
            int tier = counts[s] >= 12 ? 2 : (counts[s] >= 10 ? 1 : 0);
            i64 amt = bet * pays[s][tier] / 100;
            st.wins.push_back({(Sym)s, counts[s], amt});
            st.win += amt;
        }
        if (st.wins.empty()) break;
        std::copy(&g[0][0], &g[0][0] + COLS * ROWS, &st.before[0][0]);
        for (int c = 0; c < COLS; c++)
            for (int row = 0; row < ROWS; row++)
                for (auto& w : st.wins)
                    if (g[c][row].s == w.sym) st.hit[c][row] = true;
        // gravity: survivors slide down, new symbols drop in from the top
        for (int c = 0; c < COLS; c++) {
            int dst = ROWS - 1;
            for (int row = ROWS - 1; row >= 0; row--) {
                if (st.hit[c][row]) continue;
                st.after[c][dst] = g[c][row];
                st.fall[c][dst] = dst - row;
                dst--;
            }
            st.fresh[c] = dst + 1;
            for (int row = dst; row >= 0; row--) {
                st.after[c][row] = roll(r, freeGame);
                st.fall[c][row] = dst + 1; // drops in from above the grid
            }
        }
        std::copy(&st.after[0][0], &st.after[0][0] + COLS * ROWS, &g[0][0]);
        sp.tumbleWin += st.win;
        sp.steps.push_back(st);
    }
    for (int c = 0; c < COLS; c++)
        for (int row = 0; row < ROWS; row++)
            if (g[c][row].s == ORB) sp.orbSum += g[c][row].mult;
    sp.scatters = countScatters(g);
    sp.fsMultBefore = fsMult;
    if (freeGame) {
        if (sp.tumbleWin > 0 && sp.orbSum > 0) {
            fsMult += sp.orbSum;
            sp.baseWin = sp.tumbleWin * fsMult;
        } else {
            sp.baseWin = sp.tumbleWin;
        }
        if (sp.scatters >= 3) sp.freeSpins = FS_RETRIGGER;
    } else {
        sp.baseWin = (sp.tumbleWin > 0 && sp.orbSum > 0) ? sp.tumbleWin * sp.orbSum : sp.tumbleWin;
        if (sp.scatters >= 4) sp.freeSpins = FS_AWARD;
    }
    sp.fsMultAfter = fsMult;
    if (sp.scatters >= 6) sp.scatterWin = bet * 100;
    else if (sp.scatters == 5) sp.scatterWin = bet * 5;
    else if (sp.scatters == 4) sp.scatterWin = bet * 3;
    sp.total = std::min(sp.baseWin + sp.scatterWin, bet * MAX_WIN_X);
    return sp;
}

} // namespace slot
