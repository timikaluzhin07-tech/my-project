#include "gfx.h"

#include <SDL_ttf.h>

#include <cstring>
#include <unordered_map>

#include "parallel.h"
#include "platform.h"

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "../../third_party/nanosvg/nanosvg.h"
#include "../../third_party/nanosvg/nanosvgrast.h"

// ============================================================================
// noise
// ============================================================================
namespace noise {

float hash2(int x, int y, uint32_t seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xffffff) / 16777215.0f;
}

float value(float x, float y, uint32_t seed) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    float a = hash2(xi, yi, seed), b = hash2(xi + 1, yi, seed);
    float c = hash2(xi, yi + 1, seed), d = hash2(xi + 1, yi + 1, seed);
    return lerp(lerp(a, b, u), lerp(c, d, u), v);
}

float fbm(float x, float y, int octaves, uint32_t seed) {
    float sum = 0, amp = 0.5f, norm = 0;
    for (int i = 0; i < octaves; i++) {
        sum += value(x, y, seed + i * 101u) * amp;
        norm += amp;
        x *= 2.03f;
        y *= 2.03f;
        amp *= 0.5f;
    }
    return sum / norm;
}

} // namespace noise

// ============================================================================
// Image
// ============================================================================
Image::Image(int w_, int h_, Color fill) : w(w_), h(h_), px((size_t)w_ * h_ * 4) {
    for (size_t i = 0; i < (size_t)w * h; i++) {
        px[i * 4 + 0] = fill.r;
        px[i * 4 + 1] = fill.g;
        px[i * 4 + 2] = fill.b;
        px[i * 4 + 3] = fill.a;
    }
}

static inline void blendPx(uint8_t* d, float sr, float sg, float sb, float sa) {
    if (sa <= 0.f) return;
    float da = d[3] / 255.f;
    float oa = sa + da * (1 - sa);
    if (oa <= 0.f) return;
    float k = da * (1 - sa);
    d[0] = (uint8_t)std::clamp((sr * sa + d[0] * k) / oa, 0.f, 255.f);
    d[1] = (uint8_t)std::clamp((sg * sa + d[1] * k) / oa, 0.f, 255.f);
    d[2] = (uint8_t)std::clamp((sb * sa + d[2] * k) / oa, 0.f, 255.f);
    d[3] = (uint8_t)std::clamp(oa * 255.f, 0.f, 255.f);
}

void Image::blend(int x, int y, Color c, float coverage) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    blendPx(at(x, y), c.r, c.g, c.b, c.a / 255.f * coverage);
}

void Image::draw(const Image& src, int dx, int dy, float alpha) {
    for (int y = 0; y < src.h; y++) {
        int ty = y + dy;
        if (ty < 0 || ty >= h) continue;
        for (int x = 0; x < src.w; x++) {
            int tx = x + dx;
            if (tx < 0 || tx >= w) continue;
            const uint8_t* s = src.at(x, y);
            if (!s[3]) continue;
            blendPx(at(tx, ty), s[0], s[1], s[2], s[3] / 255.f * alpha);
        }
    }
}

void Image::drawTinted(const Image& mask, int dx, int dy, Color tint) {
    float ta = tint.a / 255.f;
    for (int y = 0; y < mask.h; y++) {
        int ty = y + dy;
        if (ty < 0 || ty >= h) continue;
        for (int x = 0; x < mask.w; x++) {
            int tx = x + dx;
            if (tx < 0 || tx >= w) continue;
            uint8_t a = mask.at(x, y)[3];
            if (!a) continue;
            blendPx(at(tx, ty), tint.r, tint.g, tint.b, a / 255.f * ta);
        }
    }
}

void Image::drawRotated(const Image& mask, float cx, float cy, float ang, Color tint, float scale) {
    float c = std::cos(ang), s = std::sin(ang);
    float hw = mask.w * 0.5f * scale, hh = mask.h * 0.5f * scale;
    float rad = std::sqrt(hw * hw + hh * hh) + 2;
    int x0 = std::max(0, (int)(cx - rad)), x1 = std::min(w - 1, (int)(cx + rad));
    int y0 = std::max(0, (int)(cy - rad)), y1 = std::min(h - 1, (int)(cy + rad));
    float ta = tint.a / 255.f;
    auto sample = [&](float u, float v) -> float {
        // bilinear alpha sample, u/v in mask pixels
        u -= 0.5f; v -= 0.5f;
        int iu = (int)std::floor(u), iv = (int)std::floor(v);
        float fu = u - iu, fv = v - iv;
        auto a = [&](int x, int y) -> float {
            if (x < 0 || y < 0 || x >= mask.w || y >= mask.h) return 0.f;
            return mask.at(x, y)[3] / 255.f;
        };
        return lerp(lerp(a(iu, iv), a(iu + 1, iv), fu), lerp(a(iu, iv + 1), a(iu + 1, iv + 1), fu), fv);
    };
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            // inverse rotate into mask space
            float u = (dx * c + dy * s) / scale + mask.w * 0.5f;
            float v = (-dx * s + dy * c) / scale + mask.h * 0.5f;
            if (u < -1 || v < -1 || u > mask.w + 1 || v > mask.h + 1) continue;
            float a = sample(u, v);
            if (a > 0.f) blendPx(at(x, y), tint.r, tint.g, tint.b, a * ta);
        }
}

