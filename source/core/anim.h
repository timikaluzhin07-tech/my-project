// Small animation helpers: a timed action queue, moving cards and flying chips.
#pragma once

#include <deque>

#include "../art/art.h"
#include "common.h"

// Runs callbacks one after another with delays — dealer choreography.
class Seq {
public:
    // Waits `delay` seconds after the previous step, then runs fn.
    void then(float delay, std::function<void()> fn) { q_.push_back({delay, std::move(fn)}); }
    void wait(float delay) { then(delay, [] {}); }
    void update(float dt) {
        t_ += dt;
        while (!q_.empty() && t_ >= q_.front().delay) {
            t_ -= q_.front().delay;
            auto fn = std::move(q_.front().fn);
            q_.pop_front();
            if (fn) fn();
        }
        if (q_.empty()) t_ = 0;
    }
    bool busy() const { return !q_.empty(); }
    void clear() { q_.clear(); t_ = 0; }

private:
    struct Step { float delay; std::function<void()> fn; };
    std::deque<Step> q_;
    float t_ = 0;
};

// Independent delayed callbacks (unlike Seq, these run in parallel).
class Timers {
public:
    void add(float delay, std::function<void()> fn) { items_.push_back({delay, std::move(fn)}); }
    void update(float dt) {
        for (size_t i = 0; i < items_.size(); i++) items_[i].t -= dt;
        std::vector<Item> due;
        for (auto it = items_.begin(); it != items_.end();) {
            if (it->t <= 0) { due.push_back(std::move(*it)); it = items_.erase(it); }
            else ++it;
        }
        for (auto& d : due) if (d.fn) d.fn();
    }
    bool busy() const { return !items_.empty(); }
    void clear() { items_.clear(); }

private:
    struct Item { float t; std::function<void()> fn; };
    std::vector<Item> items_;
};

// A card that glides between table positions and can flip over.
struct CardSprite {
    Card card;
    float x = 0, y = 0, angle = 0, face = 0, scale = 1;
    float sx = 0, sy = 0, sa = 0, sf = 0, ss = 1;
    float tx = 0, ty = 0, ta = 0, tf = 0, ts = 1;
    float t = 1, dur = 0.3f, delay = 0;
    float alpha = 1;
    bool highlight = false;
    bool dim = false;

    void moveTo(float nx, float ny, float nangle, float nface, float nscale, float duration, float wait = 0) {
        sx = x; sy = y; sa = angle; sf = face; ss = scale;
        tx = nx; ty = ny; ta = nangle; tf = nface; ts = nscale;
        t = 0; dur = std::max(0.001f, duration); delay = wait;
    }
    void place(float nx, float ny, float nangle, float nface, float nscale) {
        x = tx = nx; y = ty = ny; angle = ta = nangle; face = tf = nface; scale = ts = nscale; t = 1;
    }
    bool moving() const { return t < 1; }
    void update(float dt) {
        if (delay > 0) { delay -= dt; return; }
        if (t >= 1) return;
        t = std::min(1.f, t + dt / dur);
        float e = ease::outCubic(t);
        x = lerp(sx, tx, e);
        y = lerp(sy, ty, e) - std::sin(t * PI) * 10 * (std::fabs(tx - sx) > 40 ? 1.f : 0.f);
        angle = lerp(sa, ta, e);
        scale = lerp(ss, ts, e);
        float ft = clamp01((t - 0.35f) / 0.65f);
        face = lerp(sf, tf, ease::inOutCubic(ft));
    }
    void draw() const {
        if (alpha <= 0) return;
        art::drawCard(card, x, y, scale, angle, face, alpha, true, dim ? Color(150, 146, 140) : pal::white);
    }
};

// A stack of chips travelling across the table (bets, payouts, collections).
struct FlyingChips {
    i64 amount = 0;
    float fx, fy, tx, ty;
    float t = 0, dur = 0.45f, delay = 0;
    float scale = 0.8f;
    int playerColor = -1; // >=0: draw as coloured roulette chips
    int count = 1;
    std::function<void()> onArrive;
    bool done = false;

    void update(float dt) {
        if (done) return;
        if (delay > 0) { delay -= dt; return; }
        t += dt / dur;
        if (t >= 1) {
            t = 1;
            done = true;
            if (onArrive) onArrive();
        }
    }
    void draw() const {
        if (done || delay > 0) return;
        float e = ease::inOutCubic(t);
        float x = lerp(fx, tx, e), y = lerp(fy, ty, e) - std::sin(e * PI) * 24;
        if (playerColor >= 0) art::drawPlayerChipStack(playerColor, count, x, y, scale);
        else art::drawChipStack(amount, x, y, scale, 1.f, 2);
    }
};

template <class T>
inline void pruneDone(std::vector<T>& v) {
    v.erase(std::remove_if(v.begin(), v.end(), [](const T& f) { return f.done; }), v.end());
}
