#include <iostream>
#include <random>
#include <functional>
#include "hand.h"
#include <algorithm>
#include "simd.hpp"
#include <stdint.h>
#include "intrinsic.hpp"
#include <cassert>

extern void test();
extern void test2();

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

};

/// Combines two hands.
Hand operator + (const Hand &a, const Hand &b)
{
    return Hand(a.value + b.value);
}

/// Returns an integer with the highest N set bits of x kept.
int KeepHighestBitsSet(int x, int N)
{
    int result = 0;
    for (int i = 0; i < N && x; i++)
    {
        int b = intrinsic::bit_scan_reverse(x);
        result |= (1 << b);
        x &= ~result;
    }
    return result;
}

int KeepHighestBitSet(int x)
{
    return (x)? (1 << intrinsic::bit_scan_reverse(x)) : 0;
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
HandStrength EvaluateHand(const Hand &hand)
{
    // Compute a mask of the ranks present in the hand.
    int ranks_present = simd::GetMask(hand.value > 0x10) & 0x1FFF;
    int ranks_paired = simd::GetMask(hand.value > 0x20) & 0x1FFF;
    int ranks_3_of_a_kind = simd::GetMask(hand.value > 0x30) & 0x1FFF;

    // Compute a mask of the flushed suit. (For seven or fewer cards, there
    // can be at most one flushed suit.) The number of cards of each suit
    // are stored in the four nibbles (4-bit integers) of the highest two 
    // bytes in the hand's vector. To have five to seven cards of the same 
    // suit (5 <= x <= 7), we have equivalently 8 <= x+3 <= 10. We can easily
    // check for this condition by testing the highest bit of each nibble.
    int suits_count = simd::extract<7>(simd::simd_t<uint16_t,8>(hand.value));
    int suits_flushed = (suits_count + 0x3333) & 0x8888;
    int ranks_flushed = 0;
    if (suits_flushed)
    {
        int m = suits_flushed;
        int8_t suits_mask = ((m >> 3) | (m >> 6) | (m >> 9) | (m >> 12)) & 0xF;
        ranks_flushed = simd::GetMask((hand.value & suits_mask) > 0) & 0x1FFF;
    }

    // Check for straight flush.
    if (suits_flushed)
    {
        // Check for straight within the flushed suit. This is automatic when
        // there are exactly 5 cards; but when there are more than 5 cards, 
        // straight and flush may not imply straight-flush. The algorithm to 
        // check for straights is detailed later.
        int m = (ranks_flushed << 1) | (ranks_flushed >> 12);
        int mask_straight = (m & (m << 1) & (m << 2) & (m << 3) & (m << 4)) >> 1;
        if (mask_straight)
        {
            return HandStrength(StraightFlush, KeepHighestBitSet(mask_straight));
        }
    }

    // Check for four-of-a-kind. For seven or fewer cards, there can be 
    // at most one four-of-a-kind combination.
    int ranks_4_of_a_kind = simd::GetMask(hand.value > 0x40) & 0x1FFF;
    if (ranks_4_of_a_kind)
    {
        RankMask master = KeepHighestBitSet(ranks_4_of_a_kind);
        RankMask kicker = KeepHighestBitSet(ranks_present & ~master);
        return HandStrength(FourOfAKind, master, kicker);
    }

    // Check for full house.
    // Note that for 7 cards, there may be two possible three-of-a-kind...
    if (ranks_3_of_a_kind)
    {
        RankMask master = KeepHighestBitSet(ranks_3_of_a_kind);
        if (ranks_paired & ~master)
        {
            RankMask kicker = KeepHighestBitSet(ranks_paired & ~master);
            return HandStrength(FullHouse, master, kicker);
        }
    }

    // Check for flush.
    if (ranks_flushed)
    {
        RankMask master = KeepHighestBitsSet(ranks_flushed, 5);
        return HandStrength(Flush, master);
    }

    // Check for straights. We first copy the Ace-bit to the lowest bit so 
    // that we can easily test for 5-4-3-2-1 straight. Then a straight is 
    // such that there are five consecutive bits set in the mask. This can 
    // be tested by shift-and the mask five times. The highest bit left set
    // is the best straight. If no bit is left set, there are no straights.
    int m = (ranks_present << 1) | (ranks_present >> 12);
    int mask_straight = (m & (m << 1) & (m << 2) & (m << 3) & (m << 4)) >> 1;
    if (mask_straight)
    {
        return HandStrength(Straight, KeepHighestBitSet(mask_straight));
    }

    // Check for three-of-a-kind.
    // Note that for 7 cards, there may be two possible three-of-a-kind...
    if (ranks_3_of_a_kind)
    {
        RankMask master = KeepHighestBitSet(ranks_3_of_a_kind);
        RankMask kicker = KeepHighestBitsSet(ranks_present & ~master, 2);
        return HandStrength(ThreeOfAKind, master, kicker);
    }

    // Check for two-pair and one pair.
    if (ranks_paired)
    {
        RankMask master = KeepHighestBitSet(ranks_paired);
        ranks_paired &= ~master;
        if (ranks_paired) // two-pair
        {
            master |= KeepHighestBitSet(ranks_paired);
            RankMask kicker = KeepHighestBitSet(ranks_present & ~master);
            return HandStrength(TwoPair, master, kicker);
        }
        else // one pair
        {
            RankMask kicker = KeepHighestBitsSet(ranks_present & ~master, 3);
            return HandStrength(OnePair, master, kicker);
        }
    }

    // Now we are left with high card.
    return HandStrength(HighCard, KeepHighestBitsSet(ranks_present, 5));
}

/// Finds the best combination of 5 cards out of 7 cards.
hand_t find_best_hand(const card_t cards[7], HandStrength *strength_output)
{
#if 1
    // New method: try it out!
    Hand hand(cards, 7);
    HandStrength strength = EvaluateHand(hand);
    *strength_output = strength;
#endif

	card_t choice[5];
	hand_t best;

	// Exclude each pair of cards (i, j), and evaluate the rest.
	for (int i = 0; i < 7; i++)
	{
		for (int j = i + 1; j < 7; j++)
		{
			int ii = 0;
			for (int k = 0; k < 7; k++)
				if (k != i && k != j)
					choice[ii++] = cards[k];

			hand_t h = make_hand(choice);
			if (j == 1 || h > best)
				best = h;
		}
	}
	return best;
}

#define HOLE_CARD_COMBINATIONS 169

/// Computes an index for two hole cards, only accounting for rank and 
/// suited-ness. The index is computed as follows:
/// 23s, 23o, 24s, 24o, ..., 2As, 2Ao
/// 34s, 34o, 35s, 35o, ..., 3As, 3Ao
/// ...
/// KAs, KAo
/// 22, 33, ..., AA
/// There are in total 2*(12+11+...+1)+13=169 combinations.
/// This can also be viewed as drawing two cards (a,b) from 1..13 randomly.
/// If a = b, this maps to a pair (off-suit); if a < b, this maps to off-suit;
/// if a > b, this maps to same-suit.
int compute_hole_index(const card_t cards[2])
{
	int r1 = cards[0].rank, r2 = cards[1].rank;
	if (r1 > r2) // make sure r1 <= r2
		std::swap(r1, r2);

	if (cards[0].suit == cards[1].suit) // same-suit
		return r2 * 13 + r1;
	else
		return r1 * 13 + r2;
}

void format_hole_index(char s[4], int index)
{
	int r1 = index / 13, r2 = index % 13;
	s[0] = format_rank((Rank)std::max(r1, r2));
	s[1] = format_rank((Rank)std::min(r1, r2));
	s[2] = (r1 == r2)? ' ' : (r1 > r2)? 's' : 'o';
	s[3] = 0;
}

#define MAX_PLAYERS 10

// Run a Monte-Carlo simulation of a game with 6 players.
// Simulate 1,000,000 games.
// Record the winning hole cards of each game.
// Then count the winning frequency of each hole cards.
void simulate(int num_players, int num_simulations)
{
	std::mt19937 engine;
	//std::uniform_int_distribution<int> d(0, 51);
	auto gen = [&](int n) -> int {
		return std::uniform_int_distribution<int>(0, n - 1)(engine);
	};

	// Keep track of the number of occurrences and winning of each combination
	// of hole cards.
	struct hole_stat_t
	{
		char type[4];  // e.g. "AKs"
		int num_occur[MAX_PLAYERS]; // number of occurrence given n opponents
		int num_win[MAX_PLAYERS];   // number of winning given n opponents
		//double odds(int num_players) const 
		//{
		//	return (double)(num_occur[num_players] - num_win) / num_win;
		//}
	};
	hole_stat_t stat[HOLE_CARD_COMBINATIONS];
	for (int i = 0; i < HOLE_CARD_COMBINATIONS; i++)
	{
		format_hole_index(stat[i].type, i);
		memset(stat[i].num_occur, 0, sizeof(stat[i].num_occur));
		memset(stat[i].num_win, 0, sizeof(stat[i].num_win));
	}

	// Initialize a deck of cards.
	card_t deck[52];
	for (int i = 0; i < 52; i++)
	{
		deck[i].rank = (Rank)(i / 4);
		deck[i].suit = (Suit)(i % 4);
	}

	for (int i = 0; i < num_simulations; i++)
	{
		// Shuffle the deck.
		std::random_shuffle(deck + 0, deck + 52, gen);

		// Store the winning hand and hole cards.
		hand_t win_hand;
		card_t win_hole[2];

		// Use the first five cards as community cards.
		card_t c[7];
		std::copy(deck + 0, deck + 5, c);

		// Use each of the next two cards as hole cards for the players.
		for (int j = 0; j < num_players; j++)
		{
			c[5] = deck[5 + j * 2];
			c[6] = deck[5 + j * 2 + 1];

			// Update the occurrence of this combination of hole cards.
			stat[compute_hole_index(&c[5])].num_occur[j]++;

			// Find the best hand.
            HandStrength strength;
			hand_t best = find_best_hand(c, &strength);
            if (j > 0)
            {
                // Check the new algorithm.
                int xx = compare_hands(best, win_hand);
                HandStrength win_strength = EvaluateHand(Hand(win_hand.cards, 5));
                int yy = (strength > win_strength)? 1 :
                    (strength < win_strength)? -1 : 0;
                assert(xx == yy && "New algorithm is wrong!");
                if (xx != yy)
                {
                    printf("xx = %d, yy = %d\n", xx, yy);
                    getc(stdin);
                }
            }

			// Update the winning hand statistics for a game with j+1 players.
			if (j == 0 || best > win_hand)
			{
				win_hand = best;
				win_hole[0] = c[5];
				win_hole[1] = c[6];
				stat[compute_hole_index(win_hole)].num_win[j]++;
			}
		}

		// Note that we do not process tie here. This needs to be fixed.
	}

#if 1
	printf("r1 r2 s Hole");
	for (int n = 2; n <= num_players; n++)
	{
		printf(" %6d", n);
	}
	printf("\n");
	for (int hole = 0; hole < 169; hole++)
	{
		char t = stat[hole].type[2];
		printf("%2d %2d %c %s ", hole / 13, hole % 13, 
			(t == ' ')? 'p' : t, stat[hole].type);
		for (int n = 2; n <= num_players; n++)
		{
			double prob = (double)stat[hole].num_win[n-1] / stat[hole].num_occur[n-1];
			printf(" %.4lf", prob);
		}
		printf("\n");
	}
	//printf("----------------\n");
#endif

#if 0
	// Display a rectangular table.
	printf("O\\S");
	for (int r2 = Rank_Ace; r2 >= Rank_Duce; r2--)
	{
		printf("%5c", format_rank((rank_t)r2));
	}
	printf("\n");
	for (int r1 = Rank_Ace; r1 >= Rank_Duce; r1--)
	{
		// Label
		printf("%c: ", format_rank((rank_t)r1));

		// Off suit (r1, r2) for r1 <= r2.
		for (int r2 = Rank_Ace; r2 >= Rank_Duce; r2--)
		{
			int index = r1 * 13 + r2;
			printf((r1 == r2)? " *%3.1lf" : "%5.1lf", stat[index].odds());
		}

		// Pair (r1, r1).

		// Same-suit (r1, r2) for r1 > r2.
		printf("\n");
	}
#endif

#if 0
	// Sort the statistics by winning count (strongest hand first).
	std::sort(stat, stat + HOLE_CARD_COMBINATIONS,
		[](const hole_stat_t &s1, const hole_stat_t &s2) -> bool 
	{
		double p1 = (double)s1.num_win / (s1.num_occur + 0.0001);
		double p2 = (double)s2.num_win / (s2.num_occur + 0.0001);
		return p1 > p2;
	});
	
	// Display statistics.
	for (int i = 0; i < HOLE_CARD_COMBINATIONS; i++)
	{
		double percentage = 100.0 * stat[i].num_win / (stat[i].num_occur + 0.0001);
		// printf("%s %.2lf%%\n", stat[i].type, percentage);
		printf("%s %.1lf\n", stat[i].type, (100-percentage)/percentage);
	}
#endif

}

int main()
{
#if _DEBUG
	simulate(4, 10000);
#else
	simulate(8, 100000);
#endif
	return 0;
}
