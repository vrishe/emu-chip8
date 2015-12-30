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
	EXPECT_EQ(Interpreter::KEY_NONE, snapshot.getKeyboardValue());
}

TEST_F(InterpreterTest, Opcode0NNN_2NNN) {
	// Opcode 00e0
	{
		const byte program[] = { 0x00,0xE0 };
		const Interpreter::Frame referenceFrame = { 0x00 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();	
		ASSERT_TRUE(interpreter->isFrameUpdated());
		EXPECT_EQ(0x01, snapshot.getCarryValue());
		EXPECT_EQ(0, memcmp(&snapshot.getFrameValue(0), referenceFrame, sizeof(Interpreter::Frame)));

		EXPECT_EQ(64, snapshot.getCountCycles());
	}
	// Opcode 00e0 \w 2NNN
	{
		const byte program[] = { 0x22,0x02, 0x00,0xEE };

		interpreter->reset(program, sizeof(program));
		
		interpreter->doCycle(); 
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + sizeof(Opcode), snapshot.getProgramCounterValue());

		interpreter->doCycle(); 
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + sizeof(Opcode), snapshot.getProgramCounterValue());

		EXPECT_EQ(144, snapshot.getCountCycles());
	}
}

TEST_F(InterpreterTest, Opcode1NNN) {
	const byte program[] = { 0x12,0x00 };

	interpreter->reset(program, sizeof(program));
	
	interpreter->doCycle(); 
	EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());

	interpreter->doCycle();
	EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());
	
	EXPECT_EQ(160, snapshot.getCountCycles());
}

TEST_F(InterpreterTest, Opcode3XNN_4XNN_6XNN_1NNN) {
	// Opcode 3XNN \w 6XNN & 1NNN (True)
	{
		const byte program[] = { 0x65,0xBE, 0x35,0xBE, 0x65,0x00, 0x12,0x00 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle(); 
		EXPECT_EQ(0xBE, snapshot.getRegisterValue(5));

		interpreter->doCycle(); 
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 3 * sizeof(Opcode), snapshot.getProgramCounterValue());

		interpreter->doCycle();
		EXPECT_EQ(0xBE, snapshot.getRegisterValue(5));
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());
		
		EXPECT_EQ(236, snapshot.getCountCycles());
	}
	// Opcode 3XNN \w 6XNN & 1NNN (False)
	{
		const byte program[] = { 0x65,0xBE, 0x35,0xBF, 0x65,0x00, 0x12,0x00 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle(); 
		EXPECT_EQ(0xBE, snapshot.getRegisterValue(5));

		interpreter->doCycle(); 
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 2 * sizeof(Opcode), snapshot.getProgramCounterValue());

		interpreter->doCycle(); 
		EXPECT_EQ(0x00, snapshot.getRegisterValue(5));

		interpreter->doCycle(); 
		EXPECT_EQ(0x00, snapshot.getRegisterValue(5));
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());
		
		EXPECT_EQ(306, snapshot.getCountCycles());
	}
	// Opcode 4XNN \w 6XNN & 1NNN (True)
	{
		const byte program[] = { 0x65,0xBE, 0x45,0xBF, 0x65,0x00, 0x12,0x00 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle(); 
		EXPECT_EQ(0xBE, snapshot.getRegisterValue(5));

		interpreter->doCycle(); 
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 3 * sizeof(Opcode), snapshot.getProgramCounterValue());

		interpreter->doCycle(); 
		EXPECT_EQ(0xBE, snapshot.getRegisterValue(5));
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());

		EXPECT_EQ(236, snapshot.getCountCycles());
	}
	// Opcode 4XNN \w 6XNN & 1NNN (False)
	{
		const byte program[] = { 0x65,0xBE, 0x45,0xBE, 0x65,0x00, 0x12,0x00 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xBE, snapshot.getRegisterValue(5));

		interpreter->doCycle();
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 2 * sizeof(Opcode), snapshot.getProgramCounterValue());

		interpreter->doCycle();
		EXPECT_EQ(0x00, snapshot.getRegisterValue(5));

		interpreter->doCycle();
		EXPECT_EQ(0x00, snapshot.getRegisterValue(5));
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());

		EXPECT_EQ(306, snapshot.getCountCycles());
	}
}

