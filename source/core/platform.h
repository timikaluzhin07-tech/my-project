// Thin layer over everything that differs between the Switch build and the PC test build.
#pragma once

#include "common.h"

namespace platform {

void init();
void shutdown();
bool isSwitch();
// False once the system asks the app to close (Switch applet loop).
bool running();

// Read-only bundled assets (romfs on Switch, ./romfs next to the binary on PC).
std::string assetPath(const std::string& rel);
// Writable save location (sdmc:/switch/GrandCasinoNX on Switch).
std::string savePath(const std::string& file);

bool readFile(const std::string& path, std::vector<uint8_t>& out);
bool writeFile(const std::string& path, const std::string& data);

// System software keyboard. Returns false if the player cancelled.
// On PC there is no keyboard applet, so it cycles through a list of preset names.
bool editText(const std::string& header, const std::string& initial, int maxLen, std::string& out);

// Opens the system controller applet (Change Grip/Order) for the given player count.
void controllerSetup(int players);

} // namespace platform
