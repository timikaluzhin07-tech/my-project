#include "input.h"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace input {

namespace {

constexpr u32 REPEATABLE = BTN_DIRS | BTN_L | BTN_R | BTN_ZL | BTN_ZR;

struct Pad {
    bool connected = false;
    u32 raw = 0;       // physical state this frame
    u32 consumed = 0;  // held buttons ignored until released
    u32 held = 0, prev = 0, pressed = 0, rep = 0;
    float holdTime[32] = {};
    float nextRep[32] = {};
    u32 injected = 0;
    bool injectActive = false;
};

Pad g_pads[MAX_PADS];

#ifdef __SWITCH__
PadState g_nx[MAX_PADS];

u32 mapNx(u64 b) {
    u32 m = 0;
    if (b & HidNpadButton_A) m |= BTN_A;
    if (b & HidNpadButton_B) m |= BTN_B;
    if (b & HidNpadButton_X) m |= BTN_X;
    if (b & HidNpadButton_Y) m |= BTN_Y;
    if (b & (HidNpadButton_L | HidNpadButton_AnySL)) m |= BTN_L;
    if (b & (HidNpadButton_R | HidNpadButton_AnySR)) m |= BTN_R;
    if (b & HidNpadButton_ZL) m |= BTN_ZL;
    if (b & HidNpadButton_ZR) m |= BTN_ZR;
    if (b & HidNpadButton_Plus) m |= BTN_PLUS;
    if (b & HidNpadButton_Minus) m |= BTN_MINUS;
    if (b & HidNpadButton_AnyUp) m |= BTN_UP;
    if (b & HidNpadButton_AnyDown) m |= BTN_DOWN;
    if (b & HidNpadButton_AnyLeft) m |= BTN_LEFT;
    if (b & HidNpadButton_AnyRight) m |= BTN_RIGHT;
    return m;
}
#else
SDL_GameController* g_ctrl[MAX_PADS] = {};

u32 keyboardPad0() {
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    u32 m = 0;
    if (k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER]) m |= BTN_A;
    if (k[SDL_SCANCODE_BACKSPACE]) m |= BTN_B;
    if (k[SDL_SCANCODE_X]) m |= BTN_X;
    if (k[SDL_SCANCODE_C]) m |= BTN_Y;
    if (k[SDL_SCANCODE_Q]) m |= BTN_L;
    if (k[SDL_SCANCODE_E]) m |= BTN_R;
    if (k[SDL_SCANCODE_1]) m |= BTN_ZL;
    if (k[SDL_SCANCODE_3]) m |= BTN_ZR;
    if (k[SDL_SCANCODE_ESCAPE]) m |= BTN_PLUS;
    if (k[SDL_SCANCODE_TAB]) m |= BTN_MINUS;
    if (k[SDL_SCANCODE_UP]) m |= BTN_UP;
    if (k[SDL_SCANCODE_DOWN]) m |= BTN_DOWN;
    if (k[SDL_SCANCODE_LEFT]) m |= BTN_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) m |= BTN_RIGHT;
    return m;
}

