// Shared procedural art: playing cards, casino chips and their drawing helpers.
#pragma once

#include "../core/gfx.h"
#include "../games/cards.h"

namespace art {

constexpr float CARD_W = 96.f;
constexpr float CARD_H = 134.f;
constexpr int CHIP_COUNT = 7;
constexpr float CHIP_R = 23.f; // logical radius of a chip at scale 1
extern const i64 CHIP_VALUES[CHIP_COUNT];

// Builds everything (call repeatedly with increasing step for a progress bar).
int buildSteps();
void buildStep(int step);
void ensureBuilt();
void shutdown();

std::string suitShape(int suit);   // SVG elements in a 100x100 box
Color suitColor(int suit);

const Tex& cardFace(const Card& c);
const Tex& cardBack();
const Tex& chip(int denom);
const Tex& playerChip(int colorIdx);

// faceUp: 0 = back, 1 = face; values between animate the flip.
void drawCard(const Card& c, float cx, float cy, float scale = 1.f, float angleDeg = 0.f, float faceUp = 1.f,
              float alpha = 1.f, bool shadow = true, Color tint = pal::white);
void cardHighlight(float cx, float cy, float scale, Color c, float alpha, float angleDeg = 0.f);
// Empty card outline printed on felt.
void cardSlot(float cx, float cy, float scale, float alpha = 1.f);

void drawChip(int denom, float cx, float cy, float scale = 1.f, float alpha = 1.f);
// Stack(s) of chips that add up to `amount`; returns the width used.
float drawChipStack(i64 amount, float cx, float cy, float scale = 1.f, float alpha = 1.f, int maxStacks = 3);
void drawPlayerChipStack(int colorIdx, int count, float cx, float cy, float scale = 1.f, float alpha = 1.f);
// Index of the largest chip value <= v (for chip selectors).
int chipIndexFor(i64 v);

} // namespace art
