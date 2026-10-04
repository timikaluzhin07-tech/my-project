// Art preview: renders procedural sprites into a contact sheet (BMP) without running the game.
//   make -f Makefile.pc artlab && ./build-pc/artlab out.bmp
#include <SDL.h>

#include "../source/art/art.h"
#include "../source/art/shade.h"
#include "../source/art/svg.h"
#include "../source/core/gfx.h"
#include "../source/core/platform.h"

void artlabSheet(Image& sheet); // implemented below, edited while iterating on art

static void save(const Image& img, const char* path) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom((void*)img.px.data(), img.w, img.h, 32, img.w * 4, SDL_PIXELFORMAT_RGBA32);
    SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
}

int main(int argc, char** argv) {
    platform::init();
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* w = SDL_CreateWindow("artlab", 0, 0, 64, 64, 0);
    SDL_Renderer* r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
    gfx::init(r);
    Image sheet(1600, 1000, Color(40, 36, 34));
    artlabSheet(sheet);
    save(sheet, argc > 1 ? argv[1] : "artlab.bmp");
    art::shutdown();
    gfx::shutdown();
    SDL_Quit();
    return 0;
}

// ---------------------------------------------------------------------------
using namespace shade;

void artlabSheet(Image& sheet) {
    // cards
    int x = 20;
    for (Card c : {Card{13, HEARTS}, Card{12, SPADES}, Card{11, DIAMONDS}, Card{14, SPADES}, Card{7, CLUBS}}) {
        sheet.draw(art::cardImage(c), x, 300);
        x += 160;
    }
    sheet.draw(art::cardBackImage(), x, 300);
    x += 160;
    for (int i = 0; i < 4; i++) sheet.draw(art::chipImage(i * 2, false), x + i * 80, 300);
    for (int i = 0; i < 4; i++) sheet.draw(art::chipImage(i * 2, true), x + i * 80, 400);
    // chips: relief top view + 3D piece
    float R = 23;
    std::vector<Layer> L;
    std::string base = svg::open(46, 46) + svg::circle(23, 23, 22, "#b3202e") + svg::close();
    Layer body; body.svg = base; body.mat = mat::clay(); body.bevel = 3.5f; body.depth = 0.9f; body.grain = 0.04f;
    L.push_back(body);
    std::string spots = svg::open(46, 46);
    for (int i = 0; i < 8; i++) {
        float a = (i * 45 + 22.5f) * PI / 180, hw = 8.5f * PI / 180;
        spots += svg::path(svg::sector(23, 23, 15.5f, 21.6f, a - hw, a + hw), "#f2f0ea");
    }
    spots += svg::close();
    Layer sp; sp.svg = spots; sp.mat = mat::clay(); sp.bevel = 0.8f; sp.depth = 0.5f;
    L.push_back(sp);
    Layer inl; inl.svg = svg::open(46, 46) + svg::circle(23, 23, 11.5f, "#efe8d6") + svg::close(); inl.mat = mat::gloss(); inl.bevel = 1.4f; inl.depth = 0.6f;
    L.push_back(inl);
    Image top = relief(46, 46, L);
    sheet.draw(top, 20, 20);
    float rim;
    Image piece = chipPiece(top, Color::hex(0xb3202e), Color::hex(0xf2f0ea), R, 7, 58, TEX_SCALE, &rim);
    for (int k = 0; k < 8; k++) sheet.draw(piece, 120, (int)(120 - k * rim * TEX_SCALE));
    // gems
    std::vector<SDL_FPoint> ov;
    for (int i = 0; i < 12; i++) { float a = -PI / 2 + i * 2 * PI / 12; ov.push_back({40 + std::cos(a) * 30, 42 + std::sin(a) * 36}); }
    Image g1 = gem(80, 84, ov, {40, 42}, 0.55f, Color::hex(0x1d58d0));
    sheet.draw(g1, 260, 20);
    std::vector<SDL_FPoint> hx;
    for (int i = 0; i < 8; i++) { float a = -PI / 2 + i * 2 * PI / 8; hx.push_back({40 + std::cos(a) * 36, 42 + std::sin(a) * 36}); }
    Image g2 = gem(80, 84, hx, {40, 42}, 0.5f, Color::hex(0xc81030));
    glints(g2, 3, 5, 9);
    sheet.draw(g2, 420, 20);
    Image g3 = gem(80, 84, hx, {40, 42}, 0.5f, Color::hex(0x0f9a50));
    sheet.draw(g3, 580, 20);
    // spheres
    sheet.draw(sphere(20, Color::hex(0xf2f0ea), mat::ivory()), 740, 20);
    sheet.draw(glassOrb(36, Color::hex(0x2a6aff)), 860, 20);
    sheet.draw(glassOrb(36, Color::hex(0xd0401a)), 1000, 20);
    // gold relief
    std::vector<Layer> cr;
    Layer crown; crown.svg = svg::open(100, 100) + svg::path("M12 78 L8 34 L28 54 L37 24 L50 46 L63 24 L72 54 L92 34 L88 78 Z", "#f2c35a") + svg::close();
    crown.mat = mat::gold(); crown.bevel = 12; crown.depth = 0.9f;
    cr.push_back(crown);
    Layer band; band.svg = svg::open(100, 100) + svg::rect(12, 72, 76, 16, 3, "#e8b54e") + svg::close(); band.mat = mat::gold(); band.bevel = 7; band.shadow = 2;
    cr.push_back(band);
    Layer stones; stones.svg = svg::open(100, 100) + svg::circle(30, 80, 5, "#c01030") + svg::circle(50, 80, 6, "#1d58d0") + svg::circle(70, 80, 5, "#109a50") + svg::close();
    stones.mat = mat::gloss(); stones.bevel = 4; stones.depth = 1.5f;
    cr.push_back(stones);
    sheet.draw(relief(100, 100, cr), 1140, 20);
}
