#include "shade.h"

#include <cstring>

#include "../core/parallel.h"

namespace shade {

namespace {

const float INF = 1e20f;

void dt1d(const float* f, int n, float* d, int* v, float* z) {
    int k = 0;
    v[0] = 0;
    z[0] = -INF;
    z[1] = INF;
    for (int q = 1; q < n; q++) {
        float s = ((f[q] + (float)q * q) - (f[v[k]] + (float)v[k] * v[k])) / (2.f * q - 2.f * v[k]);
        while (s <= z[k]) {
            k--;
            s = ((f[q] + (float)q * q) - (f[v[k]] + (float)v[k] * v[k])) / (2.f * q - 2.f * v[k]);
        }
        k++;
        v[k] = q;
        z[k] = s;
        z[k + 1] = INF;
    }
    k = 0;
    for (int q = 0; q < n; q++) {
        while (z[k + 1] < q) k++;
        float dq = (float)(q - v[k]);
        d[q] = dq * dq + f[v[k]];
    }
}

inline float softClip(float x) {
    if (x < 0.82f) return std::max(0.f, x);
    return 0.82f + 0.18f * (1 - std::exp(-(x - 0.82f) / 0.18f));
}

inline Vec3 lerp3(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

const Vec3 KEY = Vec3(-0.5f, -0.72f, 0.62f).norm();

Vec3 shadePixel(const Vec3& A, const Vec3& N, const Material& m) {
    const Vec3 V(0, 0, 1);
    float ndl = std::max(0.f, N.dot(KEY));
    Vec3 H = (KEY + V).norm();
    float ndh = std::max(0.f, N.dot(H));
    Vec3 R(2 * N.z * N.x, 2 * N.z * N.y, 2 * N.z * N.z - 1);
    Vec3 E = environment(R);
    float F = std::pow(1 - std::max(0.f, N.z), 5.f);
    float spec = std::pow(ndh, m.shininess) * m.spec;
    Vec3 diel = A * (m.ambient + m.diffuse * ndl) + E * (m.reflect + F * 0.3f) + Vec3(spec, spec, spec);
    Vec3 metal = A * E * 1.1f + A * (0.12f * ndl) + A * (spec * 1.6f) + E * (F * 0.25f);
    Vec3 c = lerp3(diel, metal, m.metal);
    if (m.clearcoat > 0) {
        float s2 = std::pow(ndh, 220.f) * 1.4f;
        c = c + E * ((0.03f + F * 0.5f) * m.clearcoat) + Vec3(s2, s2, s2) * m.clearcoat;
    }
    if (m.rim > 0) {
        float r = std::pow(1 - std::max(0.f, N.z), 2.5f) * m.rim;
        c = c + Vec3(r * 0.9f, r * 0.85f, r * 0.8f);
    }
    return c * m.exposure;
}

void store(uint8_t* p, const Vec3& c) {
    p[0] = (uint8_t)(softClip(c.x) * 255.f);
    p[1] = (uint8_t)(softClip(c.y) * 255.f);
    p[2] = (uint8_t)(softClip(c.z) * 255.f);
}

} // namespace

Vec3 environment(const Vec3& d) {
    float up = -d.y;
    Vec3 floorC(0.05f, 0.04f, 0.035f), horizon(0.34f, 0.29f, 0.24f), ceil(0.95f, 0.82f, 0.64f);
    Vec3 c = up < 0 ? lerp3(horizon, floorC, std::min(1.f, -up * 1.7f)) : lerp3(horizon, ceil, std::pow(up, 0.75f));
    // the room behind the camera is lit too: flat surfaces facing us must not go black
    float fwd = std::max(0.f, d.z);
    c = c + Vec3(0.42f, 0.36f, 0.28f) * (fwd * fwd);
    auto box = [&](Vec3 dir, Vec3 col, float tight) {
        float a = d.dot(dir.norm());
        if (a > 0) c = c + col * std::pow(a, tight);
    };
    box({-0.45f, -0.75f, 0.5f}, {3.4f, 3.0f, 2.4f}, 70);
    box({0.75f, -0.3f, 0.45f}, {1.0f, 1.15f, 1.45f}, 45);
    box({0.f, -1.f, 0.1f}, {1.4f, 1.25f, 1.0f}, 16);
    box({-0.9f, 0.2f, 0.3f}, {0.35f, 0.3f, 0.25f}, 8);
    return c;
}

namespace mat {
Material clay() { Material m; m.ambient = 0.38f; m.diffuse = 0.7f; m.spec = 0.18f; m.shininess = 22; m.reflect = 0.05f; return m; }
Material gloss() { Material m; m.spec = 0.7f; m.shininess = 80; m.reflect = 0.12f; m.clearcoat = 0.6f; return m; }
Material lacquer() { Material m; m.ambient = 0.3f; m.spec = 0.6f; m.shininess = 70; m.reflect = 0.1f; m.clearcoat = 0.8f; return m; }
Material leather() { Material m; m.ambient = 0.3f; m.diffuse = 0.8f; m.spec = 0.28f; m.shininess = 16; m.reflect = 0.06f; m.rim = 0.15f; return m; }
Material felt() { Material m; m.ambient = 0.45f; m.diffuse = 0.6f; m.spec = 0.02f; m.shininess = 4; m.reflect = 0.f; return m; }
Material paper() { Material m; m.ambient = 0.55f; m.diffuse = 0.5f; m.spec = 0.12f; m.shininess = 18; m.reflect = 0.04f; return m; }
Material gold() { Material m; m.metal = 1; m.spec = 0.9f; m.shininess = 60; m.clearcoat = 0.2f; return m; }
Material silver() { Material m; m.metal = 1; m.spec = 1.f; m.shininess = 90; m.clearcoat = 0.3f; return m; }
Material ivory() { Material m; m.ambient = 0.42f; m.diffuse = 0.65f; m.spec = 0.45f; m.shininess = 50; m.reflect = 0.08f; m.clearcoat = 0.3f; return m; }
} // namespace mat

std::vector<float> insideDistance(const Image& mask, int threshold) {
    int w = mask.w, h = mask.h;
    std::vector<float> g((size_t)w * h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            bool in = mask.at(x, y)[3] >= threshold && x > 0 && y > 0 && x < w - 1 && y < h - 1;
            g[(size_t)y * w + x] = in ? INF : 0.f;
        }
    int n = std::max(w, h);
    // columns
    parallelFor(w, [&](int x0, int x1) {
        std::vector<float> f(n), d(n), z(n + 1);
        std::vector<int> v(n);
        for (int x = x0; x < x1; x++) {
            for (int y = 0; y < h; y++) f[y] = g[(size_t)y * w + x];
            dt1d(f.data(), h, d.data(), v.data(), z.data());
            for (int y = 0; y < h; y++) g[(size_t)y * w + x] = d[y];
        }
    });
    parallelFor(h, [&](int y0, int y1) {
        std::vector<float> f(n), d(n), z(n + 1);
        std::vector<int> v(n);
        for (int y = y0; y < y1; y++) {
            for (int x = 0; x < w; x++) f[x] = g[(size_t)y * w + x];
            dt1d(f.data(), w, d.data(), v.data(), z.data());
            for (int x = 0; x < w; x++) g[(size_t)y * w + x] = std::sqrt(d[x]);
        }
    });
    // sub-pixel correction from anti-aliased alpha
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float& v = g[(size_t)y * w + x];
            uint8_t a = mask.at(x, y)[3];
            if (v > 0) v = std::max(0.f, v - 0.5f + a / 510.f);
        }
    return g;
}

void shadeHeight(Image& img, const std::vector<float>& height, float depthPx, const Material& m) {
    int w = img.w, h = img.h;
    parallelFor(h, [&](int y0, int y1) {
        for (int y = y0; y < y1; y++)
            for (int x = 0; x < w; x++) {
                uint8_t* p = img.at(x, y);
                if (!p[3]) continue;
                auto H = [&](int xx, int yy) {
                    xx = std::clamp(xx, 0, w - 1);
                    yy = std::clamp(yy, 0, h - 1);
                    return height[(size_t)yy * w + xx];
                };
                float gx = (H(x + 1, y) - H(x - 1, y)) * 0.5f * depthPx;
                float gy = (H(x, y + 1) - H(x, y - 1)) * 0.5f * depthPx;
                Vec3 N = Vec3(-gx, -gy, 1).norm();
                Vec3 A(p[0] / 255.f, p[1] / 255.f, p[2] / 255.f);
                store(p, shadePixel(A, N, m));
            }
    });
}

void bevel(Image& img, float bevelPx, float depth, const Material& m, float profile) {
    auto dist = insideDistance(img);
    std::vector<float> hgt(dist.size());
    float bw = std::max(0.5f, bevelPx);
    for (size_t i = 0; i < dist.size(); i++) {
        float t = clamp01(dist[i] / bw);
        float round = std::sqrt(std::max(0.f, 1 - (1 - t) * (1 - t)));
        hgt[i] = lerp(t, round, profile);
    }
    shadeHeight(img, hgt, bw * depth, m);
}

Image relief(float w, float h, const std::vector<Layer>& layers, float scale) {
    Image out((int)std::ceil(w * scale), (int)std::ceil(h * scale));
    uint32_t seed = 17;
    for (const Layer& L : layers) {
        Image img = gfx::rasterSvg(L.svg, scale);
        if (img.w != out.w || img.h != out.h) {
            Image fit(out.w, out.h);
            fit.draw(img, 0, 0);
            img = std::move(fit);
        }
        if (L.grain > 0) img.grain(L.grain, seed++, L.grainCell);
        if (L.paint) L.paint(img);
        if (!L.flat) bevel(img, L.bevel * scale, L.depth, L.mat, L.profile);
        if (L.shadow > 0) {
            int r = (int)(L.shadow * scale);
            Image sh = img.shadow(r, L.shadowOpacity);
            out.draw(sh, -r * 2 + (int)(1.5f * scale), -r * 2 + (int)(2.5f * scale));
        }
        out.draw(img, 0, 0);
    }
    return out;
}

// ---------------------------------------------------------------------------
Image gem(float w, float h, const std::vector<SDL_FPoint>& girdle, SDL_FPoint centre, float table, Color body,
          float scale, float crownHeight) {
    int W = (int)std::ceil(w * scale), Hh = (int)std::ceil(h * scale);
    Image out(W, Hh);
    int n = (int)girdle.size();
    float radius = 0;
    for (auto& g : girdle) radius += std::hypot(g.x - centre.x, g.y - centre.y);
    radius /= n;
    float zt = crownHeight * radius;
    struct P3 { float x, y, z; };
    std::vector<P3> G(n), T(n);
    for (int i = 0; i < n; i++) {
        G[i] = {girdle[i].x, girdle[i].y, 0};
        const SDL_FPoint& a = girdle[i];
        const SDL_FPoint& b = girdle[(i + 1) % n];
        float mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2;
        T[i] = {centre.x + (mx - centre.x) * table, centre.y + (my - centre.y) * table, zt};
    }
    struct Tri { P3 a, b, c; Vec3 n; };
    std::vector<Tri> tris;
    auto addTri = [&](P3 a, P3 b, P3 c) {
        Vec3 u(b.x - a.x, b.y - a.y, b.z - a.z), v(c.x - a.x, c.y - a.y, c.z - a.z);
        Vec3 nn(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x);
        nn = nn.norm();
        if (nn.z < 0) nn = nn * -1.f;
        // facets are viewed from above: screen y grows downward, flip y of the normal
        tris.push_back({a, b, c, nn});
    };
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n, pi = (i + n - 1) % n;
        addTri(G[i], G[j], T[i]);
        addTri(T[pi], T[i], G[i]);
    }
    auto inTri = [](float px, float py, const P3& a, const P3& b, const P3& c) {
        float d1 = (px - b.x) * (a.y - b.y) - (a.x - b.x) * (py - b.y);
        float d2 = (px - c.x) * (b.y - c.y) - (b.x - c.x) * (py - c.y);
        float d3 = (px - a.x) * (c.y - a.y) - (c.x - a.x) * (py - a.y);
        bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0), pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        return !(neg && pos);
    };
    auto inPoly = [&](float px, float py, const std::vector<P3>& poly) {
        bool c = false;
        for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
            if (((poly[i].y > py) != (poly[j].y > py)) &&
                (px < (poly[j].x - poly[i].x) * (py - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x))
                c = !c;
        }
        return c;
    };
    Vec3 B = rgb(body);
    const Vec3 V(0, 0, 1);
    auto facetColour = [&](const Vec3& N, int id) -> Vec3 {
        Vec3 R(2 * N.z * N.x, 2 * N.z * N.y, 2 * N.z * N.z - 1);
        Vec3 E = environment(R);
        // light bouncing inside the stone: mirror the normal for a second look-up
        Vec3 R2(-R.x * 1.3f, -R.y * 1.3f, R.z);
        Vec3 E2 = environment(R2.norm());
        float lum = (E2.x + E2.y + E2.z) / 3.f;
        float hv = noise::hash2(id, 3, 99);
        // facets alternate between dark body colour and bright "fire"
        float fire = 0.05f + 0.55f * std::min(lum, 1.5f) + 1.4f * hv * hv * hv;
        Vec3 inner = B * fire + Vec3(1, 1, 1) * (0.25f * std::max(0.f, hv - 0.82f) * 5.f);
        float F = 0.05f + 0.95f * std::pow(1 - N.z, 5.f);
        Vec3 Hh2 = (KEY + V).norm();
        float spec = std::pow(std::max(0.f, N.dot(Hh2)), 200.f) * 3.f;
        return inner * (1 - F) + E * (F * 1.1f + 0.12f) + Vec3(spec, spec, spec);
    };
    parallelFor(Hh, [&](int y0, int y1) {
        for (int py = y0; py < y1; py++)
            for (int px = 0; px < W; px++) {
                Vec3 acc;
                int cover = 0, firstId = -2;
                bool edge = false;
                for (int sy = 0; sy < 3; sy++)
                    for (int sx = 0; sx < 3; sx++) {
                        float x = (px + (sx + 0.5f) / 3.f) / scale, y = (py + (sy + 0.5f) / 3.f) / scale;
                        int id = -1;
                        Vec3 N;
                        if (inPoly(x, y, T)) {
                            // pavilion facets seen through the table: a star of sloped sectors
                            float ang = std::atan2(y - centre.y, x - centre.x) + PI;
                            int sector = (int)(ang / (2 * PI) * (n * 2)) % (n * 2);
                            float mid = (sector + 0.5f) * PI / n - PI;
                            float rr = std::hypot(x - centre.x, y - centre.y) / (radius * table + 1e-3f);
                            float tilt = (sector % 2 ? 0.55f : 0.35f) * (1.1f - rr * 0.4f);
                            N = Vec3(-std::cos(mid) * tilt, -std::sin(mid) * tilt, 1).norm();
                            id = 1000 + sector;
                        }
                        else
                            for (size_t t = 0; t < tris.size(); t++)
                                if (inTri(x, y, tris[t].a, tris[t].b, tris[t].c)) {
                                    id = (int)t;
                                    N = tris[t].n;
                                    break;
                                }
                        if (id < 0) continue;
                        if (firstId == -2) firstId = id;
                        else if (id != firstId) edge = true;
                        acc = acc + facetColour(N, id);
                        cover++;
                    }
                if (!cover) continue;
                Vec3 c = acc * (1.f / cover);
                if (edge) c = c * 0.78f;
                uint8_t* p = out.at(px, py);
                store(p, c);
                p[3] = (uint8_t)(255 * cover / 9);
            }
    });
    // dark girdle line where coverage is partial
    for (int y = 0; y < out.h; y++)
        for (int x = 0; x < out.w; x++) {
            uint8_t* p = out.at(x, y);
            if (p[3] > 0 && p[3] < 250) { p[0] = (uint8_t)(p[0] * 0.45f); p[1] = (uint8_t)(p[1] * 0.45f); p[2] = (uint8_t)(p[2] * 0.45f); }
        }
    return out;
}

