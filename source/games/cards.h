// Playing cards and shoes — pure logic shared by blackjack and poker.
#pragma once

#include "../core/common.h"

enum Suit : int8_t { SPADES = 0, HEARTS = 1, DIAMONDS = 2, CLUBS = 3 };

struct Card {
    int8_t rank = 0; // 2..14 (11 J, 12 Q, 13 K, 14 A); 0 = none
    int8_t suit = 0;
    bool valid() const { return rank >= 2 && rank <= 14; }
    bool red() const { return suit == HEARTS || suit == DIAMONDS; }
    bool operator==(const Card& o) const { return rank == o.rank && suit == o.suit; }
    int index() const { return (rank - 2) * 4 + suit; } // 0..51
};

inline const char* rankLabel(int r) {
    static const char* L[] = {"", "", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"};
    return (r >= 2 && r <= 14) ? L[r] : "?";
}

// Russian names used in hand descriptions ("пара валетов").
inline const char* rankNameGenitivePlural(int r) {
    static const char* N[] = {"", "", "двоек", "троек", "четвёрок", "пятёрок", "шестёрок", "семёрок", "восьмёрок",
                              "девяток", "десяток", "валетов", "дам", "королей", "тузов"};
    return (r >= 2 && r <= 14) ? N[r] : "";
}
inline const char* rankNameNominative(int r) {
    static const char* N[] = {"", "", "двойка", "тройка", "четвёрка", "пятёрка", "шестёрка", "семёрка", "восьмёрка",
                              "девятка", "десятка", "валет", "дама", "король", "туз"};
    return (r >= 2 && r <= 14) ? N[r] : "";
}

class Shoe {
public:
    explicit Shoe(int decks = 1, float penetration = 1.f) : decks_(decks), penetration_(penetration) { refill(); }

    void refill() {
        cards_.clear();
        for (int d = 0; d < decks_; d++)
            for (int r = 2; r <= 14; r++)
                for (int s = 0; s < 4; s++) cards_.push_back(Card{(int8_t)r, (int8_t)s});
        shuffle();
    }
    void shuffle() {
        for (int i = (int)cards_.size() - 1; i > 0; i--) std::swap(cards_[i], cards_[rng().range(0, i)]);
        pos_ = 0;
        cut_ = (int)(cards_.size() * penetration_);
    }
    Card draw() {
        if (pos_ >= (int)cards_.size()) shuffle();
        return cards_[pos_++];
    }
    // True once the cut card has come out — reshuffle before the next round.
    bool needsShuffle() const { return pos_ >= cut_; }
    int remaining() const { return (int)cards_.size() - pos_; }
    int total() const { return (int)cards_.size(); }
    // Removes specific cards (used to build the stub deck for poker equity sims).
    void remove(const std::vector<Card>& used) {
        cards_.erase(std::remove_if(cards_.begin(), cards_.end(),
                                    [&](const Card& c) { return std::find(used.begin(), used.end(), c) != used.end(); }),
                     cards_.end());
    }
    std::vector<Card>& raw() { return cards_; }

private:
    int decks_;
    float penetration_;
    std::vector<Card> cards_;
    int pos_ = 0;
    int cut_ = 0;
};