void Image::grain(float amount, uint32_t seed, float cell) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t* p = at(x, y);
            if (!p[3]) continue;
            float n = cell <= 1.f ? noise::hash2(x, y, seed) : noise::value(x / cell, y / cell, seed);
            float f = 1 + amount * (n * 2 - 1);
            p[0] = (uint8_t)std::clamp(p[0] * f, 0.f, 255.f);
            p[1] = (uint8_t)std::clamp(p[1] * f, 0.f, 255.f);
            p[2] = (uint8_t)std::clamp(p[2] * f, 0.f, 255.f);
        }
}

void Image::vignette(float strength, float inner) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float nx = (x + 0.5f) / w * 2 - 1, ny = (y + 0.5f) / h * 2 - 1;
            float d = std::sqrt(nx * nx + ny * ny) / 1.41421f;
            float t = clamp01((d - inner) / (1.0f - inner));
            t = t * t * (3 - 2 * t);
            float f = 1 - strength * t;
            uint8_t* p = at(x, y);
            p[0] = (uint8_t)(p[0] * f);
            p[1] = (uint8_t)(p[1] * f);
            p[2] = (uint8_t)(p[2] * f);
        }
}

// Three box passes per axis ~ gaussian. Works in premultiplied space.
Image Image::blurred(int radius) const {
    Image out = *this;
    if (radius <= 0 || empty()) return out;
    std::vector<float> buf((size_t)w * h * 4), tmp((size_t)w * h * 4);
    for (size_t i = 0; i < (size_t)w * h; i++) {
        float a = px[i * 4 + 3] / 255.f;
        buf[i * 4 + 0] = px[i * 4 + 0] * a;
        buf[i * 4 + 1] = px[i * 4 + 1] * a;
        buf[i * 4 + 2] = px[i * 4 + 2] * a;
        buf[i * 4 + 3] = px[i * 4 + 3];
    }
    int r = std::max(1, radius / 2);
    auto pass = [&](std::vector<float>& src, std::vector<float>& dst, bool horiz) {
        int len = horiz ? w : h, lines = horiz ? h : w;
        float inv = 1.f / (2 * r + 1);
        parallelFor(lines, [&](int lBegin, int lEnd) {
        for (int l = lBegin; l < lEnd; l++) {
            auto idx = [&](int i) -> size_t {
                i = std::clamp(i, 0, len - 1);
                return horiz ? ((size_t)l * w + i) * 4 : ((size_t)i * w + l) * 4;
            };
            float acc[4] = {0, 0, 0, 0};
            for (int i = -r; i <= r; i++)
                for (int c = 0; c < 4; c++) acc[c] += (i < 0 || i >= len) ? 0.f : src[idx(i) + c];
            for (int i = 0; i < len; i++) {
                size_t o = idx(i);
                for (int c = 0; c < 4; c++) dst[o + c] = acc[c] * inv;
                int add = i + r + 1, rem = i - r;
                for (int c = 0; c < 4; c++) {
                    if (add < len) acc[c] += src[idx(add) + c];
                    if (rem >= 0) acc[c] -= src[idx(rem) + c];
                }
            }
        }
        });
    };
    for (int k = 0; k < 3; k++) {
        pass(buf, tmp, true);
        pass(tmp, buf, false);
    }
    for (size_t i = 0; i < (size_t)w * h; i++) {
        float a = buf[i * 4 + 3];
        float inv = a > 0.5f ? 255.f / a : 0.f;
        out.px[i * 4 + 0] = (uint8_t)std::clamp(buf[i * 4 + 0] * inv, 0.f, 255.f);
        out.px[i * 4 + 1] = (uint8_t)std::clamp(buf[i * 4 + 1] * inv, 0.f, 255.f);
        out.px[i * 4 + 2] = (uint8_t)std::clamp(buf[i * 4 + 2] * inv, 0.f, 255.f);
        out.px[i * 4 + 3] = (uint8_t)std::clamp(a, 0.f, 255.f);
    }
    return out;
}

