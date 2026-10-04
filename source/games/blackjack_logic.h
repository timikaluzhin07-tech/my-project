// Blackjack rules: 6 decks, dealer stands on soft 17, blackjack pays 3:2,
// double on any two cards, split up to four hands, late surrender, insurance 2:1.
#pragma once

#include "cards.h"

namespace bj {

constexpr int DECKS = 6;
constexpr i64 MIN_BET = 10;
constexpr i64 MAX_BET = 5000;
constexpr int MAX_HANDS = 4;

inline int cardValue(int rank) { return rank == 14 ? 11 : (rank >= 10 ? 10 : rank); }

struct Total {
    int value = 0;
    bool soft = false; // an ace is still counted as 11
};

inline Total total(const std::vector<Card>& cards) {
    int sum = 0, aces = 0;
    for (const Card& c : cards) {
        sum += cardValue(c.rank);
        if (c.rank == 14) aces++;
    }
    while (sum > 21 && aces > 0) { sum -= 10; aces--; }
    return {sum, aces > 0};
}

struct Hand {
    std::vector<Card> cards;
    i64 bet = 0;
    bool doubled = false;
    bool fromSplit = false;
    bool splitAces = false;
    bool surrendered = false;
    bool done = false;

    Total tot() const { return total(cards); }
    bool bust() const { return tot().value > 21; }
    bool blackjack() const { return cards.size() == 2 && !fromSplit && tot().value == 21; }
};

inline bool isBlackjack(const std::vector<Card>& c) { return c.size() == 2 && total(c).value == 21; }

// Dealer draws to 16 and stands on every 17, soft ones included.
inline bool dealerHits(const std::vector<Card>& dealer) { return total(dealer).value < 17; }

inline bool canSplit(const Hand& h, int handCount) {
    return h.cards.size() == 2 && handCount < MAX_HANDS && !h.splitAces &&
           cardValue(h.cards[0].rank) == cardValue(h.cards[1].rank);
}
inline bool canDouble(const Hand& h) { return h.cards.size() == 2 && !h.splitAces && !h.done; }
inline bool canSurrender(const Hand& h, int handCount) { return h.cards.size() == 2 && handCount == 1 && !h.fromSplit; }

enum Outcome { LOSE, PUSH, WIN, BLACKJACK, SURRENDER, BUST };

// Amount returned to the player for a finished hand (stake included).
inline i64 settle(const Hand& h, const std::vector<Card>& dealer, Outcome* out) {
    Outcome o;
    i64 ret = 0;
    int d = total(dealer).value;
    bool dealerBJ = isBlackjack(dealer);
    if (h.surrendered) { o = SURRENDER; ret = h.bet / 2; }
    else if (h.bust()) { o = BUST; ret = 0; }
    else if (h.blackjack()) {
        if (dealerBJ) { o = PUSH; ret = h.bet; }
        else { o = BLACKJACK; ret = h.bet + h.bet * 3 / 2; }
    } else if (dealerBJ) { o = LOSE; ret = 0; }
    else {
        int p = h.tot().value;
        if (d > 21 || p > d) { o = WIN; ret = h.bet * 2; }
        else if (p == d) { o = PUSH; ret = h.bet; }
        else { o = LOSE; ret = 0; }
    }
    if (out) *out = o;
    return ret;
}

inline std::string totalLabel(const std::vector<Card>& cards) {
    Total t = total(cards);
    if (cards.size() == 2 && t.value == 21) return "21";
    if (t.soft && t.value < 21) return std::to_string(t.value - 10) + " / " + std::to_string(t.value);
    return std::to_string(t.value);
}

} // namespace bj