Image sphere(float radius, Color albedo, const Material& m, float scale) {
    int S = (int)std::ceil(radius * 2 * scale) + 2;
    Image out(S, S);
    float c = S / 2.f, r = radius * scale;
    Vec3 A = rgb(albedo);
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float dx = (x + 0.5f - c) / r, dy = (y + 0.5f - c) / r;
            float d2 = dx * dx + dy * dy;
            float cov = clamp01((1 - std::sqrt(d2)) * r + 0.5f);
            if (cov <= 0) continue;
            Vec3 N(dx, dy, std::sqrt(std::max(0.f, 1 - d2)));
            uint8_t* p = out.at(x, y);
            store(p, shadePixel(A, N.norm(), m));
            p[3] = (uint8_t)(cov * 255);
        }
    return out;
}

Image glassOrb(float radius, Color tint, float scale) {
    int S = (int)std::ceil(radius * 2 * scale) + 2;
    Image out(S, S);
    float c = S / 2.f, r = radius * scale;
    Vec3 Tn = rgb(tint);
    const Vec3 V(0, 0, 1);
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float dx = (x + 0.5f - c) / r, dy = (y + 0.5f - c) / r;
            float d2 = dx * dx + dy * dy;
            float cov = clamp01((1 - std::sqrt(d2)) * r + 0.5f);
            if (cov <= 0) continue;
            Vec3 N = Vec3(dx, dy, std::sqrt(std::max(0.f, 1 - d2))).norm();
            Vec3 R(2 * N.z * N.x, 2 * N.z * N.y, 2 * N.z * N.z - 1);
            Vec3 E = environment(R);
            float F = 0.04f + 0.96f * std::pow(1 - N.z, 4.f);
            // inner glow (light focused by the glass towards the lower right)
            float core = std::exp(-((dx - 0.12f) * (dx - 0.12f) + (dy - 0.18f) * (dy - 0.18f)) / 0.22f);
            float caustic = std::exp(-((dx - 0.35f) * (dx - 0.35f) + (dy - 0.42f) * (dy - 0.42f)) / 0.03f);
            Vec3 inner = Tn * (0.22f + 1.1f * core) + Tn * (1.6f * caustic) + Vec3(1, 1, 1) * (0.35f * caustic);
            Vec3 Hh = (KEY + V).norm();
            float spec = std::pow(std::max(0.f, N.dot(Hh)), 300.f) * 3.f;
            Vec3 col = inner * (1 - F) + (E * 0.9f + Tn * 0.6f) * F + Vec3(spec, spec, spec);
            uint8_t* p = out.at(x, y);
            store(p, col);
            p[3] = (uint8_t)(cov * 255);
        }
    return out;
}

