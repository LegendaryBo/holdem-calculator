#include <iostream>
#include <fstream>
#include <vector>
#include <functional>
#include <algorithm>
#include "hand.h"

void test()
{
	int lineno = 0;
	int count = 0;
	std::ifstream fs("poker.txt");
	for (;;)
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
		
		hand_t h1 = make_hand(cards + 0);
		hand_t h2 = make_hand(cards + 5);
		int winner = compare_hands(h1, h2);

#if 1
		std::cout << lineno << ": ";

		write_hand(s, h1);
		std::cout << s;
		write_hand(s, h2);
		std::cout << " - " << s << ": ";

		if (winner > 0)
			std::cout << "WIN\n";
		else if (winner < 0)
			std::cout << "LOSE\n";
		else
			std::cout << "TIE\n";
#endif

		if (winner > 0)
			++count;
	}

	//cout << "Player 1 wins " << count << " hands." << endl;
	std::cout << count << std::endl;
}

void test2()
{
	std::ifstream fs("poker2.txt");
	std::vector<hand_t> hands;
	for (;;)
	{
		char s[50];
		fs.getline(s, sizeof(s));
		if (s[0] == 0)
			break;

		card_t cards[5];
		for (int i = 0; i < 5; i++)
		{
			cards[i] = read_card(s[3*i], s[3*i+1]);
		}
		
		hand_t h = make_hand(cards);
		hands.push_back(h);
	}
	std::sort(hands.begin(), hands.end(), std::greater<hand_t>());
	for (auto it = hands.begin(); it != hands.end(); ++it)
	{
		char s[100];
		write_hand(s, *it);
		std::cout << s << "\n";
	}
}
