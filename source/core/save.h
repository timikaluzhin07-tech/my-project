// Player profiles and settings, persisted to the SD card.
#pragma once

#include "common.h"

constexpr int MAX_PLAYERS = 6;
constexpr i64 START_BALANCE = 10000;

struct Profile {
    std::string name;
    int color = 0;          // index into playerColor()
    i64 balance = START_BALANCE;
    bool active = false;
    i64 stake = 0;          // chips currently sitting at the poker table
    i64 biggestWin = 0;
    i64 totalWon = 0;
    i64 totalLost = 0;
    int refills = 0;        // how many times the cashier topped the player up
};

struct Settings {
    float master = 0.9f;
    float music = 0.55f;
    float sfx = 0.9f;
    int hideCards = 0;      // poker hole cards: 0 auto, 1 always hide, 2 never hide
    int pokerBots = 3;
    int pokerBlinds = 1;    // index into blind levels
    int pokerBuyIn = 1;     // index into buy-in options
    bool fastDeal = false;
    bool slotTurbo = false;
    int slotBet = 2;        // index into slot bet levels
    bool slotRotate = true; // pass the slot machine to the next player after each spin
};

struct SaveData {
    Profile players[MAX_PLAYERS];
    Settings settings;
};

Color playerColor(int idx);
const char* playerColorName(int idx);
constexpr int PLAYER_COLORS = 8;

namespace save {

SaveData& data();
void load();
void store();
// Indices of active profiles, in seat order (k-th active player uses controller k).
std::vector<int> active();
Profile& player(int idx);
// Records the result of a settled bet/round for statistics.
void recordResult(int idx, i64 net);

} // namespace save
