#include <iostream>
#include <algorithm>
#include "hand.h"

static const char rank_s[] = { '2','3','4','5','6','7','8','9','T','J','Q','K','A' };
static const char suit_s[] = { 'C','D','H','S' };

char format_rank(Rank rank)
{
	return rank_s[rank];
}

card_t read_card(char rank, char suit)
{
	card_t c;
	c.rank = (unsigned char)(std::find(rank_s+0, rank_s+sizeof(rank_s), rank) - rank_s);
	c.suit = (unsigned char)(std::find(suit_s+0, suit_s+sizeof(suit_s), suit) - suit_s);
	return c;
}

void write_card(char s[3], card_t card)
{
	static const char suit_s2[] = { '\x05', '\x04', '\x03', '\x06' };
	s[0] = rank_s[card.rank];
	s[1] = suit_s2[card.suit];
	s[2] = 0;
}

void write_hand(char s[19], const hand_t &h)
{
	for (int i = 0; i < 5; i++)
	{
		write_card(s + 3 * i, h.cards[i]);
		s[3 * i + 2] = ' ';
	}
	static const char *cat_s[] = { "HC", "1P", "2P", "3K", "ST", "FL", "FH", "4K", "SF" };
	s[15] = '(';
	s[16] = cat_s[h.category][0];
	s[17] = cat_s[h.category][1];
	s[18] = ')';
	s[19] = 0;
}

hand_category_t normalize_hand(card_t cards[5])
{
	// Count the number of occurrences of each rank in the hand.
	unsigned char count[13] = { 0 }, max_count = 0;
	for (int i = 0; i < 5; i++)
	{
		max_count = std::max(max_count, ++count[cards[i].rank]);
	}
	
	// Sort the cards by the number of occurrences of the rank. For ranks with
	// the same number of occurrences, sort them by rank (descending).
	std::sort(cards + 0, cards + 5, [&count](card_t a, card_t b) -> bool 
	{
		return (count[a.rank] > count[b.rank]) ||
			(count[a.rank] == count[b.rank] && a.rank > b.rank);
	});

	// Check whether the hand is a flush.
	bool is_flush = std::all_of(cards+1, cards+5, [cards](card_t c) -> bool {
		return c.suit == cards[0].suit;
	});

	// Check whether the hand is straight. Note the special case A,2,3,4,5.
	bool is_straight = (max_count == 1) && 
		((cards[0].rank == cards[4].rank + 4) || 
		 (cards[0].rank == Rank_Ace && 
		  cards[1].rank == Rank_5 &&
		  cards[4].rank == Rank_Duce));
	
	// In case of A,2,3,4,5 flush, we need to rearrange the high card.
	if (is_straight && cards[0].rank == Rank_Ace)
		std::rotate(cards + 0, cards + 1, cards + 5);

	if (is_flush && is_straight)
		return StraightFlush;
	if (cards[0].rank == cards[3].rank)
		return FourOfAKind;
	if (cards[0].rank == cards[2].rank && cards[3].rank == cards[4].rank)
		return FullHouse;
	if (is_flush)
		return Flush;
	if (is_straight)
		return Straight;
	if (cards[0].rank == cards[2].rank)
		return ThreeOfAKind;
	if (cards[0].rank == cards[1].rank && cards[2].rank == cards[3].rank)
		return TwoPair;
	if (cards[0].rank == cards[1].rank)
		return OnePair;
	return HighCard;
}

hand_t make_hand(const card_t cards[5])
{
	hand_t h;
	memcpy(h.cards, cards, sizeof(h.cards));
	h.category = normalize_hand(h.cards);
	return h;
}

int compare_hands(const hand_t &a, const hand_t &b)
{
	if (a.category < b.category)
		return -1;
	if (a.category > b.category)
		return 1;

	for (int i = 0; i < 5; i++)
	{
		if (a.cards[i].rank < b.cards[i].rank)
			return -1;
		if (a.cards[i].rank > b.cards[i].rank)
			return 1;
	}
	return 0;
}

#if 0
static void solve_problem_54()
{
	int lineno = 0;
	int count = 0;
	std::ifstream fs("input/poker.txt");
	while (true)
	{
		char s[50];
		fs.getline(s, sizeof(s));
		if (s[0] == 0)
			break;

		lineno++;
		card_t cards[10];
		for (int i = 0; i < 10; i++)
		{
			cards[i] = read_card(s[3*i], s[3*i+1]);
		}
		
		int winner = compare_hands(&cards[0], &cards[5]);
#if 0
		cout << lineno << "   ";
		if (winner > 0)
			cout << "Player 1 wins" << endl;
		else if (winner < 0)
			cout << "Player 2 wins" << endl;
		else
			cout << "Tie";
#endif

		if (winner > 0)
			++count;
	}

	//cout << "Player 1 wins " << count << " hands." << endl;
	std::cout << count << std::endl;
}
#endif