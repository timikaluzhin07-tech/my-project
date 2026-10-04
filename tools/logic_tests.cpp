// Headless checks for the game rules (no SDL needed).
//   make -f Makefile.pc tests && ./build-pc/logic_tests
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>

#include "../source/games/blackjack_logic.h"
#include "../source/games/poker_logic.h"
#include "../source/games/roulette_logic.h"
#include "../source/games/slot_logic.h"

static int g_fail = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            g_fail++;                                                     \
        }                                                                 \
    } while (0)

static Card C(const char* s) {
    // "As", "Td", "9h"
    const char* ranks = "23456789TJQKA";
    int r = (int)(strchr(ranks, s[0]) - ranks) + 2;
    int suit = s[1] == 's' ? SPADES : s[1] == 'h' ? HEARTS : s[1] == 'd' ? DIAMONDS : CLUBS;
    return Card{(int8_t)r, (int8_t)suit};
}

static uint32_t E(std::initializer_list<const char*> cs) {
    std::vector<Card> v;
    for (auto c : cs) v.push_back(C(c));
    return poker::evaluate(v.data(), (int)v.size());
}

static void testEvaluator() {
    using namespace poker;
    CHECK(category(E({"As", "Ks", "Qs", "Js", "Ts", "2d", "3c"})) == STRAIGHT_FLUSH);
    CHECK(describe(E({"As", "Ks", "Qs", "Js", "Ts"})) == "Роял-флеш");
    CHECK(category(E({"5h", "4h", "3h", "2h", "Ah"})) == STRAIGHT_FLUSH);
    CHECK(category(E({"9c", "9d", "9h", "9s", "2d", "3c", "Kd"})) == QUADS);
    CHECK(category(E({"9c", "9d", "9h", "Ks", "Kd", "3c", "2d"})) == FULL_HOUSE);
    CHECK(category(E({"9c", "9d", "9h", "Ks", "Kd", "Kc", "2d"})) == FULL_HOUSE);
    CHECK(E({"9c", "9d", "9h", "Ks", "Kd", "Kc", "2d"}) > E({"Qc", "Qd", "Qh", "Js", "Jd", "2c", "3d"}));
    CHECK(category(E({"2h", "7h", "9h", "Jh", "Kh", "Ac", "Ad"})) == FLUSH);
    CHECK(category(E({"5c", "4d", "3h", "2s", "Ad", "Kc", "Kd"})) == STRAIGHT);
    CHECK((E({"5c", "4d", "3h", "2s", "Ad"}) >> 16 & 15) == 5); // wheel is a 5-high straight
    CHECK(E({"6c", "5d", "4h", "3s", "2d"}) > E({"5c", "4d", "3h", "2s", "Ad"}));
    CHECK(category(E({"Tc", "Td", "Th", "4s", "2d", "8c", "Kd"})) == TRIPS);
    CHECK(category(E({"Tc", "Td", "4h", "4s", "2d", "2c", "Kd"})) == TWO_PAIR);
    // two pair takes the best kicker among the remaining cards
    CHECK(E({"Tc", "Td", "4h", "4s", "2d", "2c", "Kd"}) > E({"Tc", "Td", "4h", "4s", "Qd", "2c", "3d"}));
    CHECK(category(E({"Ac", "Ad", "4h", "7s", "9d"})) == PAIR);
    CHECK(E({"Ac", "Ad", "Kh", "7s", "9d"}) > E({"Ac", "Ad", "Qh", "7s", "9d"}));
    CHECK(category(E({"Ac", "Jd", "4h", "7s", "9d"})) == HIGH_CARD);
    CHECK(describe(E({"Jc", "Jd", "4h", "7s", "9d"})) == "Пара валетов");
    CHECK(describe(E({"9c", "Td", "Jh", "Qs", "Kd"})) == "Стрит до короля");
    CHECK(describe(E({"5c", "5d", "5h", "7s", "7d"})) == "Фулл-хаус: три пятёрки и две семёрки");
    CHECK(describe(E({"Qc", "Qd", "Qh", "Ks", "Kd"})) == "Фулл-хаус: три дамы и два короля");
    CHECK(describe(E({"Jc", "Jd", "4h", "4s", "9d"})) == "Две пары: валеты и четвёрки");
    std::vector<Card> seven = {C("As"), C("Ks"), C("2d"), C("Qs"), C("Js"), C("3c"), C("Ts")};
    auto best = bestFive(seven.data(), 7);
    CHECK(best.size() == 5);
    CHECK(category(evaluate(best.data(), 5)) == STRAIGHT_FLUSH);
}

