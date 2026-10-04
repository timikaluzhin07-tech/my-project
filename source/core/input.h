// Multi-controller input. Pad N corresponds to the Switch player number N+1
// (the LED on the controller); pad 0 also includes handheld mode.
#pragma once

#include <SDL.h>

#include "common.h"

enum Btn : u32 {
    BTN_A = 1u << 0,
    BTN_B = 1u << 1,
    BTN_X = 1u << 2,
    BTN_Y = 1u << 3,
    BTN_L = 1u << 4,
    BTN_R = 1u << 5,
    BTN_ZL = 1u << 6,
    BTN_ZR = 1u << 7,
    BTN_PLUS = 1u << 8,
    BTN_MINUS = 1u << 9,
    BTN_UP = 1u << 10,
    BTN_DOWN = 1u << 11,
    BTN_LEFT = 1u << 12,
    BTN_RIGHT = 1u << 13,
};
constexpr u32 BTN_DIRS = BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT;
constexpr int MAX_PADS = 8;
constexpr int ANY_PAD = -1;

namespace input {

void init();
void shutdown();
void handleEvent(const SDL_Event& e);
void update(float dt);

bool connected(int pad);
int connectedCount();

// pad == ANY_PAD merges every controller.
bool pressed(int pad, u32 btn);
bool held(int pad, u32 btn);
// pressed, plus auto-repeat while a direction / shoulder button is held.
bool repeat(int pad, u32 btn);

// Controller bound to the k-th seated player. Falls back to "any controller"
// when that controller is not connected, so a single pad can always drive everyone.
int padForSeat(int seatOrder);

// Swallow everything currently pressed until it is released (avoids a press
// that closed a menu also triggering the screen underneath).
void consume();

// Vibration (HD Rumble on Switch). strength 0..1, fades out over `seconds`.
// pad == ANY_PAD rumbles every connected controller. Respects the settings toggle.
void rumble(int pad, float strength, float seconds);

// Test harness: force a button mask on a virtual pad for the next frames.
void inject(int pad, u32 heldMask);

} // namespace input
