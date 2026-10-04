// Full-screen procedural backgrounds shared by menus and tables.
#pragma once

#include "../core/gfx.h"

namespace art {

// Dark velvet wall with a faint damask lattice.
Image velvetBackdrop(Color base, Color pattern, uint32_t seed = 1);
// Casino felt: smooth gradient, woven grain and vignette.
Image feltImage(int w, int h, Color centre, Color edge, uint32_t seed = 7, float scale = TEX_SCALE);

} // namespace art
