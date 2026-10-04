// Grand Casino NX — entry point, main loop, scene transitions and test-script runner.
#include <SDL.h>

#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>

#include "core/app.h"
#include "core/audio.h"
#include "core/gfx.h"
#include "core/input.h"
#include "core/platform.h"
#include "core/save.h"
#include "core/ui.h"
#include "art/art.h"

namespace {

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
std::unique_ptr<Scene> g_scene;
bool g_quit = false;
float g_time = 0;
uint32_t g_frame = 0;
bool g_test = false;

enum TransState { T_NONE, T_OUT, T_IN };
TransState g_trans = T_NONE;
float g_fade = 0;
SceneId g_pending = SC_HALL;
audio::Music g_currentMusic = audio::MUS_NONE;
int g_ambience = 0;

std::unique_ptr<Scene> build(SceneId id) {
    switch (id) {
        case SC_BOOT: return makeBootScene();
        case SC_HALL: return makeHallScene();
        case SC_BLACKJACK: return makeBlackjackScene();
        case SC_POKER: return makePokerScene();
        case SC_ROULETTE: return makeRouletteScene();
        case SC_SLOTS: return makeSlotsScene();
        case SC_PLAYERS: return makePlayersScene();
        case SC_SETTINGS: return makeSettingsScene();
    }
    return makeHallScene();
}

void syncSceneAudio() {
    if (!g_scene) return;
    audio::Music m = g_scene->music();
    if (m != g_currentMusic) {
        audio::music(m, 1.5f);
        g_currentMusic = m;
    }
    bool amb = g_scene->ambience();
    if (amb && !g_ambience) g_ambience = audio::loop(audio::SFX_AMBIENCE, 0.0f);
    if (g_ambience) audio::setLoop(g_ambience, amb ? 0.28f : 0.0f, 1.f);
}

// ---------------------------------------------------------------------------
// Test script: lets the PC build be driven headlessly and screenshot.
// ---------------------------------------------------------------------------
struct Script {
    std::vector<std::string> lines;
    size_t pc = 0;
    int wait = 0;
    std::string shotDir = "shots";
    std::string pendingShot;
    std::string iconPath;
    struct Hold { int pad; u32 mask; int frames; };
    std::vector<Hold> holds;
    bool active() const { return !lines.empty(); }

    static u32 btn(const std::string& s) {
        static const std::map<std::string, u32> m = {
            {"A", BTN_A}, {"B", BTN_B}, {"X", BTN_X}, {"Y", BTN_Y}, {"L", BTN_L}, {"R", BTN_R},
            {"ZL", BTN_ZL}, {"ZR", BTN_ZR}, {"PLUS", BTN_PLUS}, {"MINUS", BTN_MINUS}, {"UP", BTN_UP},
            {"DOWN", BTN_DOWN}, {"LEFT", BTN_LEFT}, {"RIGHT", BTN_RIGHT}};
        auto it = m.find(s);
        return it == m.end() ? 0 : it->second;
    }

