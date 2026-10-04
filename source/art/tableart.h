// Helpers shared by the table games: printed felt lettering, the chip rack HUD,
// dealer captions and seat name plates.
#pragma once

#include "../core/gfx.h"
#include "art.h"
#include "shade.h"

namespace art {

// Lays text along the lower arc of an ellipse (pixel image, logical coordinates),
// glyph tops facing the ellipse centre, centred at the bottom of the arc.
void arcText(Image& img, const std::string& s, float cx, float cy, float rx, float ry, FontId f, float size, Color c,
             float tracking = 1.0f);
// Straight text stamped into an image (centred).
void stampText(Image& img, const std::string& s, float cx, float cy, FontId f, float size, Color c,
               float angleDeg = 0.f);

// Lacquered wood: replaces the colour of every opaque pixel with a grain pattern.
// polar = true makes the grain follow rings around (cx, cy) (table rails, wheel bowls).
void woodGrain(Image& img, float cx, float cy, Color dark, Color light, uint32_t seed, bool polar, float stretch = 1.f);
// Felt under a pendant lamp: brightness falls off from (lx, ly); `edge` returns the
// distance (logical px) from a point to the felt's border (negative = outside).
void feltLight(Image& img, float lx, float ly, float rx, float ry, const std::function<float(float, float)>& edge,
               float ao = 26.f, float strength = 1.f);
// Padded leather rail along an SVG shape (evenodd path), with stitching.
Image leatherRail(const std::string& svgPath, float w, float h, Color base, float padding, const std::string& stitchSvg);
// Relief of the screen region (x, y, w, h): layer svgs are fragments in screen
// coordinates; shapes that leave the region are not bevelled at its border.
Image regionRelief(float x, float y, float w, float h, std::vector<shade::Layer> layers, float margin = 30.f);
// Noir portrait for a poker bot (deterministic per name).
struct PortraitTraits {
    bool female = false, hat = false, glasses = false, beard = false, cigar = false, redTie = false, earrings = false;
    Color skin, hair, suit;
};
PortraitTraits portraitTraits(const std::string& name);
Image botPortrait(const std::string& name, float size);
// Where the lit end of the cigar sits, relative to the portrait centre, per unit of size.
constexpr float CIGAR_TIP_X = 0.265f, CIGAR_TIP_Y = 0.125f;

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
