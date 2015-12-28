#include "stdafx.h"

#include "chip8\Chip8Interpreter.h"

#include <fstream>

using namespace chip8;

TEST(InterpreterTest, Initialization) {
	auto interpreter = std::auto_ptr<Interpreter>(new Interpreter);
	{
		interpreter->reset(std::ifstream("15PUZZLE"));
	}
	Interpreter::Snapshot snapshot;	
	Interpreter::Snapshot::obtain(snapshot, *interpreter);

	EXPECT_EQ(0, snapshot.getTimerDelay());
	EXPECT_EQ(0, snapshot.getTimerSound());
	EXPECT_EQ(0ull, snapshot.getCountCycles());
	EXPECT_EQ(Interpreter::STACK_DEPTH, snapshot.getStackPointer());
	EXPECT_EQ(Interpreter::KEY_HALT_UNSET, snapshot.getKeyHaltRegister());
	EXPECT_EQ(0, snapshot.getCarryValue());
	EXPECT_EQ(0, snapshot.getIndexValue());
	EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());
	EXPECT_EQ(0, snapshot.getKeyboardValue());
}