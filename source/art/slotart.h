// Art for the "Wrath of Zeus" slot: gems, golden relics, scatter, orbs, Zeus medallion, temple.
#pragma once

#include <map>

#include "../core/gfx.h"
#include "../games/slot_logic.h"

namespace slotart {

constexpr float CELL = 82.f;
constexpr float SYM = 80.f; // symbol texture size (logical)

struct Assets {
    Tex sym[slot::SYM_COUNT];
    Tex symGlow[slot::SYM_COUNT];   // blurred silhouettes for win highlights
    std::map<int, Tex> orbs;        // by multiplier value
    Tex orbGlow;
    Tex medallion;
    Tex background;
    Tex frame;
    Tex coin;
};

void build(Assets& a);
Color symbolColor(slot::Sym s);
Color orbColor(int value);

} // namespace slotart
