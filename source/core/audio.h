// Software mixer with fully synthesized sound effects and music (no audio files needed).
#pragma once

#include "common.h"

namespace audio {

enum Sfx {
    SFX_NAV,
    SFX_SELECT,
    SFX_BACK,
    SFX_ERROR,
    SFX_CARD_SLIDE,
    SFX_CARD_FLIP,
    SFX_CHIP,
    SFX_CHIPS,
    SFX_SHUFFLE,
    SFX_WIN,
    SFX_BIGWIN,
    SFX_LOSE,
    SFX_BALL_LOOP,
    SFX_BALL_CLACK,
    SFX_WHOOSH,
    SFX_REEL_DROP,
    SFX_LAND,
    SFX_SHATTER,
    SFX_ZAP,
    SFX_THUNDER,
    SFX_COIN,
    SFX_TICK,
    SFX_ORB,
    SFX_BOOM,
    SFX_AMBIENCE,
    SFX_KNOCK,
    SFX_COUNT
};

enum Music { MUS_NONE, MUS_LOUNGE, MUS_OLYMPUS, MUS_COUNT };

void init();
void shutdown();

void play(Sfx s, float vol = 1.f, float pan = 0.f, float pitch = 1.f);
// Looping voice; returns a handle (0 = failed).
int loop(Sfx s, float vol, float pitch = 1.f);
void setLoop(int handle, float vol, float pitch);
void stopLoop(int handle, float fadeSeconds = 0.15f);

void music(Music m, float fadeSeconds = 1.2f);
void setVolumes(float master, float music, float sfx);
bool musicReady();

} // namespace audio
