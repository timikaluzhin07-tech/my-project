// "Wrath of Zeus" — 6x5 pay-anywhere slot with tumbles, multiplier orbs and free spins.
#pragma once

#include "../core/common.h"

namespace slot {

constexpr int COLS = 6, ROWS = 5;
constexpr int MIN_COUNT = 8;
constexpr int FS_AWARD = 15, FS_RETRIGGER = 5;
constexpr int BUY_COST = 145;      // times the bet (keeps the bonus buy near 96% RTP)
constexpr i64 MAX_WIN_X = 5000;    // win cap per spin / per bonus round, times the bet

enum Sym : int8_t { BLUE, GREEN, YELLOW, PURPLE, RED, CHALICE, RING, HOURGLASS, CROWN, SCATTER, ORB, SYM_COUNT };
constexpr int PAYING = 9; // BLUE..CROWN

struct Cell {
    Sym s = BLUE;
    int16_t mult = 0; // orb value
};

using Grid = Cell[COLS][ROWS]; // [col][row], row 0 at the top

// Pay in hundredths of the bet for 8-9, 10-11, 12+ symbols.
const int (&payTable())[PAYING][3];
const char* symbolName(Sym s);

struct SymbolWin {
    Sym sym;
    int count;
    i64 amount;
};

struct Step {
    Cell before[COLS][ROWS];
    bool hit[COLS][ROWS] = {};
    std::vector<SymbolWin> wins;
    i64 win = 0;                     // this tumble's win (before orb multipliers)
    Cell after[COLS][ROWS];
    int fall[COLS][ROWS] = {};       // rows each surviving cell in `after` dropped
    int fresh[COLS] = {};            // new cells entering from the top per column
};

struct Spin {
    Cell initial[COLS][ROWS];
    std::vector<Step> steps;
    i64 tumbleWin = 0;   // sum of tumble wins
    int orbSum = 0;      // orbs on the final grid
    i64 baseWin = 0;     // tumble win after multipliers (excl. scatter pay)
    int scatters = 0;
    i64 scatterWin = 0;
    int freeSpins = 0;   // spins awarded by this spin
    i64 total = 0;
    int fsMultBefore = 0, fsMultAfter = 0; // running free-spin multiplier
};

struct Engine {
    bool freeGame = false;
    int fsMult = 0;       // accumulated multiplier during free spins
    // Plays one spin for `bet`. In free games the running multiplier applies.
    Spin play(i64 bet, Rng& r, bool forceTrigger = false);
};

int randomOrb(Rng& r, bool freeGame);

} // namespace slot