    void step() {
        // apply holds
        u32 masks[MAX_PADS] = {};
        for (auto& h : holds) {
            if (h.frames > 0) masks[h.pad] |= h.mask;
            h.frames--;
        }
        holds.erase(std::remove_if(holds.begin(), holds.end(), [](const Hold& h) { return h.frames < -1; }),
                    holds.end());
        for (int p = 0; p < MAX_PADS; p++) input::inject(p, masks[p]);
        if (wait > 0) { wait--; return; }
        while (pc < lines.size() && wait == 0) {
            std::istringstream ss(lines[pc++]);
            std::string cmd;
            ss >> cmd;
            if (cmd.empty() || cmd[0] == '#') continue;
            if (cmd == "wait") { ss >> wait; }
            else if (cmd == "press" || cmd == "hold") {
                int pad = 0, frames = 3;
                ss >> pad;
                u32 mask = 0;
                std::string tok;
                while (ss >> tok) {
                    if (isdigit((unsigned char)tok[0])) frames = std::stoi(tok);
                    else mask |= btn(tok);
                }
                holds.push_back({pad, mask, frames});
                wait = frames + 2;
            } else if (cmd == "shot") { ss >> pendingShot; }
            else if (cmd == "go") {
                std::string s;
                ss >> s;
                static const std::map<std::string, SceneId> sc = {
                    {"hall", SC_HALL}, {"blackjack", SC_BLACKJACK}, {"poker", SC_POKER}, {"roulette", SC_ROULETTE},
                    {"slots", SC_SLOTS}, {"players", SC_PLAYERS}, {"settings", SC_SETTINGS}};
                if (sc.count(s)) app::go(sc.at(s));
                wait = 40;
            } else if (cmd == "players") {
                // players N : activate the first N profiles
                int n = 1;
                ss >> n;
                for (int i = 0; i < MAX_PLAYERS; i++) save::player(i).active = i < n;
            } else if (cmd == "balance") {
                // balance P AMOUNT : set a profile's chips (testing)
                int pl = 0;
                long long amount = 0;
                ss >> pl >> amount;
                save::player(pl).balance = amount;
            } else if (cmd == "seed") {
                u64 s;
                ss >> s;
                rng().reseed(s);
            } else if (cmd == "icon") { ss >> iconPath; }
            else if (cmd == "quit") { g_quit = true; }
            else if (cmd == "log") { std::string rest; std::getline(ss, rest); SDL_Log("script:%s", rest.c_str()); }
        }
        if (pc >= lines.size() && wait == 0 && holds.empty()) g_quit = true;
    }
};
Script g_script;

// Renders the 256x256 homebrew-menu icon into the top-left corner and saves it.
void renderIcon(const std::string& path) {
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    const float S = 256;
    gfx::rectGrad(0, 0, S, S, Color::hex(0x2a0a12), Color::hex(0x0a0306));
    gfx::glow(128, 112, 150, Color::hex(0x1f7d4e), 0.55f, false);
    gfx::glow(128, 100, 110, Color::hex(0xffc35a), 0.18f);
    art::drawChipStack(5500, 64, 150, 0.95f);
    art::drawCard(Card{14, SPADES}, 120, 102, 1.0f, -12);
    art::drawCard(Card{14, HEARTS}, 160, 108, 1.0f, 10);
    gfx::rectGrad(0, 168, S, 88, Color(0, 0, 0, 0), Color(0, 0, 0, 230));
    gfx::textGold("GRAND", 128, 196, F_TITLE, 40);
    gfx::textGold("CASINO", 128, 232, F_TITLE, 34);
    gfx::roundRectOutline(4, 4, S - 8, S - 8, 18, 3, pal::gold);
    std::vector<uint8_t> px(256 * 256 * 3);
    SDL_Rect r = {0, 0, 256, 256};
    SDL_RenderReadPixels(g_renderer, &r, SDL_PIXELFORMAT_RGB24, px.data(), 256 * 3);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(px.data(), 256, 256, 24, 256 * 3, SDL_PIXELFORMAT_RGB24);
    SDL_SaveBMP(s, path.c_str());
    SDL_FreeSurface(s);
    SDL_Log("icon saved to %s", path.c_str());
}

void saveShot(const std::string& name) {
    int w, h;
    SDL_GetRendererOutputSize(g_renderer, &w, &h);
    std::vector<uint8_t> px((size_t)w * h * 3);
    SDL_RenderReadPixels(g_renderer, nullptr, SDL_PIXELFORMAT_RGB24, px.data(), w * 3);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(px.data(), w, h, 24, w * 3, SDL_PIXELFORMAT_RGB24);
    std::string path = g_script.shotDir + "/" + name + ".bmp";
    SDL_SaveBMP(s, path.c_str());
    SDL_FreeSurface(s);
    SDL_Log("saved %s", path.c_str());
}

} // namespace

namespace app {
void go(SceneId id) {
    if (g_trans == T_OUT) return;
    g_pending = id;
    g_trans = T_OUT;
    audio::play(audio::SFX_WHOOSH, 0.5f);
}
void quit() { g_quit = true; }
float time() { return g_time; }
uint32_t frame() { return g_frame; }
bool testMode() { return g_test; }
void applyVolumes() {
    const Settings& s = save::data().settings;
    audio::setVolumes(s.master, s.music, s.sfx);
}
} // namespace app

