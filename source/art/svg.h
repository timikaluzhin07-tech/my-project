// Tiny helpers for composing SVG markup that nanosvg rasterizes into textures.
#pragma once

#include "../core/common.h"

namespace svg {

inline std::string num(float v) {
    char b[32];
    snprintf(b, sizeof b, "%.2f", v);
    // trim trailing zeros to keep markup small
    std::string s = b;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    if (s == "-0") s = "0";
    return s;
}

inline std::string open(float w, float h) {
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + num(w) + "\" height=\"" + num(h) +
           "\" viewBox=\"0 0 " + num(w) + " " + num(h) + "\">";
}
inline std::string close() { return "</svg>"; }

struct Stop { float offset; Color color; float opacity = 1.f; };

inline std::string stops(const std::vector<Stop>& st) {
    std::string s;
    for (auto& x : st)
        s += "<stop offset=\"" + num(x.offset) + "\" stop-color=\"" + x.color.css() + "\" stop-opacity=\"" +
             num(x.opacity) + "\"/>";
    return s;
}

// Linear gradient in objectBoundingBox units (0..1).
inline std::string linear(const std::string& id, float x1, float y1, float x2, float y2, const std::vector<Stop>& st) {
    return "<linearGradient id=\"" + id + "\" x1=\"" + num(x1) + "\" y1=\"" + num(y1) + "\" x2=\"" + num(x2) +
           "\" y2=\"" + num(y2) + "\">" + stops(st) + "</linearGradient>";
}
// Linear gradient in user-space coordinates.
inline std::string linearU(const std::string& id, float x1, float y1, float x2, float y2, const std::vector<Stop>& st) {
    return "<linearGradient id=\"" + id + "\" gradientUnits=\"userSpaceOnUse\" x1=\"" + num(x1) + "\" y1=\"" +
           num(y1) + "\" x2=\"" + num(x2) + "\" y2=\"" + num(y2) + "\">" + stops(st) + "</linearGradient>";
}
inline std::string radial(const std::string& id, float cx, float cy, float r, const std::vector<Stop>& st,
                          float fx = -1, float fy = -1) {
    std::string f = fx >= 0 ? " fx=\"" + num(fx) + "\" fy=\"" + num(fy) + "\"" : "";
    return "<radialGradient id=\"" + id + "\" cx=\"" + num(cx) + "\" cy=\"" + num(cy) + "\" r=\"" + num(r) + "\"" + f +
           ">" + stops(st) + "</radialGradient>";
}
inline std::string radialU(const std::string& id, float cx, float cy, float r, const std::vector<Stop>& st) {
    return "<radialGradient id=\"" + id + "\" gradientUnits=\"userSpaceOnUse\" cx=\"" + num(cx) + "\" cy=\"" +
           num(cy) + "\" r=\"" + num(r) + "\">" + stops(st) + "</radialGradient>";
}

inline std::string rect(float x, float y, float w, float h, float rx, const std::string& fill,
                        const std::string& extra = "") {
    return "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(w) + "\" height=\"" + num(h) +
           "\" rx=\"" + num(rx) + "\" ry=\"" + num(rx) + "\" fill=\"" + fill + "\" " + extra + "/>";
}
inline std::string circle(float cx, float cy, float r, const std::string& fill, const std::string& extra = "") {
    return "<circle cx=\"" + num(cx) + "\" cy=\"" + num(cy) + "\" r=\"" + num(r) + "\" fill=\"" + fill + "\" " +
           extra + "/>";
}
inline std::string ellipse(float cx, float cy, float rx, float ry, const std::string& fill,
                           const std::string& extra = "") {
    return "<ellipse cx=\"" + num(cx) + "\" cy=\"" + num(cy) + "\" rx=\"" + num(rx) + "\" ry=\"" + num(ry) +
           "\" fill=\"" + fill + "\" " + extra + "/>";
}
inline std::string path(const std::string& d, const std::string& fill, const std::string& extra = "") {
    return "<path d=\"" + d + "\" fill=\"" + fill + "\" " + extra + "/>";
}
inline std::string stroke(const std::string& color, float width, float opacity = 1.f) {
    return "stroke=\"" + color + "\" stroke-width=\"" + num(width) + "\" stroke-opacity=\"" + num(opacity) + "\"";
}
inline std::string line(float x1, float y1, float x2, float y2, const std::string& color, float width,
                        float opacity = 1.f) {
    return "<path d=\"M" + num(x1) + " " + num(y1) + " L" + num(x2) + " " + num(y2) + "\" fill=\"none\" " +
           stroke(color, width, opacity) + " stroke-linecap=\"round\"/>";
}
inline std::string url(const std::string& id) { return "url(#" + id + ")"; }

// Annular sector (ring slice) between radii r0..r1 and angles a0..a1 (radians).
inline std::string sector(float cx, float cy, float r0, float r1, float a0, float a1) {
    auto P = [&](float r, float a) { return num(cx + std::cos(a) * r) + " " + num(cy + std::sin(a) * r); };
    int large = (a1 - a0) > PI ? 1 : 0;
    return "M" + P(r1, a0) + " A" + num(r1) + " " + num(r1) + " 0 " + std::to_string(large) + " 1 " + P(r1, a1) +
           " L" + P(r0, a1) + " A" + num(r0) + " " + num(r0) + " 0 " + std::to_string(large) + " 0 " + P(r0, a0) + " Z";
}

// Regular polygon / star path.
inline std::string star(float cx, float cy, float rOuter, float rInner, int points, float rot = -PI / 2) {
    std::string d;
    for (int i = 0; i < points * 2; i++) {
        float r = (i % 2 == 0) ? rOuter : rInner;
        float a = rot + i * PI / points;
        d += (i == 0 ? "M" : " L") + num(cx + std::cos(a) * r) + " " + num(cy + std::sin(a) * r);
    }
    return d + " Z";
}

} // namespace svg
