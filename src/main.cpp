#include <iostream>
#include <random>
#include <functional>
#include "hand.h"
#include <algorithm>
#include <stdint.h>
#include <cassert>

#if 0
extern void test();
extern void test2();
#endif

#if 0
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
#endif

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
int compute_hole_index(const Card cards[2])
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
		double odds(int num_players) const 
		{
			return (double)(num_occur[num_players] - num_win[num_players])
                / num_win[num_players];
		}
	};
	hole_stat_t stat[HOLE_CARD_COMBINATIONS];
	for (int i = 0; i < HOLE_CARD_COMBINATIONS; i++)
	{
		format_hole_index(stat[i].type, i);
		memset(stat[i].num_occur, 0, sizeof(stat[i].num_occur));
		memset(stat[i].num_win, 0, sizeof(stat[i].num_win));
	}

	// Initialize a deck of cards.
	Card deck[52];
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
        HandStrength win_strength;
		Card win_hole[2];

		// Use the first five cards as community cards.
		Card c[7];
		std::copy(deck + 0, deck + 5, c);

		// Use each of the next two cards as hole cards for the players.
		for (int j = 0; j < num_players; j++)
		{
			c[5] = deck[5 + j * 2];
			c[6] = deck[5 + j * 2 + 1];

			// Update the occurrence of this combination of hole cards.
			stat[compute_hole_index(&c[5])].num_occur[j]++;

			// Find the best 5-card combination from these 7 cards.
            HandStrength strength = EvaluateHand(Hand(c, 7));

			// Update the winning hand statistics for a game with j+1 players.
			if (j == 0 || strength > win_strength)
			{
                win_strength = strength;
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
		printf("%5c", format_rank((Rank)r2));
	}
	printf("\n");
	for (int r1 = Rank_Ace; r1 >= Rank_Duce; r1--)
	{
		// Label
		printf("%c: ", format_rank((Rank)r1));

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
	simulate(8, 1000000);
#endif
	return 0;
}