u32 gamepad(SDL_GameController* c) {
    if (!c) return 0;
    auto b = [&](SDL_GameControllerButton x) { return SDL_GameControllerGetButton(c, x) != 0; };
    u32 m = 0;
    // Nintendo layout: A is the right face button.
    if (b(SDL_CONTROLLER_BUTTON_B)) m |= BTN_A;
    if (b(SDL_CONTROLLER_BUTTON_A)) m |= BTN_B;
    if (b(SDL_CONTROLLER_BUTTON_Y)) m |= BTN_X;
    if (b(SDL_CONTROLLER_BUTTON_X)) m |= BTN_Y;
    if (b(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) m |= BTN_L;
    if (b(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) m |= BTN_R;
    if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) m |= BTN_ZL;
    if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) m |= BTN_ZR;
    if (b(SDL_CONTROLLER_BUTTON_START)) m |= BTN_PLUS;
    if (b(SDL_CONTROLLER_BUTTON_BACK)) m |= BTN_MINUS;
    if (b(SDL_CONTROLLER_BUTTON_DPAD_UP)) m |= BTN_UP;
    if (b(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) m |= BTN_DOWN;
    if (b(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) m |= BTN_LEFT;
    if (b(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) m |= BTN_RIGHT;
    int lx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    int ly = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
    if (lx < -20000) m |= BTN_LEFT;
    if (lx > 20000) m |= BTN_RIGHT;
    if (ly < -20000) m |= BTN_UP;
    if (ly > 20000) m |= BTN_DOWN;
    return m;
}
#endif

} // namespace

void init() {
#ifdef __SWITCH__
    padConfigureInput(MAX_PADS, HidNpadStyleSet_NpadStandard);
    // A lone Joy-Con held sideways becomes a full mini controller.
    hidSetNpadJoyHoldType(HidNpadJoyHoldType_Horizontal);
    padInitialize(&g_nx[0], HidNpadIdType_No1, HidNpadIdType_Handheld);
    for (int i = 1; i < MAX_PADS; i++) padInitialize(&g_nx[i], (HidNpadIdType)(HidNpadIdType_No1 + i));
#else
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
#endif
}

void shutdown() {
#ifndef __SWITCH__
    for (auto& c : g_ctrl)
        if (c) { SDL_GameControllerClose(c); c = nullptr; }
#endif
}

void handleEvent(const SDL_Event& e) {
#ifndef __SWITCH__
    if (e.type == SDL_CONTROLLERDEVICEADDED) {
        for (int i = 0; i < MAX_PADS; i++)
            if (!g_ctrl[i]) { g_ctrl[i] = SDL_GameControllerOpen(e.cdevice.which); break; }
    } else if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
        for (auto& c : g_ctrl)
            if (c && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c)) == e.cdevice.which) {
                SDL_GameControllerClose(c);
                c = nullptr;
            }
    }
#else
    (void)e;
#endif
}

void update(float dt) {
    for (int i = 0; i < MAX_PADS; i++) {
        Pad& p = g_pads[i];
        u32 raw = 0;
        bool conn = false;
#ifdef __SWITCH__
        padUpdate(&g_nx[i]);
        conn = padIsConnected(&g_nx[i]);
        raw = mapNx(padGetButtons(&g_nx[i]));
#else
        if (i == 0) { raw |= keyboardPad0(); conn = true; }
        if (g_ctrl[i]) { raw |= gamepad(g_ctrl[i]); conn = true; }
#endif
        if (p.injectActive) { raw |= p.injected; conn = true; }
        p.connected = conn;
        p.raw = raw;
        p.consumed &= raw;
        p.prev = p.held;
        p.held = raw & ~p.consumed;
        p.pressed = p.held & ~p.prev;
        p.rep = p.pressed;
        for (int b = 0; b < 32; b++) {
            u32 bit = 1u << b;
            if (!(REPEATABLE & bit)) continue;
            if (p.held & bit) {
                if (p.pressed & bit) {
                    p.holdTime[b] = 0;
                    p.nextRep[b] = 0.34f;
                } else {
                    p.holdTime[b] += dt;
                    if (p.holdTime[b] >= p.nextRep[b]) {
                        p.rep |= bit;
                        p.nextRep[b] += p.holdTime[b] > 1.4f ? 0.04f : 0.075f;
                    }
                }
            }
        }
    }
}

bool connected(int pad) { return pad >= 0 && pad < MAX_PADS && g_pads[pad].connected; }

int connectedCount() {
    int n = 0;
    for (auto& p : g_pads) n += p.connected;
    return n;
}

static u32 field(int pad, u32 Pad::*f) {
    if (pad >= 0 && pad < MAX_PADS) return g_pads[pad].*f;
    u32 m = 0;
    for (auto& p : g_pads) m |= p.*f;
    return m;
}

bool pressed(int pad, u32 btn) { return field(pad, &Pad::pressed) & btn; }
bool held(int pad, u32 btn) { return field(pad, &Pad::held) & btn; }
bool repeat(int pad, u32 btn) { return field(pad, &Pad::rep) & btn; }

int padForSeat(int seatOrder) {
    if (seatOrder < 0 || seatOrder >= MAX_PADS) return ANY_PAD;
    // With a single controller everyone shares it (pass-and-play).
    if (connectedCount() <= 1) return ANY_PAD;
    return connected(seatOrder) ? seatOrder : ANY_PAD;
}

void consume() {
    for (auto& p : g_pads) {
        p.consumed |= p.raw;
        p.held &= ~p.raw;
        p.pressed = 0;
        p.rep = 0;
    }
}

void inject(int pad, u32 heldMask) {
    if (pad < 0 || pad >= MAX_PADS) return;
    g_pads[pad].injectActive = true;
    g_pads[pad].injected = heldMask;
}

} // namespace input
