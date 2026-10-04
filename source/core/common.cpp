#include "common.h"

#include <cstdarg>

Rng& rng() {
    static Rng r;
    return r;
}

std::string strf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < (int)sizeof buf) return std::string(buf, n < 0 ? 0 : n);
    std::string out(n + 1, '\0');
    va_start(ap, fmt);
    vsnprintf(out.data(), out.size(), fmt, ap);
    va_end(ap);
    out.resize(n);
    return out;
}

std::string fmtMoney(i64 v) {
    bool neg = v < 0;
    u64 a = neg ? (u64)(-v) : (u64)v;
    std::string digits = std::to_string(a);
    std::string out;
    int n = (int)digits.size();
    for (int i = 0; i < n; i++) {
        out += digits[i];
        int rest = n - 1 - i;
        if (rest > 0 && rest % 3 == 0) out += "\xC2\xA0"; // no-break space
    }
    return neg ? "-" + out : out;
}

std::string fmtChip(i64 v) {
    if (v >= 1000000 && v % 1000000 == 0) return std::to_string(v / 1000000) + "M";
    if (v >= 1000 && v % 1000 == 0) return std::to_string(v / 1000) + "K";
    if (v >= 1000 && v % 100 == 0) return strf("%.1fK", v / 1000.0);
    return std::to_string(v);
}
