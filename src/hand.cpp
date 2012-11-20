#include <iostream>
#include <algorithm>
#include "hand.h"
#include <cassert>
#include <cctype>
#include "intrinsic.hpp"

static const char rank_s[13] = { '2','3','4','5','6','7','8','9','T','J','Q','K','A' };
static const char suit_s[8] = { 'C','D','H','S', '\x05', '\x04', '\x03', '\x06' };

Card::Card(char _rank, char _suit)
{
    int r = std::find(rank_s + 0, rank_s + 13, std::toupper(_rank)) - rank_s;
    int s = std::find(suit_s + 0, suit_s + 4, std::toupper(_suit)) - suit_s;
    assert(r < 13 && s < 8);
    this->rank = (Rank)r;
    this->suit = (Suit)(s % 4);
}

char format_rank(Rank rank)
{
	return rank_s[rank];
}

void write_card(char s[3], Card card)
{
	s[0] = rank_s[card.rank];
	s[1] = suit_s[card.suit + 4];
	s[2] = 0;
}

int Hand::GetCards(Card *cards) const
{
    int n = 0;
    RankMask ranks_present = simd::GetMask(value > 0x10) & 0x1FFF;
    while (ranks_present)
    {
        int r = intrinsic::bit_scan_reverse(ranks_present);
        int8_t b = ((int8_t*)&value)[r] & 0xF;
        while (b)
        {
            int s = intrinsic::bit_scan_reverse(b);
            cards[n++] = Card((Rank)r, (Suit)s);
            b &= ~(1 << s);
        }
        ranks_present &= ~(1 << r);
    }
    return n;
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

/// Returns an integer with the highest N set bits of x kept.
template <int N>
static RankMask KeepHighestBitsSet(RankMask x)
{
    RankMask result = 0;
    for (int i = 0; i < N; i++)
    {
        int b = intrinsic::bit_scan_reverse(x);
        result |= (RankMask(1) << b);
        x &= ~result;
    }
    return result;
}

#if 0
template <typename T>
T KeepHighestBitSet(T x)
{
    return KeepHighestBitsSet<1>(x);
}
#endif

static RankMask KeepHighestBitSet(RankMask x)
{
    return KeepHighestBitsSet<1>(x);
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

/**
 * Evaluates a hand of five to seven cards and returns the strength of the
 * strongest five-card combination.
 */
HandStrength EvaluateHand2(const Hand2 &hand)
{
    // Let v be the rank masks excluding the counter bits.
    uint64_t v = hand.value & 0x1FFF1FFF1FFF1FFFULL;

    // Compute masks of the ranks present in the hand, ranks that appear at
    // least twice, ranks that appear at least 3 times, etc.
    RankMask ranks_present, ranks_2_times, ranks_3_times, ranks_4_times;
    
    RankMask m = (uint16_t)v;
    ranks_present = m;
    
    m = (uint16_t)(v >> 16);
    ranks_2_times = ranks_present & m;
    ranks_present |= m;

    m = (uint16_t)(v >> 32);
    ranks_3_times = ranks_2_times & m;
    ranks_2_times |= ranks_present & m;
    ranks_present |= m;
    
    m = (uint16_t)(v >> 48);
    ranks_4_times = ranks_3_times & m;
    ranks_3_times |= ranks_2_times & m;
    ranks_2_times |= ranks_present & m;
    ranks_present |= m;

    // Compute a mask of the flushed suit. (For seven or fewer cards, there
    // can be at most one flushed suit.) To have five to seven cards of the
    // same suit, the suit counter, x, must take one of the following values:
    // 5 (101), 6 (110), 7(111). Any non-flush values (x <= 4) have either
    // the 0x4 bit unset, or the 0x4 bit set but the lower 2 bits unset. Thus
    // we can check for flushed suit by testing the following condition:
    // bit 0x4 and (bit 0x2 or bit 0x1) != 0.
    uint64_t sc = hand.value & 0xE000E000E000E000ULL;      // suit counters
    sc &= ((sc << 1) | (sc << 2)) & 0x8000800080008000ULL; // test flush
    RankMask ranks_flushed = 0;
    if (sc)
    {
        Suit suit_flushed = (Suit)(intrinsic::bit_scan_reverse(sc) / 16);
        ranks_flushed = (RankMask)(v >> (16 * suit_flushed));
    }

    // Check for straight flush.
    if (ranks_flushed)
    {
        // Check for straight within the flushed suit. This is automatic when
        // there are exactly 5 cards; but when there are more than 5 cards, 
        // straight and flush may not imply straight-flush. The algorithm to 
        // check for straights is detailed later with "Check for straights".
        RankMask m = (ranks_flushed << 1) | (ranks_flushed >> 12);
        RankMask ranks_straight = (m & (m<<1) & (m<<2) & (m<<3) & (m<<4)) >> 1;
        if (ranks_straight)
        {
            return HandStrength(StraightFlush, KeepHighestBitSet(ranks_straight));
        }
    }

    // Check for four-of-a-kind. For seven or fewer cards, there can be 
    // at most one four-of-a-kind combination.
    if (ranks_4_times)
    {
        RankMask master = ranks_4_times;
        RankMask kicker = KeepHighestBitSet(ranks_present & ~master);
        return HandStrength(FourOfAKind, master, kicker);
    }

    // Check for full house.
    // Note that for 7 cards, there may be two possible three-of-a-kind...
    if (ranks_3_times)
    {
        RankMask master = KeepHighestBitSet(ranks_3_times);
        RankMask kicker = ranks_2_times & ~master;
        if (kicker)
        {
            kicker = KeepHighestBitSet(kicker);
            return HandStrength(FullHouse, master, kicker);
        }
    }

    // Check for flush.
    if (ranks_flushed)
    {
        return HandStrength(Flush, KeepHighestBitsSet<5>(ranks_flushed));
    }

    // Check for straights. We first copy the Ace-bit to the lowest bit so 
    // that we can easily test for 5-4-3-2-1 straight. Then a straight is 
    // such that there are five consecutive bits set in the mask. This can 
    // be tested by shift-and the mask five times. The highest bit left set
    // is the best straight. If no bit is left set, there are no straights.
    m = (ranks_present << 1) | (ranks_present >> 12);
    RankMask mask_straight = (m & (m << 1) & (m << 2) & (m << 3) & (m << 4)) >> 1;
    if (mask_straight)
    {
        return HandStrength(Straight, KeepHighestBitSet(mask_straight));
    }

    // Check for three-of-a-kind. For 7 cards, there may be two possible
    // three-of-a-kind; however, that must have already led to a full-house.
    // So here, we have at most one three-of-a-kind, together with a bunch
    // of high cards.
    if (ranks_3_times)
    {
        RankMask master = ranks_3_times;
        RankMask kicker = KeepHighestBitsSet<2>(ranks_present & ~master);
        return HandStrength(ThreeOfAKind, master, kicker);
    }

    // Check for two-pair and one pair.
    if (ranks_2_times)
    {
        RankMask master = KeepHighestBitSet(ranks_2_times);
        ranks_2_times &= ~master;
        if (ranks_2_times) // two-pair
        {
            master |= KeepHighestBitSet(ranks_2_times);
            RankMask kicker = KeepHighestBitSet(ranks_present & ~master);
            return HandStrength(TwoPair, master, kicker);
        }
        else // one pair
        {
            RankMask kicker = KeepHighestBitsSet<3>(ranks_present & ~master);
            return HandStrength(OnePair, master, kicker);
        }
    }

    // Now we are left with high card.
    return HandStrength(HighCard, KeepHighestBitsSet<5>(ranks_present));
}

#if 0
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
#endif

#if 0
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
#endif

#if 0
hand_t make_hand(const card_t cards[5])
{
	hand_t h;
	memcpy(h.cards, cards, sizeof(h.cards));
	h.category = normalize_hand(h.cards);
	return h;
}
#endif

#if 0
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
#endif

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