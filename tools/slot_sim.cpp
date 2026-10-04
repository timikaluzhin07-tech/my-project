// Monte-Carlo statistics for the Zeus slot: RTP, hit rate, bonus frequency.
//   make -f Makefile.pc slotsim && ./build-pc/slot_sim 2000000
#include <cstdio>
#include <cstdlib>

#include "../source/games/slot_logic.h"

using namespace slot;

static i64 playBonus(Rng& r, i64 bet, int spins, int* totalSpins) {
    Engine fs;
    fs.freeGame = true;
    i64 win = 0;
    int left = spins, played = 0;
    while (left > 0 && played < 300) {
        Spin s = fs.play(bet, r);
        win += s.total;
        left += s.freeSpins - 1;
        played++;
        if (win >= bet * MAX_WIN_X) { win = bet * MAX_WIN_X; break; }
    }
    if (totalSpins) *totalSpins += played;
    return win;
}

int main(int argc, char** argv) {
    long n = argc > 1 ? atol(argv[1]) : 1000000;
    Rng r(2024);
    const i64 bet = 100;
    double paid = 0, won = 0, bonusWon = 0, maxWin = 0;
    long hits = 0, bonuses = 0, tumbles = 0;
    int fsSpins = 0;
    Engine base;
    for (long i = 0; i < n; i++) {
        paid += bet;
        Spin s = base.play(bet, r);
        i64 w = s.total;
        tumbles += (long)s.steps.size();
        if (s.freeSpins > 0) {
            bonuses++;
            i64 b = playBonus(r, bet, s.freeSpins, &fsSpins);
            bonusWon += b;
            w += b;
        }
        if (w > 0) hits++;
        won += w;
        maxWin = std::max(maxWin, (double)w);
    }
    // bonus buy
    long buys = std::max(1L, n / 200);
    double buyWon = 0;
    for (long i = 0; i < buys; i++) {
        Engine e;
        Spin s = e.play(bet, r, true);
        buyWon += s.scatterWin + playBonus(r, bet, FS_AWARD, nullptr);
    }
    printf("spins %ld\n", n);
    printf("RTP            %.2f%%\n", 100.0 * won / paid);
    printf("  base part    %.2f%%\n", 100.0 * (won - bonusWon) / paid);
    printf("  bonus part   %.2f%%\n", 100.0 * bonusWon / paid);
    printf("hit rate       %.1f%%\n", 100.0 * hits / n);
    printf("bonus every    %.0f spins (avg %.1fx bet, %.1f spins)\n", bonuses ? (double)n / bonuses : 0.0,
           bonuses ? bonusWon / bonuses / bet : 0.0, bonuses ? (double)fsSpins / bonuses : 0.0);
    printf("tumbles/spin   %.2f\n", (double)tumbles / n);
    printf("max win        %.0fx\n", maxWin / bet);
    printf("buy feature    %.2f%% RTP (cost %dx)\n", 100.0 * buyWon / (buys * (double)bet * BUY_COST), BUY_COST);
    return 0;
}
