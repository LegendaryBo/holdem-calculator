#ifndef HOLDEM_HAND_H
#define HOLDEM_HAND_H

#include "simd.hpp"

/* Represents the rank of a card. */
enum Rank
{
    Rank_Duce = 0,
    Rank_3 = 1,
    Rank_4 = 2,
    Rank_5 = 3,
    Rank_6 = 4,
    Rank_7 = 5,
    Rank_8 = 6,
    Rank_9 = 7,
    Rank_10 = 8,
    Rank_J = 9,
    Rank_Q = 10,
    Rank_K = 11,
    Rank_Ace = 12,
};

/* Represents the suit of a card. */
enum Suit
{
    Suit_Club = 0,
    Suit_Diamond = 1,
    Suit_Heart = 2,
    Suit_Spade = 3
};

/* Represents a card. */
struct Card
{
    Rank rank;
    Suit suit;
    Card() : rank(), suit() { }
    Card(Rank _rank, Suit _suit) : rank(_rank), suit(_suit) { }
    Card(char _rank, char _suit);
};

/* Represents the category of a hand. */
enum HandCategory
{
    HighCard = 0,		/* HC */
    OnePair = 1,			/* 1P */
    TwoPair = 2,			/* 2P */
    ThreeOfAKind = 3,	/* 3K */
    Straight = 4,		/* ST */
    Flush = 5,			/* FL */
    FullHouse = 6,		/* FH */
    FourOfAKind = 7,		/* 4K */
    StraightFlush = 8	/* SF */
};

/**
 * Represents a hand, i.e. a subset of a deck of 52 cards.
 *
 * To maximize performance, we use a 16-byte SIMD vector to store the hand
 * internally. The format of the vector is as follows:
 *
 *    15  14  13  12  11                  ...                  1   0
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *   |S-H-D-C| 0 | A | K | Q | J | T | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 |
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *
 * Byte 0 to byte 12 stores information grouped by rank. Each byte has the
 * following format:
 *
 *     7   6   5   4   3   2   1   0
 *   +---+---+---+---+---+---+---+---+
 *   | 0 |   COUNT   | S | H | D | C |
 *   +---+---+---+---+---+---+---+---+
 *   
 * where the S, H, D, C bits are set if the cards of the corresponding rank
 * and suit is present in the hand, respectively. Bit 4 to 6 contains the
 * number of cards of the given rank in the hand. Bit 7 MUST be set to zero
 * because the underlying SIMD instruction only supports signed byte.
 *
 * Byte 13 is reserved and is always zero.
 *
 * Byte 14 to 15 is divided into four 4-bit groups to store the number of 
 * cards of each suit. Specifically, its format is as follows:
 *
 *    15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *   |  Spade-count  |  Heart-count  | Diamond-count |  Club-count   |
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 
 * The above format achieves two design goals to improve performance:
 * 1. Fast hand evaluation: a hand (with 5 or more cards) can be evaluated
 *    quickly using SIMD instructions on the representing vector.
 * 2. Fast hand construction: two hands can be combined quickly by directly
 *    adding up the two vectors that represent each hand.
 *
 * This format does enforce the restriction that no two idential cards can
 * be present in the same hand. This condition is certainly met when we draw
 * a hand from a single deck.
 */
struct Hand
{
    simd::simd_t<int8_t,16> value;

    Hand() { }
    Hand(const simd::simd_t<int8_t,16> &_value) : value(_value) { }
    Hand(const Card &card)
    {
        int r = card.rank;
        int s = card.suit;
        ((uint8_t*)&value)[r] += 0x10 | (1 << s);
        ((uint16_t*)&value)[7] += 1 << (s * 4);
    }
    Hand(const Card *cards, size_t num_cards) 
    {
        for (size_t i = 0; i < num_cards; i++)
        {
            int r = cards[i].rank;
            int s = cards[i].suit;
            ((uint8_t*)&value)[r] += 0x10 | (1 << s);
            ((uint16_t*)&value)[7] += 1 << (s * 4);
        }
    }
    Hand(const Hand *hands, size_t num_hands)
    {
        for (size_t i = 0; i < num_hands; i++)
            this->value += hands[i].value;
    }