TEST_F(InterpreterTest, Opcode5XY0_9XY0_6XNN_1NNN) {
	// Opcode 5XY0 \w 6XNN & 1NNN (True)
	{
		const byte program[] = { 0x6E,0xE0, 0x60,0xE0, 0x5E,0x00, 0x12,0x0C, 0x6E,0x00, 0x60,0x00, 0x12,0x00 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xE0, snapshot.getRegisterValue(14));

		interpreter->doCycle();
		EXPECT_EQ(0xE0, snapshot.getRegisterValue(0));

		interpreter->doCycle();
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 4 * sizeof(Opcode), snapshot.getProgramCounterValue());

		interpreter->doCycle();
		EXPECT_EQ(0x00, snapshot.getRegisterValue(14));

		interpreter->doCycle();
		EXPECT_EQ(0x00, snapshot.getRegisterValue(0));

		interpreter->doCycle();
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());

		EXPECT_EQ(462, snapshot.getCountCycles());
	}
	// Opcode 5XY0 \w 6XNN & 1NNN (False)
	{
		const byte program[] = { 0x6E,0xBE, 0x60,0xB0, 0x5E,0x00, 0x12,0x0C, 0x6E,0x00, 0x60,0x00, 0x12,0x00 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xBE, snapshot.getRegisterValue(14));

		interpreter->doCycle();
		EXPECT_EQ(0xB0, snapshot.getRegisterValue(0));

		interpreter->doCycle();
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 3 * sizeof(Opcode), snapshot.getProgramCounterValue());

		interpreter->doCycle();
		EXPECT_NE(0x00, snapshot.getRegisterValue(14));
		EXPECT_NE(0x00, snapshot.getRegisterValue(0));

		interpreter->doCycle();
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());

		EXPECT_EQ(390, snapshot.getCountCycles());
	}
	// Opcode 9XY0 \w 6XNN & 1NNN (True)
	{
		const byte program[] = { 0x6E,0xBE, 0x60,0xB0, 0x9E,0x00, 0x12,0x0C, 0x6E,0x00, 0x60,0x00, 0x12,0x00 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xBE, snapshot.getRegisterValue(14));

		interpreter->doCycle();
		EXPECT_EQ(0xB0, snapshot.getRegisterValue(0));

		interpreter->doCycle();
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 4 * sizeof(Opcode), snapshot.getProgramCounterValue());

		interpreter->doCycle();
		EXPECT_EQ(0x00, snapshot.getRegisterValue(14));

		interpreter->doCycle();
		EXPECT_EQ(0x00, snapshot.getRegisterValue(0));

		interpreter->doCycle();
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());

		EXPECT_EQ(462, snapshot.getCountCycles());
	}
	// Opcode 9XY0 \w 6XNN & 1NNN (False)
	{
		const byte program[] = { 0x6E,0xE0, 0x60,0xE0, 0x9E,0x00, 0x12,0x0C, 0x6E,0x00, 0x60,0x00, 0x12,0x00 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xE0, snapshot.getRegisterValue(14));

		interpreter->doCycle();
		EXPECT_EQ(0xE0, snapshot.getRegisterValue(0));

		interpreter->doCycle();
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 3 * sizeof(Opcode), snapshot.getProgramCounterValue());

		interpreter->doCycle();
		EXPECT_NE(0x00, snapshot.getRegisterValue(14));
		EXPECT_NE(0x00, snapshot.getRegisterValue(0));

		interpreter->doCycle();
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());

		EXPECT_EQ(390, snapshot.getCountCycles());
	}
}

TEST_F(InterpreterTest, Opcode7XNN_6XNN_1NNN) {
	const byte program[] = { 0x6A,0xFE, 0x7A,0x01, 0x12,0x00 };

	interpreter->reset(program, sizeof(program));

	interpreter->doCycle();
	EXPECT_EQ(0xFE, snapshot.getRegisterValue(10));

	interpreter->doCycle();
	EXPECT_EQ(0xFF, snapshot.getRegisterValue(10));

	interpreter->doCycle();
	EXPECT_EQ(0xFF, snapshot.getRegisterValue(10));
	EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());

	EXPECT_EQ(232, snapshot.getCountCycles());
}

