// Rendering: procedural images (SVG + per-pixel ops), GPU textures, primitives and text.
// All draw calls take logical 1280x720 coordinates.
#pragma once

#include <SDL.h>

#include "common.h"

struct _TTF_Font;

// CPU-side RGBA8 image with straight (non-premultiplied) alpha, in physical pixels.
struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> px;

    Image() = default;
    Image(int w_, int h_, Color fill = Color(0, 0, 0, 0));
    bool empty() const { return w == 0 || h == 0; }
    uint8_t* at(int x, int y) { return &px[((size_t)y * w + x) * 4]; }
    const uint8_t* at(int x, int y) const { return &px[((size_t)y * w + x) * 4]; }

    void blend(int x, int y, Color c, float coverage = 1.f);
    // Composite another image on top (source-over) at a pixel offset.
    void draw(const Image& src, int dx, int dy, float alpha = 1.f);
    // Composite a white text/mask image tinted by color.
    void drawTinted(const Image& mask, int dx, int dy, Color tint);
    // Composite rotated around (cx, cy) in this image's pixels; mask is tinted.
    void drawRotated(const Image& mask, float cx, float cy, float angleRad, Color tint, float scale = 1.f);
    // Multiplies the rgb of opaque pixels by (1 + amount * noise), noise in [-1,1].
    void grain(float amount, uint32_t seed, float cell = 1.f);
    // Darkens edges with an elliptical falloff.
    void vignette(float strength, float inner = 0.45f);
    Image blurred(int radius) const;
    // Black copy with blurred alpha — a drop shadow.
    Image shadow(int radius, float opacity) const;
    Image flipped180() const;
};

// RAII GPU texture. w/h are logical sizes (pixel size / scale).
struct Tex {
    SDL_Texture* tex = nullptr;
    int pw = 0, ph = 0;
    float w = 0, h = 0;

    Tex() = default;
    Tex(const Tex&) = delete;
    Tex& operator=(const Tex&) = delete;
    Tex(Tex&& o) noexcept { *this = std::move(o); }
    Tex& operator=(Tex&& o) noexcept;
    ~Tex();
    explicit operator bool() const { return tex != nullptr; }
};

enum FontId { F_TITLE, F_SERIF, F_SERIF_REG, F_NUM, F_SANS, F_SANS_BOLD, F_COUNT };

enum TextStyle { TS_PLAIN = 0, TS_GOLD = 1, TS_GLOW = 2 };

namespace noise {
float hash2(int x, int y, uint32_t seed);
float value(float x, float y, uint32_t seed);           // [0,1]
float fbm(float x, float y, int octaves, uint32_t seed); // [0,1]
} // namespace noise

namespace gfx {

bool init(SDL_Renderer* r);
void shutdown();
SDL_Renderer* renderer();
void frameTick();

// ---- image / texture creation ----
Image rasterSvg(const std::string& svg, float scale = TEX_SCALE);
Tex upload(const Image& img, float scale = TEX_SCALE);
Tex svgTex(const std::string& svg, float scale = TEX_SCALE);

// ---- drawing ----
void draw(const Tex& t, float x, float y, float alpha = 1.f, Color tint = pal::white);
void drawScaled(const Tex& t, float x, float y, float w, float h, float alpha = 1.f, Color tint = pal::white,
                bool additive = false);
void drawCentered(const Tex& t, float cx, float cy, float scale = 1.f, float angleDeg = 0.f, float alpha = 1.f,
                  Color tint = pal::white, bool additive = false);
// Separate x/y scale (card flips, squash) around the centre.
void drawCenteredXY(const Tex& t, float cx, float cy, float sx, float sy, float angleDeg = 0.f, float alpha = 1.f,
                    Color tint = pal::white, bool additive = false);
// Draws the source sub-rectangle (logical units within the texture).
void drawPart(const Tex& t, float sx, float sy, float sw, float sh, float x, float y, float w, float h,
              float alpha = 1.f);

// Nine-slice: corners keep their size (border, logical), edges and centre stretch.
void nine(const Tex& t, float x, float y, float w, float h, float border, float alpha = 1.f, Color tint = pal::white);

void rect(float x, float y, float w, float h, Color c);
void rectGrad(float x, float y, float w, float h, Color top, Color bottom);
void rectGradH(float x, float y, float w, float h, Color left, Color right);
void roundRect(float x, float y, float w, float h, float r, Color c);
void roundRectOutline(float x, float y, float w, float h, float r, float thick, Color c);
void line(float x1, float y1, float x2, float y2, float thick, Color c, bool additive = false);
void circle(float cx, float cy, float r, Color c);
void ring(float cx, float cy, float r, float thick, Color c);
void ellipse(float cx, float cy, float rx, float ry, Color c);
// Soft radial glow (additive by default).
void glow(float cx, float cy, float radius, Color c, float alpha = 1.f, bool additive = true);
void glowEllipse(float cx, float cy, float rx, float ry, Color c, float alpha = 1.f, bool additive = true);
void triangle(SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, Color ca, Color cb, Color cc, bool additive = false);
void quad(SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_FPoint d, Color ca, Color cb, Color cc, Color cd,
          bool additive = false);

void clip(float x, float y, float w, float h);
void unclip();
// Full-screen tint (fades, dimming behind dialogs).
void dim(float alpha, Color c = pal::black);

// ---- text ----
// align: -1 left, 0 centre, 1 right. y is the vertical centre of the line.
void text(const std::string& s, float x, float y, FontId f, float size, Color c, int align = -1,
          float alpha = 1.f, TextStyle style = TS_PLAIN);
// Text with a soft dark drop shadow underneath.
void textShadow(const std::string& s, float x, float y, FontId f, float size, Color c, int align = -1,
                float alpha = 1.f);
void textGold(const std::string& s, float x, float y, FontId f, float size, int align = 0, float alpha = 1.f,
              bool shadow = true);
void textGlow(const std::string& s, float x, float y, FontId f, float size, Color glowColor, float glowAlpha,
              int align = 0);
// Word-wrapped paragraph, y is the top. Returns the height used.
float textWrapped(const std::string& s, float x, float y, float wrapW, FontId f, float size, Color c,
                  float lineGap = 1.25f, float alpha = 1.f);
float textWidth(const std::string& s, FontId f, float size);
// Rendered white glyph mask for compositing into Images (pixel units at the given scale).
Image textMask(const std::string& s, FontId f, float size, float scale = TEX_SCALE);

} // namespace gfx
