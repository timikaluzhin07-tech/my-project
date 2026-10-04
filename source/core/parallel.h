// Splits per-pixel work across CPU cores (the Switch gives apps three of them).
#pragma once

#include <thread>

#include "common.h"

// Calls fn(begin, end) on disjoint slices of [0, n).
inline void parallelFor(int n, const std::function<void(int, int)>& fn) {
    unsigned hw = std::thread::hardware_concurrency();
#ifdef __SWITCH__
    int threads = 3; // cores 0-2 belong to applications
    (void)hw;
#else
    int threads = (int)std::clamp(hw == 0 ? 3u : hw, 1u, 4u);
#endif
    if (n < 64 || threads == 1) { fn(0, n); return; }
    std::vector<std::thread> pool;
    int chunk = (n + threads - 1) / threads;
    for (int t = 1; t < threads; t++) {
        int b = t * chunk, e = std::min(n, b + chunk);
        if (b < e) pool.emplace_back([&fn, b, e] { fn(b, e); });
    }
    fn(0, std::min(n, chunk));
    for (auto& th : pool) th.join();
}