TEST_F(InterpreterTest, Opcode8XYN_6XNN) {
	// Opcode 8XY0
	{
		const byte program[] = { 0x6D,0xAA, 0x83,0xD0 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xAA, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0xAA, snapshot.getRegisterValue(3));

		EXPECT_EQ(154, snapshot.getCountCycles());
	}
	// Opcode 8XY1
	{
		const byte program[] = { 0x6D,0xAA, 0x63,0x55, 0x83,0xD1 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xAA, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0x55, snapshot.getRegisterValue(3));

		interpreter->doCycle();
		EXPECT_EQ(0xFF, snapshot.getRegisterValue(3));

		EXPECT_EQ(260, snapshot.getCountCycles());
	}
	// Opcode 8XY2
	{
		const byte program[] = { 0x6D,0xAA, 0x63,0x55, 0x83,0xD2 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xAA, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0x55, snapshot.getRegisterValue(3));

		interpreter->doCycle();
		EXPECT_EQ(0x00, snapshot.getRegisterValue(3));

		EXPECT_EQ(260, snapshot.getCountCycles());
	}
	// Opcode 8XY3
	{
		const byte program[] = { 0x6D,0xA5, 0x63,0x81, 0x83,0xD3 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xA5, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0x81, snapshot.getRegisterValue(3));

		interpreter->doCycle();
		EXPECT_EQ(0x24, snapshot.getRegisterValue(3));

		EXPECT_EQ(260, snapshot.getCountCycles());
	}
	// Opcode 8XY4 (no carry)
	{
		const byte program[] = { 0x6D,0xFE, 0x63,0x01, 0x83,0xD4 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xFE, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0x01, snapshot.getRegisterValue(3));

		interpreter->doCycle();
		EXPECT_EQ(0xFF, snapshot.getRegisterValue(3));
		EXPECT_EQ(0x00, snapshot.getCarryValue());

		EXPECT_EQ(260, snapshot.getCountCycles());
	}
	// Opcode 8XY4 (carry)
	{
		const byte program[] = { 0x6D,0xFE, 0x63,0x02, 0x83,0xD4 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xFE, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0x02, snapshot.getRegisterValue(3));

		interpreter->doCycle();
		EXPECT_EQ(0x00, snapshot.getRegisterValue(3));
		EXPECT_EQ(0x01, snapshot.getCarryValue());

		EXPECT_EQ(260, snapshot.getCountCycles());
	}
	// Opcode 8XY5 (no borrrow)
	{
		const byte program[] = { 0x6D,0x0E, 0x63,0xFE, 0x83,0xD5 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0x0E, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0xFE, snapshot.getRegisterValue(3));

		interpreter->doCycle();
		EXPECT_EQ(0xF0, snapshot.getRegisterValue(3));
		EXPECT_EQ(0x01, snapshot.getCarryValue());

		EXPECT_EQ(260, snapshot.getCountCycles());
	}
	// Opcode 8XY5 (borrrow)
	{
		const byte program[] = { 0x6D,0x0E, 0x63,0xFE, 0x83,0xD5 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0x0E, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0xFE, snapshot.getRegisterValue(3));

		interpreter->doCycle();
		EXPECT_EQ(0xF0, snapshot.getRegisterValue(3));
		EXPECT_EQ(0x01, snapshot.getCarryValue());

		EXPECT_EQ(260, snapshot.getCountCycles());
	}
	// Opcode 8XY6 (no carry)
	{
		const byte program[] = { 0x6D,0x02, 0x83,0xD6 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0x02, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0x01, snapshot.getRegisterValue(13));
		EXPECT_EQ(0x01, snapshot.getRegisterValue(3));
		EXPECT_EQ(0x00, snapshot.getCarryValue());

		EXPECT_EQ(186, snapshot.getCountCycles());
	}
	// Opcode 8XY6 (carry)
	{
		const byte program[] = { 0x6D,0x05, 0x83,0xD6 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0x05, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0x02, snapshot.getRegisterValue(13));
		EXPECT_EQ(0x02, snapshot.getRegisterValue(3));
		EXPECT_EQ(0x01, snapshot.getCarryValue());

		EXPECT_EQ(186, snapshot.getCountCycles());
	}
	// Opcode 8XY7 (no borrrow)
	{
		const byte program[] = { 0x6D,0x0E, 0x63,0xFE, 0x8D,0x37 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0x0E, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0xFE, snapshot.getRegisterValue(3));

		interpreter->doCycle();
		EXPECT_EQ(0xF0, snapshot.getRegisterValue(13));
		EXPECT_EQ(0x01, snapshot.getCarryValue());

		EXPECT_EQ(260, snapshot.getCountCycles());
	}
	// Opcode 8XY7 (borrrow)
	{
		const byte program[] = { 0x6D,0x0E, 0x63,0xFE, 0x8D,0x37 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0x0E, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0xFE, snapshot.getRegisterValue(3));

		interpreter->doCycle();
		EXPECT_EQ(0xF0, snapshot.getRegisterValue(13));
		EXPECT_EQ(0x01, snapshot.getCarryValue());

		EXPECT_EQ(260, snapshot.getCountCycles());
	}
	// Opcode 8XYE (no carry)
	{
		const byte program[] = { 0x6D,0x40, 0x83,0xDE };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0x40, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0x80, snapshot.getRegisterValue(13));
		EXPECT_EQ(0x80, snapshot.getRegisterValue(3));
		EXPECT_EQ(0x00, snapshot.getCarryValue());

		EXPECT_EQ(186, snapshot.getCountCycles());
	}
	// Opcode 8XYE (carry)
	{
		const byte program[] = { 0x6D,0xA0, 0x83,0xDE };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xA0, snapshot.getRegisterValue(13));

		interpreter->doCycle();
		EXPECT_EQ(0x40, snapshot.getRegisterValue(13));
		EXPECT_EQ(0x40, snapshot.getRegisterValue(3));
		EXPECT_EQ(0x01, snapshot.getCarryValue());

		EXPECT_EQ(186, snapshot.getCountCycles());
	}
}