static void testBlackjack() {
    using namespace bj;
    CHECK(total({C("Ah"), C("6d")}).value == 17 && total({C("Ah"), C("6d")}).soft);
    CHECK(total({C("Ah"), C("6d"), C("Kc")}).value == 17 && !total({C("Ah"), C("6d"), C("Kc")}).soft);
    CHECK(total({C("Ah"), C("Ad"), C("9c")}).value == 21);
    CHECK(!dealerHits({C("Ah"), C("6d")})); // stands on soft 17
    CHECK(dealerHits({C("Th"), C("6d")}));
    Hand h;
    h.bet = 100;
    h.cards = {C("Ah"), C("Kd")};
    Outcome o;
    CHECK(settle(h, {C("9c"), C("8d")}, &o) == 250 && o == BLACKJACK);
    CHECK(settle(h, {C("Ac"), C("Qd")}, &o) == 100 && o == PUSH);
    h.cards = {C("Th"), C("Kd")};
    CHECK(settle(h, {C("Ac"), C("Qd")}, &o) == 0 && o == LOSE);
    CHECK(settle(h, {C("9c"), C("8d")}, &o) == 200 && o == WIN);
    CHECK(settle(h, {C("Tc"), C("Qd")}, &o) == 100 && o == PUSH);
    CHECK(settle(h, {C("Tc"), C("6d"), C("9h")}, &o) == 200 && o == WIN);
    h.cards = {C("Th"), C("Kd"), C("5c")};
    CHECK(settle(h, {C("Tc"), C("6d"), C("9h")}, &o) == 0 && o == BUST);
    h.cards = {C("Th"), C("6d")};
    h.surrendered = true;
    CHECK(settle(h, {C("Tc"), C("9d")}, &o) == 50 && o == SURRENDER);
    Hand split;
    split.bet = 100;
    split.fromSplit = true;
    split.cards = {C("Ah"), C("Kd")};
    CHECK(!split.blackjack());
    CHECK(settle(split, {C("9c"), C("8d")}, &o) == 200 && o == WIN);
    Hand pair;
    pair.cards = {C("8h"), C("8d")};
    CHECK(canSplit(pair, 1));
    pair.cards = {C("Kh"), C("Qd")};
    CHECK(canSplit(pair, 1));
    CHECK(!canSplit(pair, MAX_HANDS));
}

// Random self-play: every hand must end, chips must be conserved, stacks never negative.
static void testPokerSelfPlay() {
    using namespace poker;
    Rng r(777);
    rng().reseed(99);
    for (int game = 0; game < 40; game++) {
        Table t;
        int players = r.range(2, 6);
        for (int i = 0; i < players; i++) {
            t.seats[i].used = true;
            t.seats[i].bot = true;
            t.seats[i].style = r.range(0, 3);
            t.seats[i].stack = r.range(1, 60) * 50;
        }
        t.sb = 25;
        t.bb = 50;
        i64 chips = 0;
        for (auto& s : t.seats) chips += s.stack;
        for (int hand = 0; hand < 120 && t.seatedWithChips() >= 2; hand++) {
            CHECK(t.startHand());
            CHECK(t.totalChips() == chips);
            int guard = 0;
            while (true) {
                if (t.onlyOneLeft()) break;
                if (t.toAct < 0) {
                    if (t.street == RIVER) break;
                    t.nextStreet();
                    if (t.street == SHOWDOWN) break;
                    continue;
                }
                int i = t.toAct;
                Action a;
                if (r.chance(0.25)) a = botDecide(t, i, r);
                else {
                    int k = r.range(0, 9);
                    Legal l = t.legal(i);
                    if (k < 2) a = {FOLD, 0};
                    else if (k < 5) a = {l.canCheck ? CHECK : CALL, 0};
                    else if (k < 8) a = {RAISE, l.minRaiseTo + r.range(0, 4) * t.bb};
                    else a = {ALLIN, 0};
                }
                t.apply(i, a);
                CHECK(t.totalChips() == chips);
                for (auto& s : t.seats) CHECK(s.stack >= 0);
                if (++guard > 400) { CHECK(!"hand did not finish"); break; }
            }
            // pots must account for every chip bet this hand
            i64 potSum = 0;
            for (auto& p : t.pots()) potSum += p.amount;
            CHECK(potSum == t.potTotal());
            auto awards = t.finish();
            i64 after = 0;
            for (auto& s : t.seats) after += s.stack;
            CHECK(after == chips);
            CHECK(!awards.empty());
        }
    }
}

