#include "stdafx.h"

#include "chip8\Chip8Interpreter.h"

#include <fstream>

using namespace chip8;

class InterpreterTest : public ::testing::Test {

protected:

	std::auto_ptr<Interpreter> interpreter;

	Interpreter::Snapshot snapshot;

	void SetUp() {
		interpreter.reset(new Interpreter);

		Interpreter::Snapshot::obtain(snapshot, *interpreter);
	}
};

TEST_F(InterpreterTest, Initialization) {
	interpreter->reset(std::ifstream("15PUZZLE"));

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

TEST_F(InterpreterTest, Opcode0xxx) {
	// Opcode 0x00e0
	{
		const byte program[] = { 0x00, 0xE0 };
		const Interpreter::Frame referenceFrame = { 0x00 };

		interpreter->reset(program, sizeof(program));
		interpreter->doCycle();	

		ASSERT_TRUE(interpreter->isFrameUpdated());
		EXPECT_EQ(0, memcmp(interpreter->getFrame(), referenceFrame, _countof(referenceFrame)));

		EXPECT_EQ(0x01, snapshot.getCarryValue());
	}
	// Opcode 0x00e0
	{
		const byte program[] = { 0x22, 0x02, 0x00, 0xEE };

		interpreter->reset(program, sizeof(program));
		
		interpreter->doCycle(); EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + sizeof(Opcode), snapshot.getProgramCounterValue());
		interpreter->doCycle(); EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + sizeof(Opcode), snapshot.getProgramCounterValue());
	}
}