TEST_F(InterpreterTest, OpcodeBNNN_6XNN) {
	const byte program[] = { 0x60,0x06, 0xB2,0x00, 0xB1,0xFC, 0x60,0x04, 0xB2,0x00 };

	interpreter->reset(program, sizeof(program));

	interpreter->doCycle();
	EXPECT_EQ(0x06, snapshot.getRegisterValue(0));

	interpreter->doCycle();
	EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 6, snapshot.getProgramCounterValue());

	interpreter->doCycle();
	EXPECT_EQ(0x04, snapshot.getRegisterValue(0));

	interpreter->doCycle();
	EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 4, snapshot.getProgramCounterValue());

	interpreter->doCycle();
	EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());

	EXPECT_EQ(418, snapshot.getCountCycles());
}

TEST_F(InterpreterTest, OpcodeDXYN_6XNN_ANNN) {
	const byte program[] = { 0xA2,0x20,

		0x60,0x00, 0x61,0x00, 0xD0,0x18,			//  0;  0
		0x60,0x3B, 0x61,0x00, 0xD0,0x18,			// 59;  0
		0x60,0x00, 0x61,0x1B, 0xD0,0x18,			//  0; 27
		0x60,0x3B, 0x61,0x1B, 0xD0,0x18,			// 59; 27
		0x60,0x40, 0x61,0x20, 0xD0,0x18,			// 64; 32

		// Sprite data below (offset 32 bytes):
		0x99,0x42,0x3C,0xA5,0xA5,0x3C,0x42,0x99 
	};
	Interpreter::Frame referenceFrame = { 0x00 };

	interpreter->reset(program, sizeof(program));

	interpreter->doCycle();
	EXPECT_EQ(0x220, snapshot.getIndexValue());
	EXPECT_EQ(0, memcmp(&snapshot.getMemoryValue(0x220), &program[32], 8));

	interpreter->doCycle();
	interpreter->doCycle();
	interpreter->doCycle();
	{
		referenceFrame[  0] = 0xFF; referenceFrame[  3] = 0xFF; referenceFrame[  4] = 0xFF; referenceFrame[  7] = 0xFF;
		referenceFrame[ 65] = 0xFF; referenceFrame[ 70] = 0xFF;
		referenceFrame[130] = 0xFF; referenceFrame[131] = 0xFF; referenceFrame[132] = 0xFF; referenceFrame[133] = 0xFF;
		referenceFrame[192] = 0xFF; referenceFrame[194] = 0xFF; referenceFrame[197] = 0xFF; referenceFrame[199] = 0xFF;
		referenceFrame[256] = 0xFF; referenceFrame[258] = 0xFF; referenceFrame[261] = 0xFF; referenceFrame[263] = 0xFF;
		referenceFrame[322] = 0xFF; referenceFrame[323] = 0xFF; referenceFrame[324] = 0xFF; referenceFrame[325] = 0xFF;
		referenceFrame[385] = 0xFF; referenceFrame[390] = 0xFF;
		referenceFrame[448] = 0xFF; referenceFrame[451] = 0xFF; referenceFrame[452] = 0xFF; referenceFrame[455] = 0xFF;
	}
	EXPECT_EQ(0, memcmp(&snapshot.getFrameValue(0), referenceFrame, sizeof(Interpreter::Frame)));
	EXPECT_EQ(0x00, snapshot.getCarryValue());

	interpreter->doCycle();
	interpreter->doCycle();
	interpreter->doCycle();
	{
		referenceFrame[ 59] = 0xFF; referenceFrame[ 62] = 0xFF; referenceFrame[ 63] = 0xFF;
		referenceFrame[124] = 0xFF;
		referenceFrame[189] = 0xFF; referenceFrame[190] = 0xFF; referenceFrame[191] = 0xFF;
		referenceFrame[251] = 0xFF; referenceFrame[253] = 0xFF;
		referenceFrame[315] = 0xFF; referenceFrame[317] = 0xFF;
		referenceFrame[381] = 0xFF; referenceFrame[382] = 0xFF; referenceFrame[383] = 0xFF;
		referenceFrame[444] = 0xFF;
		referenceFrame[507] = 0xFF; referenceFrame[510] = 0xFF; referenceFrame[511] = 0xFF;
	}
	EXPECT_EQ(0, memcmp(&snapshot.getFrameValue(0), referenceFrame, sizeof(Interpreter::Frame)));
	EXPECT_EQ(0x00, snapshot.getCarryValue());

	interpreter->doCycle();
	interpreter->doCycle();
	interpreter->doCycle();
	{
		referenceFrame[1728] = 0xFF; referenceFrame[1731] = 0xFF; referenceFrame[1732] = 0xFF; referenceFrame[1735] = 0xFF;
		referenceFrame[1793] = 0xFF; referenceFrame[1798] = 0xFF;
		referenceFrame[1858] = 0xFF; referenceFrame[1859] = 0xFF; referenceFrame[1860] = 0xFF; referenceFrame[1861] = 0xFF;
		referenceFrame[1920] = 0xFF; referenceFrame[1922] = 0xFF; referenceFrame[1925] = 0xFF; referenceFrame[1927] = 0xFF;
		referenceFrame[1984] = 0xFF; referenceFrame[1986] = 0xFF; referenceFrame[1989] = 0xFF; referenceFrame[1991] = 0xFF;
	}
	EXPECT_EQ(0, memcmp(&snapshot.getFrameValue(0), referenceFrame, sizeof(Interpreter::Frame)));
	EXPECT_EQ(0x00, snapshot.getCarryValue());

	interpreter->doCycle();
	interpreter->doCycle();
	interpreter->doCycle();
	{
		referenceFrame[1787] = 0xFF; referenceFrame[1790] = 0xFF; referenceFrame[1791] = 0xFF;
		referenceFrame[1852] = 0xFF;
		referenceFrame[1917] = 0xFF; referenceFrame[1918] = 0xFF; referenceFrame[1919] = 0xFF;
		referenceFrame[1979] = 0xFF; referenceFrame[1981] = 0xFF;
		referenceFrame[2043] = 0xFF; referenceFrame[2045] = 0xFF;
	}
	EXPECT_EQ(0, memcmp(&snapshot.getFrameValue(0), referenceFrame, sizeof(Interpreter::Frame)));
	EXPECT_EQ(0x00, snapshot.getCarryValue());

	interpreter->doCycle();
	interpreter->doCycle();
	interpreter->doCycle();
	{
		referenceFrame[  0] = 0x00; referenceFrame[  3] = 0x00; referenceFrame[  4] = 0x00; referenceFrame[  7] = 0x00;
		referenceFrame[ 65] = 0x00; referenceFrame[ 70] = 0x00;
		referenceFrame[130] = 0x00; referenceFrame[131] = 0x00; referenceFrame[132] = 0x00; referenceFrame[133] = 0x00;
		referenceFrame[192] = 0x00; referenceFrame[194] = 0x00; referenceFrame[197] = 0x00; referenceFrame[199] = 0x00;
		referenceFrame[256] = 0x00; referenceFrame[258] = 0x00; referenceFrame[261] = 0x00; referenceFrame[263] = 0x00;
		referenceFrame[322] = 0x00; referenceFrame[323] = 0x00; referenceFrame[324] = 0x00; referenceFrame[325] = 0x00;
		referenceFrame[385] = 0x00; referenceFrame[390] = 0x00;
		referenceFrame[448] = 0x00; referenceFrame[451] = 0x00; referenceFrame[452] = 0x00; referenceFrame[455] = 0x00;
	}
	EXPECT_EQ(0, memcmp(&snapshot.getFrameValue(0), referenceFrame, sizeof(Interpreter::Frame)));
	EXPECT_EQ(0x01, snapshot.getCarryValue());

	EXPECT_EQ(11320, snapshot.getCountCycles());
}