    int GetCards(Card *cards) const;

    Hand& operator += (const Hand &a)
    {
        this->value += a.value;
        return *this;
    }

    Hand& operator -= (const Hand &a)
    {
        this->value -= a.value;
        return *this;
    }
};

/// Combines two hands.
inline Hand operator + (const Hand &a, const Hand &b)
{
    return Hand(a.value + b.value);
}

typedef unsigned int RankMask;

/**
 * Represents the strength of a 5-card hand. 
 *
 * The strength of a hand is composed of three parts -- hand category, master
 * cards, and side cards (kickers). A hand from a higher category is stronger
 * than a hand of from a lower category; for two hands from the same category,
 * the one with higher master cards is stronger than the one with lower master
 * cards; if the master cards are again equal, the kickers must be compared to
 * determine their relative strength.
 *
 * Internally the hand strength is stored in a 30-bit integer as follows:
 *
 *    29      26 25      13 12       0
 *   +---....---+---....---+---....---+
 *   | category |  master  |  kicker  |
 *   +---....---+---....---+---....---+
 *
 * Since card suit is only used to determine the hand category and is not 
 * significant for hands in the same category, it is not stored as part of
 * hand strength. Only the card ranks are stored (as a bit-map) for the master
 * card and the kicker.
 *
 * The above format ensures that the strengths of two hands can be compared
 * by simply comparing the representing integer.
 *
 * The actual 'master' and 'kicker' definition varies with the category, and
 * is listed below:
 *
 *   Category         Example  Master  Kicker
 *   ----------------------------------------
 *   Straight flush   QJT98    Q       -
 *   Four of a kind   7777K    7       K
 *   Full house       77788    7       8
 *   Flush            KT874    KT874   -
 *   Straight         A2345    5       -
 *   Three of a kind  888AA    8       A
 *   Two pair         TTQQA    TQ      A
 *   One pair         33789    3       789
 *   High card        97543    97543   -
 */
struct HandStrength
{
    uint32_t value;
    HandStrength() : value(0) { }
    HandStrength(HandCategory category, RankMask master, RankMask kicker)
        : value((category << 26) | (master << 13) | kicker) { }
    HandStrength(HandCategory category, RankMask master)
        : value((category << 26) | (master << 13)) { }
};

inline bool operator > (const HandStrength &a, const HandStrength &b)
{
    return a.value > b.value;
}

inline bool operator < (const HandStrength &a, const HandStrength &b)
{
    return a.value < b.value;
}

inline bool operator == (const HandStrength &a, const HandStrength &b)
{
    return a.value == b.value;
}

/**
 * Evaluates a hand of five to seven cards and returns the strength of the
 * strongest five-card combination.
 */
HandStrength EvaluateHand(const Hand &hand);

#if 0
/* Represents a hand of five cards. */
struct hand_t
{
    //unsigned char category; /* see hand_category_t */
    HandCategory category;
    card_t cards[5];
};

/**
 * Computes the category of a hand and rearrange the card sequence of the
 * hand so that two hands in the same category can be compared lexically to
 * determine their strength.
 */
hand_category_t normalize_hand(card_t cards[5]);

hand_t make_hand(const card_t cards[5]);
#endif

/// Gets the character that represents a given rank.
char format_rank(Rank rank);

void write_card(char s[3], Card card);

//int compare_hands(const hand_t &a, const hand_t &b);

//void write_hand(char s[20], const hand_t &h);

#if 0
inline bool operator < (const hand_t &a, const hand_t &b)
{
    return compare_hands(a, b) < 0;
}

inline bool operator > (const hand_t &a, const hand_t &b)
{
    return compare_hands(a, b) > 0;
}
#endif

#endif /* HOLDEM_HAND_H */
