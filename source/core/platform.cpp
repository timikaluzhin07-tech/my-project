#include "platform.h"

#include <sys/stat.h>

#include <cstring>
#include <fstream>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace platform {

#ifdef __SWITCH__
static bool g_romfs = false;
#endif

void init() {
#ifdef __SWITCH__
    g_romfs = R_SUCCEEDED(romfsInit());
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/GrandCasinoNX", 0777);
    // Keep the screen awake while players think over a hand.
    appletSetMediaPlaybackState(true);
#endif
}

void shutdown() {
#ifdef __SWITCH__
    appletSetMediaPlaybackState(false);
    if (g_romfs) romfsExit();
#endif
}

bool isSwitch() {
#ifdef __SWITCH__
    return true;
#else
    return false;
#endif
}

bool running() {
#ifdef __SWITCH__
    return appletMainLoop();
#else
    return true;
#endif
}

std::string assetPath(const std::string& rel) {
#ifdef __SWITCH__
    return "romfs:/" + rel;
#else
    return "romfs/" + rel;
#endif
}

std::string savePath(const std::string& file) {
#ifdef __SWITCH__
    return "sdmc:/switch/GrandCasinoNX/" + file;
#else
    return file;
#endif
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return false; }
    out.resize((size_t)n);
    size_t got = n ? fread(out.data(), 1, (size_t)n, f) : 0;
    fclose(f);
    return got == (size_t)n;
}

bool writeFile(const std::string& path, const std::string& data) {
    // Write to a temp file first so a crash mid-write never corrupts the save.
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = (fclose(f) == 0) && ok;
    if (!ok) return false;
    remove(path.c_str());
    return rename(tmp.c_str(), path.c_str()) == 0;
}

bool editText(const std::string& header, const std::string& initial, int maxLen, std::string& out) {
#ifdef __SWITCH__
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) return false;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, header.c_str());
    swkbdConfigSetGuideText(&kbd, "Имя игрока");
    swkbdConfigSetInitialText(&kbd, initial.c_str());
    swkbdConfigSetStringLenMax(&kbd, (u32)maxLen);
    swkbdConfigSetOkButtonText(&kbd, "Готово");
    char buf[128] = {0};
    Result rc = swkbdShow(&kbd, buf, sizeof buf);
    swkbdClose(&kbd);
    if (R_FAILED(rc) || buf[0] == 0) return false;
    out = buf;
    return true;
#else
    (void)header;
    (void)maxLen;
    static const char* names[] = {"Алекс", "Мария", "Дмитрий", "Анна", "Максим", "Ольга", "Игорь", "Вика"};
    int n = (int)(sizeof names / sizeof names[0]);
    int idx = 0;
    for (int i = 0; i < n; i++)
        if (initial == names[i]) idx = i + 1;
    out = names[idx % n];
    return true;
#endif
}

void controllerSetup(int players) {
#ifdef __SWITCH__
    HidLaControllerSupportArg arg;
    hidLaCreateControllerSupportArg(&arg);
    arg.hdr.player_count_min = 1;
    arg.hdr.player_count_max = (s8)std::clamp(players, 1, 8);
    arg.hdr.enable_take_over_connection = 1;
    arg.hdr.enable_left_justify = 1;
    arg.hdr.enable_permit_joy_dual = 1;
    arg.hdr.enable_single_mode = players <= 1 ? 1 : 0;
    HidLaControllerSupportResultInfo info;
    hidLaShowControllerSupport(&info, &arg);
#else
    (void)players;
#endif
}

} // namespace platform