TEST_F(InterpreterTest, OpcodeEX9E_EXA1_6XNN) {
	const byte program[] = { 0x6E,0x06, 0xEE,0x9E, 0x6E,0x00, 0x12,0x00 };

	interpreter->reset(program, sizeof(program));

	// Opcode EX9E \w 6XNN (True)
	{
		interpreter->setKeyHit(Interpreter::KEY_6);

		interpreter->doCycle();
		EXPECT_EQ(0x06, snapshot.getRegisterValue(14));

		interpreter->doCycle();
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 3 * sizeof(Opcode), snapshot.getProgramCounterValue());

		interpreter->doCycle();
	}
	EXPECT_EQ(240, snapshot.getCountCycles());
	EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());

	// Opcode EX9E \w 6XNN (True)
	{
		interpreter->setKeyHit(Interpreter::KEY_NONE);

		interpreter->doCycle();
		EXPECT_EQ(0x06, snapshot.getRegisterValue(14));

		interpreter->doCycle();
		EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START + 2 * sizeof(Opcode), snapshot.getProgramCounterValue());

		interpreter->doCycle();
		EXPECT_EQ(0x00, snapshot.getRegisterValue(14));

		interpreter->doCycle();
	}
	EXPECT_EQ(550, snapshot.getCountCycles());
	EXPECT_EQ(Interpreter::OFFSET_PROGRAM_START, snapshot.getProgramCounterValue());
}

