// Scene management, main loop services and the test-script harness.
#pragma once

#include "audio.h"
#include "common.h"

class Scene {
public:
    virtual ~Scene() = default;
    virtual void update(float dt) = 0;
    virtual void render() = 0;
    virtual audio::Music music() const { return audio::MUS_LOUNGE; }
    virtual bool ambience() const { return true; }
};

enum SceneId { SC_BOOT, SC_HALL, SC_BLACKJACK, SC_POKER, SC_ROULETTE, SC_SLOTS, SC_PLAYERS, SC_SETTINGS };

namespace app {

// Fades out, builds the new scene, fades in.
void go(SceneId id);
void quit();
float time();       // seconds since launch
uint32_t frame();
bool testMode();
void applyVolumes();

} // namespace app

// Implemented by each scene file.
std::unique_ptr<Scene> makeBootScene();
std::unique_ptr<Scene> makeHallScene();
std::unique_ptr<Scene> makeBlackjackScene();
std::unique_ptr<Scene> makePokerScene();
std::unique_ptr<Scene> makeRouletteScene();
std::unique_ptr<Scene> makeSlotsScene();
std::unique_ptr<Scene> makePlayersScene();
std::unique_ptr<Scene> makeSettingsScene();
