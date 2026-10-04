// Software "shaders" that turn flat masks into lit, volumetric sprites:
// distance-field bevels, normal maps, environment-mapped materials, faceted gems,
// glossy spheres and ray-cast casino chips.
#pragma once

#include "../core/gfx.h"

namespace shade {

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 norm() const {
        float l = std::sqrt(x * x + y * y + z * z);
        return l > 1e-6f ? Vec3{x / l, y / l, z / l} : Vec3{0, 0, 1};
    }
};
inline Vec3 rgb(Color c) { return {c.r / 255.f, c.g / 255.f, c.b / 255.f}; }

// Screen space: x right, y down, z towards the viewer.
// Casino interior "HDRI": warm ceiling lights, dark floor, two soft boxes.
Vec3 environment(const Vec3& dir);

struct Material {
    float ambient = 0.32f;
    float diffuse = 0.75f;
    float spec = 0.35f;       // Blinn-Phong highlight strength
    float shininess = 40.f;
    float reflect = 0.08f;    // dielectric environment reflection
    float metal = 0.f;        // 1 = metal (reflection tinted by albedo, no diffuse)
    float clearcoat = 0.f;    // sharp lacquer reflection on top
    float rim = 0.f;          // grazing light
    float exposure = 1.f;
};

namespace mat {
Material clay();       // casino chip body
Material gloss();      // polished plastic / enamel
Material lacquer();    // varnished wood
Material leather();
Material felt();
Material paper();
Material gold();
Material silver();
Material ivory();
} // namespace mat

// Euclidean distance (pixels) from each inside pixel to the shape edge.
std::vector<float> insideDistance(const Image& mask, int threshold = 128);

// Shades `img` (its colours are the albedo, its alpha the shape) as a bevelled solid.
// bevel: width of the rounded edge in pixels; profile 1 = quarter round, 0 = chamfer.
void bevel(Image& img, float bevelPx, float depth, const Material& m, float profile = 1.f);

// Same, but with an explicit height field (0..1) per pixel.
void shadeHeight(Image& img, const std::vector<float>& height, float depthPx, const Material& m);

// One layer of a relief stack: a coloured SVG drawing shaded as a bevelled solid.
struct Layer {
    std::string svg;
    Material mat;
    float bevel = 3;       // logical px
    float depth = 1;       // height scale
    float profile = 1;
    bool flat = false;     // printed ink: composite without shading
    float grain = 0;       // albedo noise amount (wood/leather/clay)
    float grainCell = 1;
    int shadow = 0;        // drop shadow radius in logical px (0 = none)
    float shadowOpacity = 0.5f;
    std::function<void(Image&)> paint; // optional albedo pass (wood grain...) before shading
};
// Renders layers bottom to top into a w x h (logical) image.
Image relief(float w, float h, const std::vector<Layer>& layers, float scale = TEX_SCALE);

// A brilliant-cut gem seen from above. outline: girdle points (logical, around cx,cy).
Image gem(float w, float h, const std::vector<SDL_FPoint>& girdle, SDL_FPoint centre, float table, Color body,
          float scale = TEX_SCALE, float crownHeight = 0.42f);

// Glossy sphere (ball / pearl) or glass orb with an inner glow.
Image sphere(float radius, Color albedo, const Material& m, float scale = TEX_SCALE);
Image glassOrb(float radius, Color tint, float scale = TEX_SCALE);

// Ray-casts a casino chip seen from the table edge: top face textured with `top`
// (a square top-view image), striped rim. Returns the sprite and the rim height (logical).
Image chipPiece(const Image& top, Color base, Color spot, float radius, float thick, float elevationDeg,
                float scale = TEX_SCALE, float* rimHeight = nullptr);

// Adds sparkle stars (additive white) at bright spots — jewellery glints.
void glints(Image& img, int count, uint32_t seed, float size);

} // namespace shade