TEST_F(InterpreterTest, OpcodeFXNN_6XNN_ANNN) {
	// Opcode FX15, FX18 and FX07 \w 6XNN
	{
		const byte program[] = { 0x6E,0xFF, 0xFE,0x15, 0xFE,0x18, 0xFA,0x07 };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xFF, snapshot.getRegisterValue(14));

		interpreter->doCycle();
		EXPECT_EQ(0xFF, snapshot.getTimerDelay());

		interpreter->doCycle();
		EXPECT_EQ(0xFF, snapshot.getTimerSound());

		interpreter->doCycle();
		EXPECT_EQ(0xFF, snapshot.getRegisterValue(10));

		EXPECT_EQ(308, snapshot.getCountCycles());
	}
	// Opcode FX1E \w 6XNN, ANNN
	{
		const byte program[] = { 0x6E,0xDD, 0xA2,0x22, 0xFE,0x1E };

		interpreter->reset(program, sizeof(program));

		interpreter->doCycle();
		EXPECT_EQ(0xDD, snapshot.getRegisterValue(14));

		interpreter->doCycle();
		EXPECT_EQ(0x222, snapshot.getIndexValue());

		interpreter->doCycle();
		EXPECT_EQ(0x2FF, snapshot.getIndexValue());

		EXPECT_EQ(238, snapshot.getCountCycles());
	}
}