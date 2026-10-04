// Helpers shared by the table games: printed felt lettering, the chip rack HUD,
// dealer captions and seat name plates.
#pragma once

#include "../core/gfx.h"
#include "art.h"

namespace art {

// Lays text along the lower arc of an ellipse (pixel image, logical coordinates),
// glyph tops facing the ellipse centre, centred at the bottom of the arc.
void arcText(Image& img, const std::string& s, float cx, float cy, float rx, float ry, FontId f, float size, Color c,
             float tracking = 1.0f);
// Straight text stamped into an image (centred).
void stampText(Image& img, const std::string& s, float cx, float cy, FontId f, float size, Color c,
               float angleDeg = 0.f);

// Row of chip denominations; the selected one is raised.
void drawChipRack(int selected, float cx, float y, i64 affordable, float alpha = 1.f);

// Dealer speech line ("Ставки сделаны").
struct Caption {
    std::string text;
    float t = 99, dur = 0;
    void say(const std::string& s, float seconds = 2.4f) { text = s; t = 0; dur = seconds; }
    void update(float dt) { t += dt; }
    void render(float cx, float cy) const;
};

// Seat plate: colour chip, name, balance (or override), highlight when acting.
void seatPlate(int profile, float cx, float cy, bool active, float pulse, const std::string& line2 = "",
               Color line2Color = pal::goldLight, float alpha = 1.f, float width = 170.f);

} // namespace art
