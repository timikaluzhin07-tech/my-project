#include "save.h"

#include <cstdlib>
#include <cstring>

#include "platform.h"

static const uint32_t kColors[PLAYER_COLORS] = {0xc8323c, 0x2f6fd6, 0x1f9d64, 0xe3a21a,
                                                 0x8f4fc4, 0x16a3b8, 0xe0663a, 0xd4d0c8};
static const char* kColorNames[PLAYER_COLORS] = {"Алый", "Сапфир", "Изумруд", "Янтарь",
                                                  "Аметист", "Бирюза", "Коралл", "Жемчуг"};

Color playerColor(int idx) { return Color::hex(kColors[((idx % PLAYER_COLORS) + PLAYER_COLORS) % PLAYER_COLORS]); }
const char* playerColorName(int idx) { return kColorNames[((idx % PLAYER_COLORS) + PLAYER_COLORS) % PLAYER_COLORS]; }

namespace save {

namespace {
SaveData g_data;
const char* kFile = "save.cfg";

void defaults() {
    g_data = SaveData();
    for (int i = 0; i < MAX_PLAYERS; i++) {
        g_data.players[i].name = "Игрок " + std::to_string(i + 1);
        g_data.players[i].color = i;
        g_data.players[i].active = (i == 0);
    }
}

std::string clean(std::string s) {
    for (char& c : s)
        if (c == '\n' || c == '\r' || c == '=') c = ' ';
    if (s.size() > 48) s.resize(48);
    return s;
}
} // namespace

SaveData& data() { return g_data; }

Profile& player(int idx) { return g_data.players[std::clamp(idx, 0, MAX_PLAYERS - 1)]; }

void load() {
    defaults();
    std::vector<uint8_t> raw;
    if (!platform::readFile(platform::savePath(kFile), raw)) return;
    std::string text(raw.begin(), raw.end());
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        long long n = strtoll(v.c_str(), nullptr, 10);
        float f = (float)atof(v.c_str());
        Settings& s = g_data.settings;
        if (k.size() > 3 && k[0] == 'p' && k[2] == '.') {
            int i = k[1] - '0';
            if (i < 0 || i >= MAX_PLAYERS) continue;
            Profile& p = g_data.players[i];
            std::string f2 = k.substr(3);
            if (f2 == "name") p.name = clean(v);
            else if (f2 == "color") p.color = (int)n;
            else if (f2 == "balance") p.balance = n;
            else if (f2 == "active") p.active = n != 0;
            else if (f2 == "stake") p.stake = n;
            else if (f2 == "best") p.biggestWin = n;
            else if (f2 == "won") p.totalWon = n;
            else if (f2 == "lost") p.totalLost = n;
            else if (f2 == "refills") p.refills = (int)n;
        } else if (k == "master") s.master = std::clamp(f, 0.f, 1.f);
        else if (k == "music") s.music = std::clamp(f, 0.f, 1.f);
        else if (k == "sfx") s.sfx = std::clamp(f, 0.f, 1.f);
        else if (k == "hideCards") s.hideCards = std::clamp((int)n, 0, 2);
        else if (k == "pokerBots") s.pokerBots = std::clamp((int)n, 0, 5);
        else if (k == "pokerBlinds") s.pokerBlinds = std::clamp((int)n, 0, 3);
        else if (k == "pokerBuyIn") s.pokerBuyIn = std::clamp((int)n, 0, 3);
        else if (k == "fastDeal") s.fastDeal = n != 0;
        else if (k == "slotTurbo") s.slotTurbo = n != 0;
        else if (k == "slotBet") s.slotBet = std::clamp((int)n, 0, 7);
        else if (k == "slotRotate") s.slotRotate = n != 0;
    }
    // Chips left on the poker table when the app was closed go back to the wallet.
    bool any = false;
    for (auto& p : g_data.players) {
        if (p.stake > 0) { p.balance += p.stake; p.stake = 0; }
        if (p.balance < 0) p.balance = 0;
        any |= p.active;
    }
    if (!any) g_data.players[0].active = true;
}

void store() {
    std::string out = "# Grand Casino NX save\n";
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Profile& p = g_data.players[i];
        std::string pre = "p" + std::to_string(i) + ".";
        out += pre + "name=" + clean(p.name) + "\n";
        out += pre + "color=" + std::to_string(p.color) + "\n";
        out += pre + "balance=" + std::to_string(p.balance) + "\n";
        out += pre + "active=" + std::to_string(p.active ? 1 : 0) + "\n";
        out += pre + "stake=" + std::to_string(p.stake) + "\n";
        out += pre + "best=" + std::to_string(p.biggestWin) + "\n";
        out += pre + "won=" + std::to_string(p.totalWon) + "\n";
        out += pre + "lost=" + std::to_string(p.totalLost) + "\n";
        out += pre + "refills=" + std::to_string(p.refills) + "\n";
    }
    const Settings& s = g_data.settings;
    out += strf("master=%.2f\nmusic=%.2f\nsfx=%.2f\n", s.master, s.music, s.sfx);
    out += strf("hideCards=%d\npokerBots=%d\npokerBlinds=%d\npokerBuyIn=%d\n", s.hideCards, s.pokerBots,
                s.pokerBlinds, s.pokerBuyIn);
    out += strf("fastDeal=%d\nslotTurbo=%d\nslotBet=%d\nslotRotate=%d\n", s.fastDeal ? 1 : 0, s.slotTurbo ? 1 : 0,
                s.slotBet, s.slotRotate ? 1 : 0);
    platform::writeFile(platform::savePath(kFile), out);
}

std::vector<int> active() {
    std::vector<int> v;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g_data.players[i].active) v.push_back(i);
    return v;
}

void recordResult(int idx, i64 net) {
    Profile& p = player(idx);
    if (net > 0) {
        p.totalWon += net;
        p.biggestWin = std::max(p.biggestWin, net);
    } else if (net < 0) {
        p.totalLost += -net;
    }
}

} // namespace save
