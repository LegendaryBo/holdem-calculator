#ifndef HOLDEM_HAND_H
#define HOLDEM_HAND_H

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
struct card_t
{
	unsigned char rank; /* see rank_t */
	unsigned char suit; /* see suit_t */
};

/*
static const char value_s[] = { '2','3','4','5','6','7','8','9','T','J','Q','K','A' };
static const char suit_s[] = { 'S','H','D','C' };
*/

/*
static const char value_s[] = { '2','3','4','5','6','7','8','9','T','J','Q','K','A' };
static const char suit_s[] = { 'S','H','D','C' };
*/

/* Represents the category of a hand. */
enum hand_category_t
{
	HighCard,		/* HC */
	OnePair,			/* 1P */
	TwoPair,			/* 2P */
	ThreeOfAKind,	/* 3K */
	Straight,		/* ST */
	Flush,			/* FL */
	FullHouse,		/* FH */
	FourOfAKind,		/* 4K */
	StraightFlush	/* SF */
};

/* Represents a hand of five cards. */
struct hand_t
{
	unsigned char category; /* see hand_category_t */
	card_t cards[5];
};

/**
 * Computes the category of a hand and rearrange the card sequence of the
 * hand so that two hands in the same category can be compared lexically to
 * determine their strength.
 */
hand_category_t normalize_hand(card_t cards[5]);

hand_t make_hand(const card_t cards[5]);

/// Gets the character that represents a given rank.
char format_rank(Rank rank);

card_t read_card(char rank, char suit);

void write_card(char s[3], card_t card);

int compare_hands(const hand_t &a, const hand_t &b);

void write_hand(char s[20], const hand_t &h);

inline bool operator < (const hand_t &a, const hand_t &b)
{
	return compare_hands(a, b) < 0;
}

inline bool operator > (const hand_t &a, const hand_t &b)
{
	return compare_hands(a, b) > 0;
}

/*
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
*/

#endif /* HOLDEM_HAND_H */
