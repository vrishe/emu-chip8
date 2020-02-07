// emu-chip8-test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


int _tmain(int argc, _TCHAR* argv[])
{
	::testing::InitGoogleTest(&argc, argv);

	auto result = RUN_ALL_TESTS();

	std::cout << std::endl << "Hit ANY key to exit.";
	_getch();

	return result;
}