int main(int argc, char** argv) {
    int winW = 1280, winH = 720;
    int maxFrames = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--script") && i + 1 < argc) {
            std::ifstream f(argv[++i]);
            std::string line;
            while (std::getline(f, line)) g_script.lines.push_back(line);
            g_test = true;
        } else if (!strcmp(argv[i], "--shots") && i + 1 < argc) {
            g_script.shotDir = argv[++i];
        } else if (!strcmp(argv[i], "--hd")) {
            winW = 1920; winH = 1080;
        } else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            maxFrames = atoi(argv[++i]);
        }
    }
    platform::init();
    rng().reseed(g_test ? 12345 : (u64)::time(nullptr) ^ ((u64)SDL_GetPerformanceCounter() << 17));

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
#ifdef __SWITCH__
    // Match the console's current output (1080p docked, 720p handheld); the logical
    // 1280x720 canvas is scaled onto whatever the window ends up being.
    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0 && dm.w >= 1280 && dm.h >= 720) {
        winW = dm.w;
        winH = dm.h;
    }
    g_window = SDL_CreateWindow("Grand Casino NX", 0, 0, winW, winH, 0);
#else
    g_window = SDL_CreateWindow("Grand Casino NX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH,
                                SDL_WINDOW_RESIZABLE);
#endif
    if (!g_window) {
        SDL_Log("window: %s", SDL_GetError());
        return 1;
    }
    Uint32 rflags = g_test ? SDL_RENDERER_SOFTWARE : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    g_renderer = SDL_CreateRenderer(g_window, -1, rflags);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, 0);
    SDL_RenderSetLogicalSize(g_renderer, SCREEN_W, SCREEN_H);

    gfx::init(g_renderer);
    input::init();
    save::load();
    audio::init();
    app::applyVolumes();

    g_scene = build(g_test ? SC_HALL : SC_BOOT);
    syncSceneAudio();

    Uint64 last = SDL_GetPerformanceCounter();
    while (!g_quit && platform::running()) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) g_quit = true;
            input::handleEvent(e);
        }
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)((now - last) / (double)SDL_GetPerformanceFrequency());
        last = now;
        if (g_test) dt = 1.f / 60.f;
        dt = std::min(dt, 1.f / 20.f);
        g_time += dt;

        if (g_script.active()) g_script.step(); // injects scripted buttons
        input::update(dt);
        ui::updateToasts(dt);

        if (g_trans == T_OUT) {
            g_fade = std::min(1.f, g_fade + dt / 0.28f);
            if (g_fade >= 1.f) {
                // Show a black frame with a spinner while the next scene builds its art.
                SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
                SDL_RenderClear(g_renderer);
                ui::spinner(SCREEN_W / 2.f, SCREEN_H / 2.f, 16, g_time);
                SDL_RenderPresent(g_renderer);
                g_scene.reset();
                Uint64 t0 = SDL_GetPerformanceCounter();
                g_scene = build(g_pending);
                SDL_Log("scene %d built in %.0f ms", (int)g_pending,
                        (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency());
                syncSceneAudio();
                input::consume();
                g_trans = T_IN;
                last = SDL_GetPerformanceCounter();
            }
        } else if (g_trans == T_IN) {
            g_fade = std::max(0.f, g_fade - dt / 0.35f);
            if (g_fade <= 0) g_trans = T_NONE;
        }
        if (g_scene && g_trans != T_OUT) g_scene->update(dt);
        syncSceneAudio();

        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);
        if (g_scene) g_scene->render();
        ui::renderToasts();
        if (g_fade > 0) gfx::dim(ease::inOutCubic(g_fade));
        if (!g_script.iconPath.empty()) {
            renderIcon(g_script.iconPath);
            g_script.iconPath.clear();
        }
        if (!g_script.pendingShot.empty()) {
            saveShot(g_script.pendingShot);
            g_script.pendingShot.clear();
        }
        SDL_RenderPresent(g_renderer);
        gfx::frameTick();
        g_frame++;
        if (maxFrames && (int)g_frame >= maxFrames) g_quit = true;
    }

    save::store();
    g_scene.reset();
    art::shutdown();
    audio::shutdown();
    input::shutdown();
    gfx::shutdown();
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
    platform::shutdown();
    return 0;
}