Image Image::shadow(int radius, float opacity) const {
    int pad = radius * 2;
    Image s(w + pad * 2, h + pad * 2);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t* d = s.at(x + pad, y + pad);
            d[3] = at(x, y)[3];
        }
    Image b = s.blurred(radius);
    for (size_t i = 0; i < (size_t)b.w * b.h; i++) {
        b.px[i * 4 + 0] = b.px[i * 4 + 1] = b.px[i * 4 + 2] = 0;
        b.px[i * 4 + 3] = (uint8_t)(b.px[i * 4 + 3] * opacity);
    }
    return b;
}

Image Image::flipped180() const {
    Image o(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) memcpy(o.at(w - 1 - x, h - 1 - y), at(x, y), 4);
    return o;
}

// ============================================================================
// Tex
// ============================================================================
Tex& Tex::operator=(Tex&& o) noexcept {
    if (this != &o) {
        if (tex) SDL_DestroyTexture(tex);
        tex = o.tex; pw = o.pw; ph = o.ph; w = o.w; h = o.h;
        o.tex = nullptr;
    }
    return *this;
}

Tex::~Tex() {
    if (tex) SDL_DestroyTexture(tex);
}

// ============================================================================
// gfx
// ============================================================================
namespace gfx {

namespace {

SDL_Renderer* R = nullptr;
NSVGrasterizer* g_rast = nullptr;
Tex g_soft;    // radial falloff, white
uint32_t g_frame = 0;

const char* kFontFiles[F_COUNT] = {
    "fonts/PlayfairDisplaySC-Bold.ttf", "fonts/PlayfairDisplay-Bold.ttf", "fonts/PlayfairDisplay-Regular.ttf",
    "fonts/Oswald-Medium.ttf", "fonts/PTSansNarrow-Regular.ttf", "fonts/PTSansNarrow-Bold.ttf",
};
std::vector<uint8_t> g_fontData[F_COUNT];
std::unordered_map<int, TTF_Font*> g_fonts; // key: font*4096 + px

struct CachedText {
    Tex tex;
    float ascentPx = 0;
    int pad = 0;
    uint32_t last = 0;
};
std::unordered_map<std::string, CachedText> g_text;

TTF_Font* font(FontId f, int px) {
    px = std::clamp(px, 4, 400);
    int key = f * 4096 + px;
    auto it = g_fonts.find(key);
    if (it != g_fonts.end()) return it->second;
    TTF_Font* tf = nullptr;
    if (!g_fontData[f].empty()) {
        SDL_RWops* rw = SDL_RWFromConstMem(g_fontData[f].data(), (int)g_fontData[f].size());
        tf = TTF_OpenFontRW(rw, 1, px);
        if (tf) TTF_SetFontHinting(tf, TTF_HINTING_LIGHT);
    }
    g_fonts[key] = tf;
    return tf;
}

Image surfaceToMask(SDL_Surface* s) {
    SDL_Surface* conv = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0);
    Image img(conv->w, conv->h);
    SDL_LockSurface(conv);
    for (int y = 0; y < conv->h; y++) {
        const uint8_t* row = (const uint8_t*)conv->pixels + y * conv->pitch;
        for (int x = 0; x < conv->w; x++) {
            uint8_t* d = img.at(x, y);
            d[0] = d[1] = d[2] = 255;
            d[3] = row[x * 4 + 3];
        }
    }
    SDL_UnlockSurface(conv);
    SDL_FreeSurface(conv);
    return img;
}

void applyGold(Image& img) {
    struct Stop { float t; Color c; };
    static const Stop stops[] = {
        {0.00f, Color::hex(0xfff3c2)}, {0.38f, Color::hex(0xf1cc68)}, {0.52f, Color::hex(0xb3842a)},
        {0.72f, Color::hex(0xd7ab45)}, {1.00f, Color::hex(0xf7e09a)},
    };
    for (int y = 0; y < img.h; y++) {
        float t = (y + 0.5f) / img.h;
        Color c = stops[4].c;
        for (int i = 0; i < 4; i++)
            if (t <= stops[i + 1].t) {
                c = lerpColor(stops[i].c, stops[i + 1].c, (t - stops[i].t) / (stops[i + 1].t - stops[i].t));
                break;
            }
        for (int x = 0; x < img.w; x++) {
            uint8_t* p = img.at(x, y);
            p[0] = c.r; p[1] = c.g; p[2] = c.b;
        }
    }
}

CachedText* cachedText(const std::string& s, FontId f, float size, TextStyle style, int wrapPx = 0) {
    if (s.empty()) return nullptr;
    int px = (int)std::lround(size * TEX_SCALE);
    std::string key;
    key.reserve(s.size() + 24);
    key += std::to_string(f * 4096 + px);
    key += '|';
    key += std::to_string(style);
    key += '|';
    key += std::to_string(wrapPx);
    key += '|';
    key += s;
    auto it = g_text.find(key);
    if (it != g_text.end()) {
        it->second.last = g_frame;
        return &it->second;
    }
    TTF_Font* tf = font(f, px);
    if (!tf) return nullptr;
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surf = wrapPx > 0 ? TTF_RenderUTF8_Blended_Wrapped(tf, s.c_str(), white, wrapPx)
                                   : TTF_RenderUTF8_Blended(tf, s.c_str(), white);
    if (!surf) return nullptr;
    Image mask = surfaceToMask(surf);
    SDL_FreeSurface(surf);
    CachedText ct;
    ct.ascentPx = (float)TTF_FontAscent(tf);
    if (style == TS_GOLD) {
        applyGold(mask);
    } else if (style == TS_GLOW) {
        int r = std::max(2, px / 7);
        int pad = r * 2;
        Image padded(mask.w + pad * 2, mask.h + pad * 2);
        padded.draw(mask, pad, pad);
        mask = padded.blurred(r);
        ct.pad = pad;
    }
    ct.tex = upload(mask, TEX_SCALE);
    ct.last = g_frame;
    auto res = g_text.emplace(std::move(key), std::move(ct));
    return &res.first->second;
}

std::vector<SDL_Vertex> g_v;
std::vector<int> g_i;

inline SDL_Vertex vtx(float x, float y, Color c) {
    SDL_Vertex v;
    v.position = {x, y};
    v.color = {c.r, c.g, c.b, c.a};
    v.tex_coord = {0, 0};
    return v;
}

void flushGeom(bool additive) {
    if (g_v.empty()) return;
    SDL_SetRenderDrawBlendMode(R, additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(R, nullptr, g_v.data(), (int)g_v.size(), g_i.empty() ? nullptr : g_i.data(),
                       (int)g_i.size());
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    g_v.clear();
    g_i.clear();
}

constexpr float FEATHER = 0.9f; // logical px of soft edge for anti-aliasing

// Fills a convex polygon (points in order) with a feathered rim.
void convexFill(const std::vector<SDL_FPoint>& rawPts, Color c, bool additive = false) {
    // drop coincident neighbours: degenerate fan triangles leave seams on some rasterizers
    std::vector<SDL_FPoint> pts;
    pts.reserve(rawPts.size());
    for (auto& p : rawPts)
        if (pts.empty() || std::fabs(p.x - pts.back().x) + std::fabs(p.y - pts.back().y) > 0.01f) pts.push_back(p);
    while (pts.size() > 1 && std::fabs(pts.front().x - pts.back().x) + std::fabs(pts.front().y - pts.back().y) <= 0.01f)
        pts.pop_back();
    int n = (int)pts.size();
    if (n < 3) return;
    float cx = 0, cy = 0;
    for (auto& p : pts) { cx += p.x; cy += p.y; }
    cx /= n; cy /= n;
    Color edge = c.withA(0);
    int base = (int)g_v.size();
    g_v.push_back(vtx(cx, cy, c));
    for (int i = 0; i < n; i++) {
        const SDL_FPoint& p = pts[i];
        const SDL_FPoint& a = pts[(i + n - 1) % n];
        const SDL_FPoint& b = pts[(i + 1) % n];
        // outward normal: average of the two edge normals
        float e1x = p.x - a.x, e1y = p.y - a.y, e2x = b.x - p.x, e2y = b.y - p.y;
        float n1x = e1y, n1y = -e1x, n2x = e2y, n2y = -e2x;
        float l1 = std::sqrt(n1x * n1x + n1y * n1y), l2 = std::sqrt(n2x * n2x + n2y * n2y);
        if (l1 > 0) { n1x /= l1; n1y /= l1; }
        if (l2 > 0) { n2x /= l2; n2y /= l2; }
        float nx = n1x + n2x, ny = n1y + n2y;
        float nl = std::sqrt(nx * nx + ny * ny);
        if (nl > 0) { nx /= nl; ny /= nl; }
        // orientation check: ensure normal points away from centroid
        if ((p.x - cx) * nx + (p.y - cy) * ny < 0) { nx = -nx; ny = -ny; }
        float h = FEATHER * 0.5f;
        g_v.push_back(vtx(p.x - nx * h, p.y - ny * h, c));
        g_v.push_back(vtx(p.x + nx * h, p.y + ny * h, edge));
    }
    for (int i = 0; i < n; i++) {
        int in0 = base + 1 + i * 2, out0 = in0 + 1;
        int in1 = base + 1 + ((i + 1) % n) * 2, out1 = in1 + 1;
        g_i.insert(g_i.end(), {base, in0, in1});
        g_i.insert(g_i.end(), {in0, out0, out1, in0, out1, in1});
    }
    flushGeom(additive);
}

void arcPoints(std::vector<SDL_FPoint>& out, float cx, float cy, float rx, float ry, float a0, float a1, int segs) {
    for (int i = 0; i <= segs; i++) {
        float a = a0 + (a1 - a0) * i / segs;
        out.push_back({cx + std::cos(a) * rx, cy + std::sin(a) * ry});
    }
}

std::vector<SDL_FPoint> roundRectPts(float x, float y, float w, float h, float r) {
    r = std::max(0.f, std::min(r, std::min(w, h) * 0.5f));
    std::vector<SDL_FPoint> pts;
    int segs = std::max(2, (int)(r * 0.6f));
    if (r <= 0.01f) return {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    arcPoints(pts, x + w - r, y + r, r, r, -PI / 2, 0, segs);
    arcPoints(pts, x + w - r, y + h - r, r, r, 0, PI / 2, segs);
    arcPoints(pts, x + r, y + h - r, r, r, PI / 2, PI, segs);
    arcPoints(pts, x + r, y + r, r, r, PI, PI * 1.5f, segs);
    return pts;
}

} // namespace

bool init(SDL_Renderer* r) {
    R = r;
    if (TTF_Init() != 0) {
        SDL_Log("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    for (int i = 0; i < F_COUNT; i++) {
        if (!platform::readFile(platform::assetPath(kFontFiles[i]), g_fontData[i]))
            SDL_Log("font missing: %s", kFontFiles[i]);
    }
    g_rast = nsvgCreateRasterizer();
    // Soft radial sprite used for glows, lights, shadows.
    int S = 192;
    Image soft(S, S);
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float dx = (x + 0.5f) / S * 2 - 1, dy = (y + 0.5f) / S * 2 - 1;
            float d = std::sqrt(dx * dx + dy * dy);
            float a = clamp01(1 - d);
            a = a * a * (3 - 2 * a);
            a *= a;
            uint8_t* p = soft.at(x, y);
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(a * 255);
        }
    g_soft = upload(soft, 1.f);
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    return true;
}

void shutdown() {
    g_text.clear();
    g_soft = Tex();
    for (auto& kv : g_fonts)
        if (kv.second) TTF_CloseFont(kv.second);
    g_fonts.clear();
    if (g_rast) nsvgDeleteRasterizer(g_rast);
    g_rast = nullptr;
    TTF_Quit();
}

SDL_Renderer* renderer() { return R; }

void frameTick() {
    g_frame++;
    if (g_frame % 120 == 0) {
        for (auto it = g_text.begin(); it != g_text.end();) {
            if (g_frame - it->second.last > 600) it = g_text.erase(it);
            else ++it;
        }
    }
}

// ---------------------------------------------------------------------------
Image rasterSvg(const std::string& svg, float scale) {
    std::vector<char> buf(svg.begin(), svg.end());
    buf.push_back(0);
    NSVGimage* img = nsvgParse(buf.data(), "px", 96.f);
    if (!img) return Image();
    int w = (int)std::ceil(img->width * scale), h = (int)std::ceil(img->height * scale);
    Image out(w, h);
    if (w > 0 && h > 0) nsvgRasterize(g_rast, img, 0, 0, scale, out.px.data(), w, h, w * 4);
    nsvgDelete(img);
    return out;
}

Tex upload(const Image& img, float scale) {
    Tex t;
    if (img.empty()) return t;
    t.tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, img.w, img.h);
    if (!t.tex) {
        SDL_Log("CreateTexture %dx%d failed: %s", img.w, img.h, SDL_GetError());
        return t;
    }
    SDL_UpdateTexture(t.tex, nullptr, img.px.data(), img.w * 4);
    SDL_SetTextureBlendMode(t.tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t.tex, SDL_ScaleModeLinear);
    t.pw = img.w;
    t.ph = img.h;
    t.w = img.w / scale;
    t.h = img.h / scale;
    return t;
}

Tex svgTex(const std::string& svg, float scale) { return upload(rasterSvg(svg, scale), scale); }

// ---------------------------------------------------------------------------
static void prep(const Tex& t, float alpha, Color tint, bool additive) {
    SDL_SetTextureColorMod(t.tex, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(t.tex, (uint8_t)std::clamp(alpha * tint.a, 0.f, 255.f));
    SDL_SetTextureBlendMode(t.tex, additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
}

void draw(const Tex& t, float x, float y, float alpha, Color tint) {
    drawScaled(t, x, y, t.w, t.h, alpha, tint);
}

void drawScaled(const Tex& t, float x, float y, float w, float h, float alpha, Color tint, bool additive) {
    if (!t || alpha <= 0.f) return;
    prep(t, alpha, tint, additive);
    SDL_FRect d = {x, y, w, h};
    SDL_RenderCopyF(R, t.tex, nullptr, &d);
}

void drawCentered(const Tex& t, float cx, float cy, float scale, float angleDeg, float alpha, Color tint,
                  bool additive) {
    drawCenteredXY(t, cx, cy, scale, scale, angleDeg, alpha, tint, additive);
}

void drawCenteredXY(const Tex& t, float cx, float cy, float sx, float sy, float angleDeg, float alpha, Color tint,
                    bool additive) {
    if (!t || alpha <= 0.f) return;
    prep(t, alpha, tint, additive);
    float w = t.w * std::fabs(sx), h = t.h * std::fabs(sy);
    SDL_FRect d = {cx - w / 2, cy - h / 2, w, h};
    int flip = SDL_FLIP_NONE;
    if (sx < 0) flip |= SDL_FLIP_HORIZONTAL;
    if (sy < 0) flip |= SDL_FLIP_VERTICAL;
    if (angleDeg == 0.f && flip == SDL_FLIP_NONE) SDL_RenderCopyF(R, t.tex, nullptr, &d);
    else SDL_RenderCopyExF(R, t.tex, nullptr, &d, angleDeg, nullptr, (SDL_RendererFlip)flip);
}

void drawPart(const Tex& t, float sx, float sy, float sw, float sh, float x, float y, float w, float h, float alpha) {
    if (!t || alpha <= 0.f) return;
    prep(t, alpha, pal::white, false);
    float k = t.pw / t.w;
    SDL_Rect s = {(int)(sx * k), (int)(sy * k), (int)(sw * k), (int)(sh * k)};
    SDL_FRect d = {x, y, w, h};
    SDL_RenderCopyF(R, t.tex, &s, &d);
}

void nine(const Tex& t, float x, float y, float w, float h, float b, float alpha, Color tint) {
    if (!t || alpha <= 0.f) return;
    prep(t, alpha, tint, false);
    float k = t.pw / t.w;
    float tw = t.w, th = t.h;
    b = std::min(b, std::min(w, h) / 2);
    float sxs[4] = {0, b, tw - b, tw}, sys[4] = {0, b, th - b, th};
    float dxs[4] = {x, x + b, x + w - b, x + w}, dys[4] = {y, y + b, y + h - b, y + h};
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) {
            SDL_Rect src = {(int)std::lround(sxs[i] * k), (int)std::lround(sys[j] * k), (int)std::lround((sxs[i + 1] - sxs[i]) * k),
                            (int)std::lround((sys[j + 1] - sys[j]) * k)};
            SDL_FRect dst = {dxs[i], dys[j], dxs[i + 1] - dxs[i], dys[j + 1] - dys[j]};
            if (src.w <= 0 || src.h <= 0 || dst.w <= 0 || dst.h <= 0) continue;
            SDL_RenderCopyF(R, t.tex, &src, &dst);
        }
}

void rect(float x, float y, float w, float h, Color c) { rectGrad(x, y, w, h, c, c); }

void rectGrad(float x, float y, float w, float h, Color top, Color bottom) {
    int b = (int)g_v.size();
    g_v.push_back(vtx(x, y, top));
    g_v.push_back(vtx(x + w, y, top));
    g_v.push_back(vtx(x + w, y + h, bottom));
    g_v.push_back(vtx(x, y + h, bottom));
    g_i.insert(g_i.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    flushGeom(false);
}

void rectGradH(float x, float y, float w, float h, Color left, Color right) {
    int b = (int)g_v.size();
    g_v.push_back(vtx(x, y, left));
    g_v.push_back(vtx(x + w, y, right));
    g_v.push_back(vtx(x + w, y + h, right));
    g_v.push_back(vtx(x, y + h, left));
    g_i.insert(g_i.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    flushGeom(false);
}

void roundRect(float x, float y, float w, float h, float r, Color c) { convexFill(roundRectPts(x, y, w, h, r), c); }

void roundRectOutline(float x, float y, float w, float h, float r, float thick, Color c) {
    auto outer = roundRectPts(x, y, w, h, r);
    auto inner = roundRectPts(x + thick, y + thick, w - thick * 2, h - thick * 2, std::max(0.f, r - thick));
    // both lists have the same structure when r > thick; fall back to rect strips otherwise
    if (outer.size() != inner.size()) {
        rect(x, y, w, thick, c);
        rect(x, y + h - thick, w, thick, c);
        rect(x, y + thick, thick, h - thick * 2, c);
        rect(x + w - thick, y + thick, thick, h - thick * 2, c);
        return;
    }
    int n = (int)outer.size();
    int base = (int)g_v.size();
    for (int i = 0; i < n; i++) {
        g_v.push_back(vtx(outer[i].x, outer[i].y, c));
        g_v.push_back(vtx(inner[i].x, inner[i].y, c));
    }
    for (int i = 0; i < n; i++) {
        int a = base + i * 2, b = base + ((i + 1) % n) * 2;
        g_i.insert(g_i.end(), {a, a + 1, b + 1, a, b + 1, b});
    }
    flushGeom(false);
}

void line(float x1, float y1, float x2, float y2, float thick, Color c, bool additive) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len, ny = dx / len;
    float h = thick * 0.5f, f = FEATHER;
    Color e = c.withA(0);
    int b = (int)g_v.size();
    float offs[4] = {-(h + f), -h, h, h + f};
    Color cols[4] = {e, c, c, e};
    for (int i = 0; i < 4; i++) {
        g_v.push_back(vtx(x1 + nx * offs[i], y1 + ny * offs[i], cols[i]));
        g_v.push_back(vtx(x2 + nx * offs[i], y2 + ny * offs[i], cols[i]));
    }
    for (int i = 0; i < 3; i++) {
        int a = b + i * 2;
        g_i.insert(g_i.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
    }
    flushGeom(additive);
}

void circle(float cx, float cy, float r, Color c) { ellipse(cx, cy, r, r, c); }

void ellipse(float cx, float cy, float rx, float ry, Color c) {
    std::vector<SDL_FPoint> pts;
    int segs = std::clamp((int)(std::max(rx, ry) * 0.9f), 14, 120);
    for (int i = 0; i < segs; i++) {
        float a = i * 2 * PI / segs;
        pts.push_back({cx + std::cos(a) * rx, cy + std::sin(a) * ry});
    }
    convexFill(pts, c);
}

void ring(float cx, float cy, float r, float thick, Color c) {
    int segs = std::clamp((int)(r * 0.9f), 16, 140);
    float ro = r, ri = r - thick, f = FEATHER;
    Color e = c.withA(0);
    float radii[4] = {ri - f, ri, ro, ro + f};
    Color cols[4] = {e, c, c, e};
    int base = (int)g_v.size();
    for (int i = 0; i < segs; i++) {
        float a = i * 2 * PI / segs;
        float ca = std::cos(a), sa = std::sin(a);
        for (int k = 0; k < 4; k++) g_v.push_back(vtx(cx + ca * radii[k], cy + sa * radii[k], cols[k]));
    }
    for (int i = 0; i < segs; i++) {
        int a = base + i * 4, b = base + ((i + 1) % segs) * 4;
        for (int k = 0; k < 3; k++) g_i.insert(g_i.end(), {a + k, a + k + 1, b + k + 1, a + k, b + k + 1, b + k});
    }
    flushGeom(false);
}

void glow(float cx, float cy, float radius, Color c, float alpha, bool additive) {
    glowEllipse(cx, cy, radius, radius, c, alpha, additive);
}

void glowEllipse(float cx, float cy, float rx, float ry, Color c, float alpha, bool additive) {
    if (alpha <= 0.f) return;
    prep(g_soft, alpha, c, additive);
    SDL_FRect d = {cx - rx, cy - ry, rx * 2, ry * 2};
    SDL_RenderCopyF(R, g_soft.tex, nullptr, &d);
}

void triangle(SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, Color ca, Color cb, Color cc, bool additive) {
    int base = (int)g_v.size();
    g_v.push_back(vtx(a.x, a.y, ca));
    g_v.push_back(vtx(b.x, b.y, cb));
    g_v.push_back(vtx(c.x, c.y, cc));
    g_i.insert(g_i.end(), {base, base + 1, base + 2});
    flushGeom(additive);
}

void quad(SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_FPoint d, Color ca, Color cb, Color cc, Color cd,
          bool additive) {
    int base = (int)g_v.size();
    g_v.push_back(vtx(a.x, a.y, ca));
    g_v.push_back(vtx(b.x, b.y, cb));
    g_v.push_back(vtx(c.x, c.y, cc));
    g_v.push_back(vtx(d.x, d.y, cd));
    g_i.insert(g_i.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    flushGeom(additive);
}

void clip(float x, float y, float w, float h) {
    SDL_Rect r = {(int)std::floor(x), (int)std::floor(y), (int)std::ceil(w), (int)std::ceil(h)};
    SDL_RenderSetClipRect(R, &r);
}

void unclip() { SDL_RenderSetClipRect(R, nullptr); }

void dim(float alpha, Color c) {
    if (alpha <= 0) return;
    rect(0, 0, SCREEN_W, SCREEN_H, c.alpha(alpha));
}

// ---------------------------------------------------------------------------
static void drawCachedText(CachedText* ct, float x, float y, float size, int align, float alpha, Color c,
                           bool additive = false) {
    if (!ct) return;
    float w = ct->tex.w, h = ct->tex.h;
    float padL = ct->pad / TEX_SCALE;
    float px = x;
    float contentW = w - padL * 2;
    if (align == 0) px = x - contentW / 2;
    else if (align > 0) px = x - contentW;
    // vertical: centre of lowercase/caps sits ~0.32em above the baseline
    float top = y - (ct->ascentPx / TEX_SCALE - size * 0.33f);
    drawScaled(ct->tex, px - padL, top - padL, w, h, alpha, c, additive);
}

void text(const std::string& s, float x, float y, FontId f, float size, Color c, int align, float alpha,
          TextStyle style) {
    drawCachedText(cachedText(s, f, size, style), x, y, size, align, alpha, style == TS_GOLD ? pal::white : c);
}

void textShadow(const std::string& s, float x, float y, FontId f, float size, Color c, int align, float alpha) {
    CachedText* sh = cachedText(s, f, size, TS_GLOW);
    drawCachedText(sh, x, y + size * 0.06f + 1, size, align, alpha * 0.85f, pal::black);
    text(s, x, y, f, size, c, align, alpha);
}

void textGold(const std::string& s, float x, float y, FontId f, float size, int align, float alpha, bool shadow) {
    if (shadow) {
        CachedText* sh = cachedText(s, f, size, TS_GLOW);
        drawCachedText(sh, x, y + size * 0.05f + 1, size, align, alpha * 0.9f, pal::black);
    }
    text(s, x, y, f, size, pal::white, align, alpha, TS_GOLD);
}

void textGlow(const std::string& s, float x, float y, FontId f, float size, Color glowColor, float glowAlpha,
              int align) {
    CachedText* g = cachedText(s, f, size, TS_GLOW);
    drawCachedText(g, x, y, size, align, glowAlpha, glowColor, true);
}

float textWrapped(const std::string& s, float x, float y, float wrapW, FontId f, float size, Color c,
                  float lineGap, float alpha) {
    (void)lineGap;
    int wrapPx = (int)(wrapW * TEX_SCALE);
    CachedText* ct = cachedText(s, f, size, TS_PLAIN, wrapPx);
    if (!ct) return 0;
    drawScaled(ct->tex, x, y, ct->tex.w, ct->tex.h, alpha, c);
    return ct->tex.h;
}

float textWidth(const std::string& s, FontId f, float size) {
    if (s.empty()) return 0;
    int px = (int)std::lround(size * TEX_SCALE);
    TTF_Font* tf = font(f, px);
    if (!tf) return 0;
    int w = 0, h = 0;
    TTF_SizeUTF8(tf, s.c_str(), &w, &h);
    return w / TEX_SCALE;
}

Image textMask(const std::string& s, FontId f, float size, float scale) {
    int px = (int)std::lround(size * scale);
    TTF_Font* tf = font(f, px);
    if (!tf || s.empty()) return Image();
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(tf, s.c_str(), white);
    if (!surf) return Image();
    Image m = surfaceToMask(surf);
    SDL_FreeSurface(surf);
    return m;
}

} // namespace gfx
