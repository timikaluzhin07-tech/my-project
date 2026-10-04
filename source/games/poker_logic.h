// No-Limit Texas Hold'em engine: hand evaluation, betting rounds, side pots, bot AI.
#pragma once

#include "cards.h"

namespace poker {

constexpr int SEATS = 6;

enum Category { HIGH_CARD, PAIR, TWO_PAIR, TRIPS, STRAIGHT, FLUSH, FULL_HOUSE, QUADS, STRAIGHT_FLUSH };

// Best five-card score of 5..7 cards; higher is better.
uint32_t evaluate(const Card* cards, int n);
inline Category category(uint32_t score) { return (Category)(score >> 20); }
// Russian description: "Пара валетов", "Фулл-хаус: дамы и семёрки", ...
std::string describe(uint32_t score);
std::string categoryName(Category c);
// The five cards that make the best hand (for highlighting).
std::vector<Card> bestFive(const Card* cards, int n);

enum Street { PREFLOP, FLOP, TURN, RIVER, SHOWDOWN };
enum ActType { FOLD, CHECK, CALL, RAISE, ALLIN };

struct Action {
    ActType type = CHECK;
    i64 to = 0; // for RAISE: total street bet after the action
};

struct Legal {
    bool canCheck = false;
    i64 toCall = 0;
    bool canRaise = false;
    i64 minRaiseTo = 0;
    i64 maxRaiseTo = 0;
};

struct Seat {
    bool used = false;
    bool bot = false;
    int profile = -1;    // human profile index
    int order = 0;       // human seat order (controller)
    std::string name;
    int color = 0;
    int style = 0;       // bot personality
    i64 stack = 0;
    Card hole[2];
    bool inHand = false, folded = false, allIn = false;
    i64 streetBet = 0, totalBet = 0;
    bool acted = false;
    int actedRaiseId = -1;
    std::string lastAction;
    bool live() const { return inHand && !folded; }
    bool canAct() const { return inHand && !folded && !allIn; }
};

struct Pot {
    i64 amount = 0;
    std::vector<int> eligible;
};

struct Award {
    int seat;
    i64 amount;
    int pot;
};

class Table {
public:
    Seat seats[SEATS];
    int button = -1;
    i64 sb = 25, bb = 50;
    std::vector<Card> board;
    Street street = PREFLOP;
    int toAct = -1;
    i64 currentBet = 0;
    i64 lastRaise = 0;
    int raiseId = 0;
    int handNo = 0;
    int sbSeat = -1, bbSeat = -1;

    int seatedWithChips() const;
    // Posts blinds and deals hole cards. False if fewer than two players can play.
    bool startHand();
    Legal legal(int seat) const;
    void apply(int seat, Action a);
    // True when the hand ended because everyone else folded.
    bool onlyOneLeft() const;
    // Betting on this street is finished (move to next street / showdown).
    bool roundDone() const;
    // Number of players that can still bet (not folded / all-in).
    int canActCount() const;
    // Advances to the next street, dealing community cards. Returns new street.
    Street nextStreet();
    i64 potTotal() const;
    std::vector<Pot> pots() const;
    // Splits all pots between winners and pays them. Uncontested hands too.
    std::vector<Award> finish();
    int nextSeat(int from, bool needChips) const;
    // All chips on the table (stacks + bets) — must stay constant during a hand.
    i64 totalChips() const;

private:
    Shoe deck_{1, 1.f};
    int nextToAct(int from) const;
};

// Monte-Carlo win probability against `opponents` random hands.
float equity(const Card hole[2], const std::vector<Card>& board, int opponents, int iterations, Rng& r);

struct BotStyle {
    const char* label;
    float tight;  // higher folds more
    float aggro;  // higher bets/raises more
    float bluff;  // bluff frequency
};
const BotStyle& botStyle(int idx);
constexpr int BOT_STYLES = 4;

Action botDecide(const Table& t, int seat, Rng& r);

} // namespace poker