static void testSidePots() {
    using namespace poker;
    Table t;
    for (int i = 0; i < 3; i++) { t.seats[i].used = true; }
    t.seats[0].stack = 100;
    t.seats[1].stack = 300;
    t.seats[2].stack = 1000;
    t.sb = 5;
    t.bb = 10;
    t.button = 1; // startHand moves it to seat 2: seat 0 SB, seat 1 BB, seat 2 acts first
    // rig the hand: everyone all-in
    CHECK(t.startHand());
    int guard = 0;
    while (t.toAct >= 0 && guard++ < 20) t.apply(t.toAct, {ALLIN, 0});
    auto pots = t.pots();
    CHECK(pots.size() == 3);
    if (pots.size() == 3) {
        CHECK(pots[0].amount == 300 && pots[0].eligible.size() == 3);
        CHECK(pots[1].amount == 400 && pots[1].eligible.size() == 2);
        CHECK(pots[2].amount == 700 && pots[2].eligible.size() == 1); // uncalled excess returns to seat 2
    }
}

// Every roulette bet must return 36 units over the 37 pockets (house edge 1/37).
static void testRoulette() {
    using namespace roulette;
    auto spots = buildSpots(Grid());
    CHECK(spots.size() == 157);
    std::map<int, int> kinds;
    for (auto& sp : spots) {
        i64 total = 0;
        for (int n = 0; n <= 36; n++) total += settle(sp, 1, n);
        CHECK(total == 36);
        kinds[sp.kind]++;
    }
    CHECK(kinds[STRAIGHT] == 37);
    CHECK(kinds[SPLIT] == 60);
    CHECK(kinds[STREET] == 12);
    CHECK(kinds[CORNER] == 22);
    CHECK(kinds[SIXLINE] == 11);
    // the wheel holds every number exactly once
    std::vector<int> seen(37, 0);
    for (int i = 0; i < POCKETS; i++) seen[wheelOrder()[i]]++;
    for (int v : seen) CHECK(v == 1);
    // navigation never gets stuck on the layout
    int cur = 0;
    const float dirs[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    Rng r(9);
    for (int k = 0; k < 400; k++) {
        int d = r.range(0, 3);
        cur = navigate(spots, cur, dirs[d][0], dirs[d][1]);
    }
    CHECK(cur >= 0 && cur < (int)spots.size());
}

// Tumble bookkeeping must be consistent for the animation to replay it.
static void testSlot() {
    using namespace slot;
    Rng r(4242);
    Engine base;
    for (int i = 0; i < 20000; i++) {
        Spin s = base.play(100, r, i % 500 == 0);
        CHECK(s.total >= 0 && s.total <= 100 * MAX_WIN_X);
        if (i % 500 == 0) CHECK(s.scatters >= 4 && s.freeSpins == FS_AWARD);
        i64 sum = 0;
        for (auto& st : s.steps) {
            sum += st.win;
            for (int c = 0; c < COLS; c++) {
                int hits = 0;
                for (int row = 0; row < ROWS; row++) hits += st.hit[c][row];
                CHECK(st.fresh[c] == hits);
                for (int row = 0; row < ROWS; row++) {
                    if (row < st.fresh[c]) continue;
                    // survivor: the cell it came from is a non-hit cell `fall` rows above
                    int from = row - st.fall[c][row];
                    CHECK(from >= 0 && !st.hit[c][from]);
                    CHECK(st.before[c][from].s == st.after[c][row].s);
                }
            }
        }
        CHECK(sum == s.tumbleWin);
    }
}

int main() {
    testRoulette();
    testSlot();
    testEvaluator();
    testBlackjack();
    testSidePots();
    testPokerSelfPlay();
    if (g_fail) {
        printf("%d check(s) failed\n", g_fail);
        return 1;
    }
    printf("all logic tests passed\n");
    return 0;
}