Image chipPiece(const Image& top, Color base, Color spot, float R, float thick, float elevationDeg, float scale,
                float* rimHeight) {
    float e = elevationDeg * PI / 180.f;
    float se = std::sin(e), ce = std::cos(e);
    float wL = R * 2, hL = R * 2 * se + thick * ce;
    int W = (int)std::ceil(wL * scale) + 2, H = (int)std::ceil(hL * scale) + 2;
    Image out(W, H);
    if (rimHeight) *rimHeight = thick * ce;
    Vec3 Bc = rgb(base), Sc = rgb(spot);
    Vec3 Lw = Vec3(-0.45f, 0.55f, 0.75f).norm(); // chip space: x right, y towards viewer, z up
    auto sampleTop = [&](float X, float Y) -> std::array<float, 4> {
        // top-view image covers [-R,R]^2
        float u = (X + R) / (2 * R) * top.w - 0.5f, v = (Y + R) / (2 * R) * top.h - 0.5f;
        int iu = (int)std::floor(u), iv = (int)std::floor(v);
        float fu = u - iu, fv = v - iv;
        std::array<float, 4> o = {0, 0, 0, 0};
        for (int k = 0; k < 4; k++) {
            int xx = std::clamp(iu + (k & 1), 0, top.w - 1), yy = std::clamp(iv + (k >> 1), 0, top.h - 1);
            float wgt = ((k & 1) ? fu : 1 - fu) * ((k >> 1) ? fv : 1 - fv);
            const uint8_t* p = top.at(xx, yy);
            for (int c = 0; c < 4; c++) o[c] += p[c] * wgt;
        }
        return o;
    };
    for (int py = 0; py < H; py++)
        for (int px = 0; px < W; px++) {
            Vec3 acc;
            float alpha = 0;
            for (int s = 0; s < 4; s++) {
                float x = (px + 0.25f + 0.5f * (s & 1)) / scale - 0.5f / scale;
                float y = (py + 0.25f + 0.5f * (s >> 1)) / scale - 0.5f / scale;
                float X = x - R;
                float Y = (y - R * se) / se;
                if (X * X + Y * Y <= R * R) {
                    auto t = sampleTop(X, Y);
                    if (t[3] > 1) {
                        float a = t[3] / 255.f;
                        Vec3 c(t[0] / 255.f, t[1] / 255.f, t[2] / 255.f);
                        // top rim catches the light
                        float rr = std::sqrt(X * X + Y * Y) / R;
                        if (rr > 0.93f) c = c * (1.f + 0.35f * (rr - 0.93f) / 0.07f * std::max(0.f, -Y / R + 0.3f));
                        acc = acc + c * a;
                        alpha += a;
                        continue;
                    }
                }
                if (std::fabs(X) <= R) {
                    float Yf = std::sqrt(std::max(0.f, R * R - X * X));
                    float depth = (y - R * se - Yf * se) / ce;
                    if (depth >= 0 && depth <= thick) {
                        float th = std::atan2(Yf, X); // 0..pi, front of the rim
                        // stripes match the top face's 8 edge spots (top view angle == th mirrored)
                        float ang = std::fmod(th * 180.f / PI + 360.f, 45.f);
                        bool isSpot = std::fabs(ang - 22.5f) < 8.5f;
                        Vec3 col = isSpot ? Sc : Bc;
                        Vec3 n(std::cos(th), std::sin(th), 0);
                        float dif = std::max(0.f, n.dot(Lw));
                        float edge = std::min(depth, thick - depth) / thick;
                        float groove = edge < 0.12f ? 0.65f : 1.f;
                        Vec3 c = col * (0.35f + 0.75f * dif) * groove;
                        float spec = std::pow(std::max(0.f, n.dot(Vec3(-0.2f, 0.98f, 0.1f).norm())), 30.f) * 0.25f;
                        c = c + Vec3(spec, spec, spec);
                        acc = acc + c;
                        alpha += 1;
                    }
                }
            }
            if (alpha <= 0) continue;
            Vec3 c = acc * (1.f / std::max(alpha, 1e-3f));
            uint8_t* p = out.at(px, py);
            store(p, c);
            p[3] = (uint8_t)std::clamp(alpha / 4.f * 255.f, 0.f, 255.f);
        }
    return out;
}

