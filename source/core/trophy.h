// Trophies (per-player achievements) and the casino-wide progressive jackpot.
#pragma once

#include "common.h"

struct Tex;

namespace trophy {

enum Id {
    FIRST_GAME,
    BLACKJACK,
    FIVE_CARDS,
    DOUBLE_WIN,
    SPLIT_WIN,
    DEALER_BUST,
    BLUFF,
    ALLIN_WIN,
    QUADS,
    STRAIGHT_FLUSH,
    ZERO,
    BULLSEYE,
    ZEUS_BONUS,
    EPIC_WIN,
    JACKPOT,
    HIGH_ROLLER,
    MILLIONAIRE,
    BROKE,
    COMPANY,
    SECRET,
    COUNT
};

struct Info {
    const char* title;
    const char* desc;
};
const Info& info(Id id);

bool has(int profile, Id id);
// First time only: marks it, saves, plays a fanfare and shows the trophy popup.
void unlock(int profile, Id id);
// Balance milestones (high roller, millionaire, broke).
void checkBalance(int profile);
int count(int profile);

// Gold medal sprite (built on first use).
const Tex& medal();

// Popup queue, drawn above everything by the main loop.
void update(float dt);
void render();
void shutdown();

} // namespace trophy

namespace jackpot {

constexpr i64 SEED = 50000;
i64 value();
// A slice of every bet feeds the pool.
void feed(i64 bet);
// Pays the whole pool to a player and reseeds it. Returns the amount.
i64 take(int profile);
// Test harness: the next slot spin hits the jackpot.
void forceNext();
bool consumeForced();

} // namespace jackpot
