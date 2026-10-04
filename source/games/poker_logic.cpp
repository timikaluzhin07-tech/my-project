#include "poker_logic.h"

namespace poker {

// ---------------------------------------------------------------------------
// Evaluation
// ---------------------------------------------------------------------------
static int straightHigh(uint32_t mask) {
    if (mask & (1u << 14)) mask |= 1u << 1; // wheel: ace plays low
    for (int hi = 14; hi >= 5; hi--)
        if (((mask >> (hi - 4)) & 0x1F) == 0x1F) return hi;
    return 0;
}

static uint32_t pack(Category c, const int* r, int n) {
    uint32_t s = (uint32_t)c << 20;
    for (int i = 0; i < 5; i++) s |= (uint32_t)(i < n ? r[i] : 0) << (16 - 4 * i);
    return s;
}

uint32_t evaluate(const Card* cards, int n) {
    int counts[15] = {0};
    int suitCount[4] = {0};
    uint32_t suitMask[4] = {0}, rankMask = 0;
    for (int i = 0; i < n; i++) {
        int r = cards[i].rank, s = cards[i].suit;
        counts[r]++;
        suitCount[s]++;
        suitMask[s] |= 1u << r;
        rankMask |= 1u << r;
    }
    int flushSuit = -1;
    for (int s = 0; s < 4; s++)
        if (suitCount[s] >= 5) flushSuit = s;
    if (flushSuit >= 0) {
        int sh = straightHigh(suitMask[flushSuit]);
        if (sh) { int r[1] = {sh}; return pack(STRAIGHT_FLUSH, r, 1); }
    }
    int quads = 0, trips[3], nt = 0, pairs[4], np = 0, singles[7], ns = 0;
    for (int r = 14; r >= 2; r--) {
        if (counts[r] == 4) quads = r;
        else if (counts[r] == 3) trips[nt++] = r;
        else if (counts[r] == 2) pairs[np++] = r;
        else if (counts[r] == 1) singles[ns++] = r;
    }
    if (quads) {
        int k = 0;
        for (int r = 14; r >= 2; r--)
            if (r != quads && counts[r] > 0) { k = r; break; }
        int r[2] = {quads, k};
        return pack(QUADS, r, 2);
    }
    if (nt >= 1 && (np >= 1 || nt >= 2)) {
        int pr = nt >= 2 ? trips[1] : 0;
        if (np >= 1) pr = std::max(pr, pairs[0]);
        int r[2] = {trips[0], pr};
        return pack(FULL_HOUSE, r, 2);
    }
    if (flushSuit >= 0) {
        int r[5], k = 0;
        for (int x = 14; x >= 2 && k < 5; x--)
            if (suitMask[flushSuit] & (1u << x)) r[k++] = x;
        return pack(FLUSH, r, 5);
    }
    int sh = straightHigh(rankMask);
    if (sh) { int r[1] = {sh}; return pack(STRAIGHT, r, 1); }
    if (nt >= 1) {
        int r[3] = {trips[0], 0, 0}, k = 1;
        for (int x = 14; x >= 2 && k < 3; x--)
            if (counts[x] > 0 && x != trips[0]) r[k++] = x;
        return pack(TRIPS, r, 3);
    }
    if (np >= 2) {
        int r[3] = {pairs[0], pairs[1], 0};
        for (int x = 14; x >= 2; x--)
            if (counts[x] > 0 && x != pairs[0] && x != pairs[1]) { r[2] = x; break; }
        return pack(TWO_PAIR, r, 3);
    }
    if (np == 1) {
        int r[4] = {pairs[0], 0, 0, 0}, k = 1;
        for (int x = 14; x >= 2 && k < 4; x--)
            if (counts[x] > 0 && x != pairs[0]) r[k++] = x;
        return pack(PAIR, r, 4);
    }
    int r[5] = {0}, k = 0;
    for (int i = 0; i < ns && k < 5; i++) r[k++] = singles[i];
    return pack(HIGH_CARD, r, k);
}

std::vector<Card> bestFive(const Card* cards, int n) {
    uint32_t best = evaluate(cards, n);
    std::vector<Card> out;
    if (n <= 5) return std::vector<Card>(cards, cards + n);
    int idx[5];
    for (idx[0] = 0; idx[0] < n; idx[0]++)
        for (idx[1] = idx[0] + 1; idx[1] < n; idx[1]++)
            for (idx[2] = idx[1] + 1; idx[2] < n; idx[2]++)
                for (idx[3] = idx[2] + 1; idx[3] < n; idx[3]++)
                    for (idx[4] = idx[3] + 1; idx[4] < n; idx[4]++) {
                        Card c[5];
                        for (int k = 0; k < 5; k++) c[k] = cards[idx[k]];
                        if (evaluate(c, 5) == best) return std::vector<Card>(c, c + 5);
                    }
    return out;
}

std::string categoryName(Category c) {
    static const char* N[] = {"Старшая карта", "Пара", "Две пары", "Тройка", "Стрит",
                              "Флеш", "Фулл-хаус", "Каре", "Стрит-флеш"};
    return N[c];
}

static const char* rankGenitive(int r) {
    static const char* N[] = {"", "", "двойки", "тройки", "четвёрки", "пятёрки", "шестёрки", "семёрки", "восьмёрки",
                              "девятки", "десятки", "валета", "дамы", "короля", "туза"};
    return (r >= 2 && r <= 14) ? N[r] : "";
}

static const char* rankPlural(int r) {
    static const char* N[] = {"", "", "двойки", "тройки", "четвёрки", "пятёрки", "шестёрки", "семёрки", "восьмёрки",
                              "девятки", "десятки", "валеты", "дамы", "короли", "тузы"};
    return (r >= 2 && r <= 14) ? N[r] : "";
}

std::string describe(uint32_t s) {
    Category c = category(s);
    int r0 = (s >> 16) & 15, r1 = (s >> 12) & 15;
    switch (c) {
        case HIGH_CARD: return std::string("Старшая карта: ") + rankNameNominative(r0);
        case PAIR: return std::string("Пара ") + rankNameGenitivePlural(r0);
        case TWO_PAIR: return std::string("Две пары: ") + rankPlural(r0) + " и " + rankPlural(r1);
        case TRIPS: return std::string("Тройка ") + rankNameGenitivePlural(r0);
        case STRAIGHT: return std::string("Стрит до ") + rankGenitive(r0);
        case FLUSH: return std::string("Флеш, старший ") + rankNameNominative(r0);
        case FULL_HOUSE:
            return std::string("Фулл-хаус: три ") + rankGenitive(r0) + (r1 == 11 || r1 >= 13 ? " и два " : " и две ") +
                   rankGenitive(r1);
        case QUADS: return std::string("Каре ") + rankNameGenitivePlural(r0);
        case STRAIGHT_FLUSH: return r0 == 14 ? "Роял-флеш" : std::string("Стрит-флеш до ") + rankGenitive(r0);
    }
    return "";
}

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------
int Table::nextSeat(int from, bool needChips) const {
    for (int k = 1; k <= SEATS; k++) {
        int i = (from + k) % SEATS;
        if (!seats[i].used) continue;
        if (needChips && seats[i].stack <= 0) continue;
        return i;
    }
    return -1;
}

int Table::seatedWithChips() const {
    int n = 0;
    for (auto& s : seats) n += s.used && s.stack > 0;
    return n;
}

i64 Table::totalChips() const {
    i64 t = 0;
    for (auto& s : seats)
        if (s.used) t += s.stack + s.totalBet;
    return t;
}

bool Table::startHand() {
    if (seatedWithChips() < 2) return false;
    handNo++;
    deck_.refill();
    board.clear();
    street = PREFLOP;
    for (auto& s : seats) {
        s.inHand = s.used && s.stack > 0;
        s.folded = s.allIn = false;
        s.streetBet = s.totalBet = 0;
        s.acted = false;
        s.actedRaiseId = -1;
        s.lastAction.clear();
    }
    button = nextSeat(button < 0 ? rng().range(0, SEATS - 1) : button, true);
    int players = seatedWithChips();
    if (players == 2) {
        sbSeat = button;               // heads-up: the button posts the small blind
        bbSeat = nextSeat(button, true);
    } else {
        sbSeat = nextSeat(button, true);
        bbSeat = nextSeat(sbSeat, true);
    }
    auto post = [&](int i, i64 amt, const char* label) {
        Seat& s = seats[i];
        amt = std::min(amt, s.stack);
        s.stack -= amt;
        s.streetBet += amt;
        s.totalBet += amt;
        if (s.stack == 0) s.allIn = true;
        s.lastAction = label;
    };
    post(sbSeat, sb, "Малый блайнд");
    post(bbSeat, bb, "Большой блайнд");
    currentBet = bb;
    lastRaise = bb;
    raiseId = 0;
    for (int round = 0; round < 2; round++) {
        int i = sbSeat;
        for (int k = 0; k < SEATS; k++) {
            if (seats[i].inHand) seats[i].hole[round] = deck_.draw();
            i = (i + 1) % SEATS;
        }
    }
    toAct = nextToAct(bbSeat);
    if (toAct >= 0 && roundDone()) toAct = -1;
    return true;
}

int Table::nextToAct(int from) const {
    for (int k = 1; k <= SEATS; k++) {
        int i = (from + k) % SEATS;
        const Seat& s = seats[i];
        if (s.canAct() && (!s.acted || s.streetBet < currentBet)) return i;
    }
    return -1;
}

int Table::canActCount() const {
    int n = 0;
    for (auto& s : seats) n += s.canAct();
    return n;
}

bool Table::onlyOneLeft() const {
    int n = 0;
    for (auto& s : seats) n += s.live();
    return n <= 1;
}

bool Table::roundDone() const {
    if (onlyOneLeft()) return true;
    int actors = 0;
    for (auto& s : seats) {
        if (!s.canAct()) continue;
        actors++;
        if (s.streetBet < currentBet) return false; // someone still owes chips
    }
    if (actors <= 1) return true; // nobody left to bet against
    for (auto& s : seats)
        if (s.canAct() && !s.acted) return false;
    return true;
}

Legal Table::legal(int i) const {
    Legal l;
    const Seat& s = seats[i];
    l.toCall = std::min(currentBet - s.streetBet, s.stack);
    l.canCheck = currentBet <= s.streetBet;
    l.maxRaiseTo = s.stack + s.streetBet;
    l.minRaiseTo = currentBet == 0 ? bb : currentBet + lastRaise;
    if (l.minRaiseTo > l.maxRaiseTo) l.minRaiseTo = l.maxRaiseTo;
    bool reopened = !s.acted || s.actedRaiseId != raiseId;
    int others = 0;
    for (int k = 0; k < SEATS; k++)
        if (k != i && seats[k].canAct()) others++;
    l.canRaise = s.stack > l.toCall && reopened && others > 0;
    return l;
}

void Table::apply(int i, Action a) {
    Seat& s = seats[i];
    Legal l = legal(i);
    auto put = [&](i64 amt) {
        amt = std::clamp<i64>(amt, 0, s.stack);
        s.stack -= amt;
        s.streetBet += amt;
        s.totalBet += amt;
        if (s.stack == 0) s.allIn = true;
    };
    if (a.type == ALLIN) {
        i64 to = s.stack + s.streetBet;
        if (to <= currentBet || !l.canRaise) a = {CALL, 0};
        else a = {RAISE, to};
    }
    if (a.type == CHECK && !l.canCheck) a.type = CALL;
    if (a.type == RAISE && !l.canRaise) a.type = l.canCheck ? CHECK : CALL;
    switch (a.type) {
        case FOLD:
            s.folded = true;
            s.lastAction = "Фолд";
            break;
        case CHECK:
            s.lastAction = "Чек";
            break;
        case CALL:
            put(l.toCall);
            s.lastAction = s.allIn ? "Олл-ин" : "Колл " + fmtMoney(s.streetBet);
            break;
        case RAISE: {
            i64 to = std::clamp(a.to, l.minRaiseTo, l.maxRaiseTo);
            bool wasBet = currentBet == 0;
            i64 raiseBy = to - currentBet;
            put(to - s.streetBet);
            if (raiseBy >= lastRaise) {
                lastRaise = raiseBy;
                raiseId++;
            }
            currentBet = std::max(currentBet, s.streetBet);
            if (s.allIn) s.lastAction = "Олл-ин";
            else s.lastAction = (wasBet ? "Бет " : "Рейз до ") + fmtMoney(s.streetBet);
            break;
        }
        default: break;
    }
    s.acted = true;
    s.actedRaiseId = raiseId;
    toAct = roundDone() ? -1 : nextToAct(i);
}

i64 Table::potTotal() const {
    i64 t = 0;
    for (auto& s : seats) t += s.totalBet;
    return t;
}

Street Table::nextStreet() {
    for (auto& s : seats) {
        s.streetBet = 0;
        s.acted = false;
        s.actedRaiseId = -1;
        if (s.live() && !s.allIn) s.lastAction.clear();
    }
    currentBet = 0;
    lastRaise = bb;
    raiseId++;
    if (street == PREFLOP) {
        deck_.draw(); // burn
        for (int k = 0; k < 3; k++) board.push_back(deck_.draw());
        street = FLOP;
    } else if (street == FLOP) {
        deck_.draw();
        board.push_back(deck_.draw());
        street = TURN;
    } else if (street == TURN) {
        deck_.draw();
        board.push_back(deck_.draw());
        street = RIVER;
    } else {
        street = SHOWDOWN;
    }
    toAct = -1;
    if (street != SHOWDOWN && canActCount() >= 2) toAct = nextToAct(button);
    return street;
}

std::vector<Pot> Table::pots() const {
    std::vector<i64> levels;
    for (auto& s : seats)
        if (s.live() && s.totalBet > 0) levels.push_back(s.totalBet);
    std::sort(levels.begin(), levels.end());
    levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
    std::vector<Pot> out;
    i64 prev = 0;
    for (i64 L : levels) {
        Pot p;
        for (int i = 0; i < SEATS; i++) {
            const Seat& s = seats[i];
            if (!s.inHand) continue;
            p.amount += std::min(s.totalBet, L) - std::min(s.totalBet, prev);
            if (s.live() && s.totalBet >= L) p.eligible.push_back(i);
        }
        if (!out.empty() && out.back().eligible == p.eligible) out.back().amount += p.amount;
        else if (p.amount > 0) out.push_back(p);
        prev = L;
    }
    // chips from folded players above every live contribution join the last pot
    i64 counted = 0;
    for (auto& p : out) counted += p.amount;
    i64 extra = potTotal() - counted;
    if (extra > 0) {
        if (out.empty()) {
            Pot p;
            for (int i = 0; i < SEATS; i++)
                if (seats[i].live()) p.eligible.push_back(i);
            out.push_back(p);
        }
        out.back().amount += extra;
    }
    return out;
}

std::vector<Award> Table::finish() {
    std::vector<Award> awards;
    auto ps = pots();
    std::vector<uint32_t> score(SEATS, 0);
    for (int i = 0; i < SEATS; i++) {
        if (!seats[i].live()) continue;
        std::vector<Card> c(board.begin(), board.end());
        c.push_back(seats[i].hole[0]);
        c.push_back(seats[i].hole[1]);
        score[i] = evaluate(c.data(), (int)c.size());
    }
    for (size_t pi = 0; pi < ps.size(); pi++) {
        const Pot& p = ps[pi];
        if (p.eligible.empty()) continue;
        uint32_t best = 0;
        for (int i : p.eligible) best = std::max(best, score[i]);
        std::vector<int> winners;
        // order winners starting left of the button (odd chips go there first)
        for (int k = 1; k <= SEATS; k++) {
            int i = (button + k) % SEATS;
            if (std::find(p.eligible.begin(), p.eligible.end(), i) != p.eligible.end() && score[i] == best)
                winners.push_back(i);
        }
        if (p.eligible.size() == 1) winners = {p.eligible[0]};
        i64 share = p.amount / (i64)winners.size();
        i64 rem = p.amount - share * (i64)winners.size();
        for (size_t w = 0; w < winners.size(); w++) {
            i64 amt = share + ((i64)w < rem ? 1 : 0);
            awards.push_back({winners[w], amt, (int)pi});
        }
    }
    for (auto& a : awards) seats[a.seat].stack += a.amount;
    for (auto& s : seats) {
        s.totalBet = 0;
        s.streetBet = 0;
    }
    street = SHOWDOWN;
    toAct = -1;
    return awards;
}

// ---------------------------------------------------------------------------
// Bots
// ---------------------------------------------------------------------------
float equity(const Card hole[2], const std::vector<Card>& board, int opponents, int iterations, Rng& r) {
    opponents = std::max(1, opponents);
    std::vector<Card> deck;
    for (int rk = 2; rk <= 14; rk++)
        for (int s = 0; s < 4; s++) {
            Card c{(int8_t)rk, (int8_t)s};
            if (c == hole[0] || c == hole[1]) continue;
            if (std::find(board.begin(), board.end(), c) != board.end()) continue;
            deck.push_back(c);
        }
    int need = 5 - (int)board.size();
    float wins = 0;
    Card mine[7], theirs[7];
    for (int it = 0; it < iterations; it++) {
        // partial Fisher-Yates for the cards we need
        int take = need + opponents * 2;
        for (int k = 0; k < take; k++) std::swap(deck[k], deck[k + r.range(0, (int)deck.size() - 1 - k)]);
        int n = 0;
        for (auto& c : board) mine[n++] = c;
        for (int k = 0; k < need; k++) mine[n++] = deck[k];
        int common = n;
        mine[n++] = hole[0];
        mine[n++] = hole[1];
        uint32_t my = evaluate(mine, n);
        bool lost = false;
        int ties = 0;
        for (int o = 0; o < opponents && !lost; o++) {
            for (int k = 0; k < common; k++) theirs[k] = mine[k];
            theirs[common] = deck[need + o * 2];
            theirs[common + 1] = deck[need + o * 2 + 1];
            uint32_t th = evaluate(theirs, common + 2);
            if (th > my) lost = true;
            else if (th == my) ties++;
        }
        if (!lost) wins += 1.f / (1 + ties);
    }
    return wins / iterations;
}

const BotStyle& botStyle(int idx) {
    static const BotStyle styles[BOT_STYLES] = {
        {"осторожный", 0.75f, 0.35f, 0.04f},
        {"агрессивный", 0.35f, 0.8f, 0.14f},
        {"расчётливый", 0.55f, 0.55f, 0.08f},
        {"рисковый", 0.25f, 0.6f, 0.2f},
    };
    return styles[((idx % BOT_STYLES) + BOT_STYLES) % BOT_STYLES];
}

static i64 roundTo(i64 v, i64 step) { return std::max(step, (v + step / 2) / step * step); }

Action botDecide(const Table& t, int i, Rng& r) {
    const Seat& s = t.seats[i];
    const BotStyle& st = botStyle(s.style);
    Legal l = t.legal(i);
    int opp = 0;
    for (int k = 0; k < SEATS; k++)
        if (k != i && t.seats[k].live()) opp++;
    float eq = equity(s.hole, t.board, opp, t.board.empty() ? 220 : 300, r);
    float rel = eq * (opp + 1); // 1.0 = an average hand in this field
    i64 pot = t.potTotal();
    i64 step = t.sb;
    auto raiseTo = [&](i64 target) -> Action {
        target = roundTo(target, step);
        target = std::clamp(target, l.minRaiseTo, l.maxRaiseTo);
        if (target >= l.maxRaiseTo) return {ALLIN, l.maxRaiseTo};
        return {RAISE, target};
    };
    float noise = r.uniform(-0.12f, 0.12f);
    bool pre = t.street == PREFLOP;
    int streetIdx = (int)t.street;
    if (l.canCheck) {
        float valueBar = 1.4f + st.tight * 0.3f;
        if (rel + noise > valueBar && l.canRaise && r.chance(0.6 + st.aggro * 0.35)) {
            if (pre) return raiseTo(t.currentBet + t.bb * (2 + r.range(0, 2)));
            return raiseTo(t.currentBet + (i64)(pot * r.uniform(0.5f, 0.9f)));
        }
        if (rel + noise > 1.1f && l.canRaise && r.chance(st.aggro * 0.7)) {
            if (pre) return raiseTo(t.currentBet + t.bb * 2);
            return raiseTo((i64)(pot * r.uniform(0.4f, 0.65f)));
        }
        // continuation bets and stabs when nobody showed interest
        if (l.canRaise && !pre && r.chance(st.bluff * (streetIdx == FLOP ? 2.2f : 1.4f)))
            return raiseTo((i64)(pot * r.uniform(0.45f, 0.75f)));
        return {CHECK, 0};
    }
    // somebody bet: their range is stronger than random cards
    if (!pre) eq *= (l.toCall * 2 > pot ? 0.78f : 0.86f);
    float potOdds = (float)l.toCall / (float)(pot + l.toCall);
    float margin = 0.03f + st.tight * 0.08f + streetIdx * 0.025f;
    bool allInCall = l.toCall >= s.stack;
    if (allInCall) {
        if (eq > potOdds + margin + 0.06f) return {CALL, 0};
        return {FOLD, 0};
    }
    if (rel + noise > 1.75f + st.tight * 0.2f && l.canRaise && r.chance(0.35 + st.aggro * 0.5)) {
        if (pre) return raiseTo(t.currentBet * 3);
        return raiseTo(t.currentBet * 2 + (i64)(pot * 0.5f));
    }
    if (eq + noise * 0.3f > potOdds + margin) return {CALL, 0};
    // cheap peeks preflop and the odd float
    if (pre && l.toCall <= t.bb && r.chance(0.5 - st.tight * 0.4)) return {CALL, 0};
    if (!pre && t.street != RIVER && l.toCall < pot / 3 && r.chance(st.bluff * 0.7)) return {CALL, 0};
    return {FOLD, 0};
}

} // namespace poker