void glints(Image& img, int count, uint32_t seed, float size) {
    Rng r(seed);
    std::vector<std::pair<float, int>> bright;
    for (int y = 1; y < img.h - 1; y += 2)
        for (int x = 1; x < img.w - 1; x += 2) {
            const uint8_t* p = img.at(x, y);
            if (p[3] < 250) continue;
            float l = (p[0] + p[1] + p[2]) / 765.f;
            if (l > 0.82f) bright.push_back({l, y * img.w + x});
        }
    if (bright.empty()) return;
    std::sort(bright.begin(), bright.end(), [](auto& a, auto& b) { return a.first > b.first; });
    int take = std::min<int>(count, (int)bright.size());
    for (int k = 0; k < take; k++) {
        int idx = bright[r.range(0, std::min<int>((int)bright.size(), take * 8) - 1)].second;
        int cx = idx % img.w, cy = idx / img.w;
        float s = size * r.uniform(0.6f, 1.f);
        for (int y = -(int)s; y <= (int)s; y++)
            for (int x = -(int)s; x <= (int)s; x++) {
                float ax = std::fabs((float)x), ay = std::fabs((float)y);
                float star = std::max(std::exp(-ay * ay * 2.5f) * (1 - ax / s), std::exp(-ax * ax * 2.5f) * (1 - ay / s));
                star = std::max(star, std::exp(-(ax * ax + ay * ay) / (s * 0.2f)) * 0.8f);
                if (star <= 0.02f) continue;
                int xx = cx + x, yy = cy + y;
                if (xx < 0 || yy < 0 || xx >= img.w || yy >= img.h) continue;
                uint8_t* p = img.at(xx, yy);
                if (!p[3]) continue;
                for (int c = 0; c < 3; c++) p[c] = (uint8_t)std::min(255.f, p[c] + 255 * star);
            }
    }
}

} // namespace shade
