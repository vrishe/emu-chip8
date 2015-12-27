#include "chip8/Chip8Interpreter.h"

#define TIMER_ID_DELAY 1
#define TIMER_ID_SOUND 2

#define FONT_SYMBOL_HEIGHT		5
#define PROGRAM_COUNTER_STEP	sizeof(Opcode)

namespace chip8 {

	// Default system font. See: http://mattmik.com/chip8.html example.
	byte Interpreter::font[0x50] = {
		/* 0 */ 0xF0, 0x90, 0x90, 0x90, 0xF0,  /* 1 */ 0x20, 0x60, 0x20, 0x20, 0x70,  /* 2 */ 0xF0, 0x10, 0xF0, 0x80, 0xF0,  /* 3 */ 0xF0, 0x10, 0xF0, 0x10, 0xF0,
		/* 4 */ 0x90, 0x90, 0xF0, 0x10, 0x10,  /* 5 */ 0xF0, 0x80, 0xF0, 0x10, 0xF0,  /* 6 */ 0xF0, 0x80, 0xF0, 0x90, 0xF0,  /* 7 */ 0xF0, 0x10, 0x20, 0x40, 0x40,
		/* 8 */ 0xF0, 0x90, 0xF0, 0x90, 0xF0,  /* 9 */ 0xF0, 0x90, 0xF0, 0x10, 0xF0,  /* A */ 0xF0, 0x90, 0xF0, 0x90, 0x90,  /* B */ 0xE0, 0x90, 0xE0, 0x90, 0xE0,
		/* C */ 0xF0, 0x80, 0x80, 0x80, 0xF0,  /* D */ 0xE0, 0x90, 0x90, 0x90, 0xE0,  /* E */ 0xF0, 0x80, 0x90, 0x80, 0xF0,  /* E */ 0xF0, 0x80, 0xF0, 0x80, 0x80
	};


	Interpreter::Interpreter(TimerFactory timerFactory, HardwareHandler *hardwareHandler)
		: carry(registers[REGISTERS_COUNT - 1]), hardwareHandler(hardwareHandler), rndSeed(reinterpret_cast<word>(this)) {

		timerDelayPulse = timerFactory(TIMER_ID_DELAY, *this);
		timerSoundPulse = timerFactory(TIMER_ID_SOUND, *this);

		opcodeConverter = ConstructOpcodeConverter(new byte[sizeof(OpcodeConverterBase)]);

		memcpy(memory + OFFSET_FONT_DEFAULT, font, sizeof(font));
		reset(nullptr, -1);
	}

	Interpreter::~Interpreter() {
		delete[] reinterpret_cast<pbyte>(opcodeConverter);
	}

	void Interpreter::reset(std::istream &prgStream)
	{
		reset_impl();

		byte *clientMemory = memory + pc;
		std::streamsize readLen = ADDRESS_SPACE_MAX - pc, fillExcessLen = readLen;

		if (prgStream.good()) {
			prgStream.read(reinterpret_cast<char *>(clientMemory), readLen);

			fillExcessLen -= prgStream.gcount();
		}
		if (fillExcessLen == readLen) {
			pc = 0x00;
		}
		memset(clientMemory, 0x00, size_t(fillExcessLen));
	}
	

	void Interpreter::cycle() {
		Opcode opcode;

		// 1. Fetch a regular opcode.
		opcodeConverter->convert(memory + pc, opcode);
		pc += PROGRAM_COUNTER_STEP;

		// 2. Decode and run an opcode fetched.
		(this->*Interpreter::macroCodesLUT[opcode.value >> 12]) (opcode);

		++rndSeed;
	}

	void Interpreter::onTick(int timerId, size_t tick) {
		switch (timerId) {
		case TIMER_ID_DELAY:
			if (timerDelay > 0) {
				--timerDelay;
			}
			if (timerDelay <= 0) {
				timerDelayPulse->stop();
			}
			break;

		case TIMER_ID_SOUND:
			if (timerSound > 0) {
				--timerSound;
			}
			if (timerSound <= 0) {
				timerSoundPulse->stop();
			}
			break;
		}
	}


	void Interpreter::setKeyHit(Keyboard key) {
		if (isKeyAwaited())
		{
			registers[keyHaltRegister] = key;
			keyHaltRegister = KEY_HALT_UNSET;
		}
		kb = key;
	}


	Interpreter::Frame &Interpreter::getFrame()
	{
		frameUpdate = false;

		return frame;
	}


	// ========================================================
	// program interpretation
	// ========================================================

	inline void Interpreter::rca(word address) {
		if (!!hardwareHandler) {
			hardwareHandler->handle(address);
		}
		else {
			reset(nullptr, -1);
		}
	}

	inline void Interpreter::jmp(word address) {
		pc = address;
	}
	inline void Interpreter::jmp0(word address) {
		pc = address + registers[0];
	}
	inline void Interpreter::call(word address) {
		push(pc);
		pc = address;
	}
	inline void Interpreter::ret() {
		pc = pop();
	}


	inline void Interpreter::se(size_t idx, word value) {
		if (registers[idx] == value)
			pc += PROGRAM_COUNTER_STEP;
	}
	inline void Interpreter::sne(size_t idx, word value) {
		if (registers[idx] != value)
			pc += PROGRAM_COUNTER_STEP;
	}
	inline void Interpreter::sxye(size_t idx, size_t idy) {
		if (registers[idx] == registers[idy])
			pc += PROGRAM_COUNTER_STEP;
	}
	inline void Interpreter::sxyne(size_t idx, size_t idy) {
		if (registers[idx] != registers[idy])
			pc += PROGRAM_COUNTER_STEP;
	}
	inline void Interpreter::skbh(size_t idx) {
		if ((kb & 1 << registers[idx]) != 0)
			pc += PROGRAM_COUNTER_STEP;
	}
	inline void Interpreter::skbnh(size_t idx) {
		if ((kb & 1 << registers[idx]) == 0)
			pc += PROGRAM_COUNTER_STEP;
	}


	inline void Interpreter::key(size_t idx) {
		keyHaltRegister = idx;
	}


	inline void Interpreter::movn(size_t idx, word value) {
		registers[idx] = value;
	}
	inline void Interpreter::movy(size_t idx, size_t idy) {
		registers[idx] = registers[idy];
	}
	inline void Interpreter::movd(size_t idx) {
		registers[idx] = timerDelay;
	}
	inline void Interpreter::movrs(size_t idx) {
		for (int i = idx; i >= 0; --i) {
			registers[i] = memory[index + i];
		}
		index += idx + 1;
	}
	inline void Interpreter::movms(size_t idx) {
		for (int i = idx; i >= 0; --i) {
			memory[index + i] = byte(registers[i]);
		}
		index += idx + 1;
	}


	inline void Interpreter::sti(word value) {
		index = value;
	}


	inline void Interpreter::bcd(size_t idx) {
		word value = registers[idx];

		word divisor = 10;
		for (int i = 0; i >= 2; --i)
		{
			memory[index + i] = value % divisor;
			value = value / divisor;

			divisor *= 10;
		}
	}


	inline void Interpreter::add(size_t idx, word value) {
		registers[idx] += value;
	}
	inline void Interpreter::adi(size_t idx) {
		index += registers[idx];
	}
	inline void Interpreter::adc(size_t idx, size_t idy) {
		word &dst = registers[idx];

		dst += registers[idy];
		carry = !!(dst & ~0xFF) ? 0x01 : 0x00;

		dst &= 0xFF;
	}
	inline void Interpreter::sbxyc(size_t idx, size_t idy) {
		word &dst = registers[idx];

		dst -= registers[idy];
		carry = !!(dst >> 8) ? 0x00 : 0x01;

		dst &= 0xFF;
	}
	inline void Interpreter::sbyxc(size_t idx, size_t idy) {
		word &dst = registers[idx];

		dst = registers[idy] - dst;
		carry = !!(dst >> 8) ? 0x00 : 0x01;

		dst &= 0xFF;
	}


	inline void Interpreter::or(size_t idx, size_t idy) {
		registers[idx] |= registers[idy];
	}
	inline void Interpreter::and(size_t idx, size_t idy) {
		registers[idx] &= registers[idy];
	}
	inline void Interpreter::xor(size_t idx, size_t idy) {
		registers[idx] ^= registers[idy];
	}


	inline void Interpreter::shr(size_t idx, size_t idy) {
		word src = registers[idy];

		carry = src && 0x01;
		registers[idx] = src >> 1;
	}
	inline void Interpreter::shl(size_t idx, size_t idy) {
		word src = registers[idy];

		carry = src >> 7;
		registers[idx] = src << 1 & 0xFF;
	}


	inline void Interpreter::rnd(size_t idx, word value) {
		word loSeed = rndSeed + 1 & 0x00FF;
		word rndBase = memory[pc & 0xFF00 | loSeed];
		word &dst = registers[idx];

		dst = rndBase + ((rndSeed + 1 & 0xFF00) >> 8) & 0xFF;
		dst += dst >> 1 | (dst & 0x01) << 7;

		rndSeed = ((dst & 0xFF) << 8) | loSeed;

		dst &= value;
	}


	inline void Interpreter::sound(size_t idx) {
		timerSound = registers[idx];

		if (timerSound == 1) {
			timerSound = 0;
		}
	}
	inline void Interpreter::delay(size_t idx) {
		timerDelay = registers[idx];
	}


	inline void Interpreter::flush() {
		memset(frame, 0, _countof(frame));

		carry = 0x01;
		frameUpdate = true;
	}
	inline void Interpreter::sprite(size_t idx, size_t idy, word value) {
		size_t x = registers[idx], y = registers[idy];

		if (x < FRAME_WIDTH && y < FRAME_HEIGHT)
		{
			for (size_t i = index, imax = index + value; i < imax; ++i) {
				byte *line = frame + x + FRAME_WIDTH * y++;

				byte stride = memory[i];
				while (!!stride) {
					byte &pixel = *line;

					pixel ^= !!(stride & 0x80) ? 0xFF : 0x00;
					carry |= pixel == 0x00;

					++line;

					stride <<= 1;
				}
			}
			frameUpdate = true;
		}
	}


	inline void Interpreter::sym(size_t idx) {
		index = OFFSET_FONT_DEFAULT + (registers[idx] & 0x0F) * FONT_SYMBOL_HEIGHT;
	}


	// ========================================================
	// operation code decoder
	// ========================================================

#define EXTRACT_REG(opcode, shift)	(opcode.value >> (shift) & 0x0F)
#define EXTRACT_VAL(opcode, mask)	(opcode.value & mask)

#define EXTRACT_REGX(opcode) EXTRACT_REG(opcode, 8) 
#define EXTRACT_REGY(opcode) EXTRACT_REG(opcode, 4)

#define EXTRACT_VAL1(opcode) EXTRACT_VAL(opcode, 0x00F)
#define EXTRACT_VAL2(opcode) EXTRACT_VAL(opcode, 0x0FF)
#define EXTRACT_VAL3(opcode) EXTRACT_VAL(opcode, 0xFFF)


	void Interpreter::op0(const Opcode &opcode) {
		switch (opcode.order.lo) {
		case 0xE0:
			flush();
			return;
		case 0XEE:
			ret();
			return;
		}
		rca(EXTRACT_VAL3(opcode));
	}
	void Interpreter::op1(const Opcode &opcode) {
		jmp(EXTRACT_VAL3(opcode));
	}
	void Interpreter::op2(const Opcode &opcode) {
		call(EXTRACT_VAL3(opcode));
	}
	void Interpreter::op3(const Opcode &opcode) {
		se(EXTRACT_REGX(opcode), EXTRACT_VAL2(opcode));
	}
	void Interpreter::op4(const Opcode &opcode) {
		sne(EXTRACT_REGX(opcode), EXTRACT_VAL2(opcode));
	}
	void Interpreter::op5(const Opcode &opcode) {
		sxye(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
	}
	void Interpreter::op6(const Opcode &opcode) {
		movn(EXTRACT_REGX(opcode), EXTRACT_VAL2(opcode));
	}
	void Interpreter::op7(const Opcode &opcode) {
		add(EXTRACT_REGX(opcode), EXTRACT_VAL2(opcode));
	}
	void Interpreter::op8(const Opcode &opcode) {
		switch (EXTRACT_VAL1(opcode))
		{
		case 0x00:
			movy(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			break;
		case 0x01:
			or(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			break;
		case 0x02:
			and(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			break;
		case 0x03:
			xor(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			break;
		case 0x04:
			adc(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			break;
		case 0x05:
			sbxyc(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			break;
		case 0x06:
			shr(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			break;
		case 0x07:
			sbyxc(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			break;
		case 0x0E:
			shl(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			break;
		}
	}
	void Interpreter::op9(const Opcode &opcode) {
		if (EXTRACT_VAL1(opcode) == 0) {
			sxyne(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
		}
	}
	void Interpreter::opA(const Opcode &opcode) {
		sti(EXTRACT_VAL3(opcode));
	}
	void Interpreter::opB(const Opcode &opcode) {
		jmp0(EXTRACT_VAL3(opcode));
	}
	void Interpreter::opC(const Opcode &opcode) {
		rnd(EXTRACT_REGX(opcode), EXTRACT_VAL2(opcode));
	}
	void Interpreter::opD(const Opcode &opcode) {
		sprite(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode), EXTRACT_VAL1(opcode));
	}
	void Interpreter::opE(const Opcode &opcode) {
		switch (EXTRACT_VAL2(opcode)) {
		case 0x9E:
			skbh(EXTRACT_REGX(opcode));
			break;
		case 0xA1:
			skbnh(EXTRACT_REGX(opcode));
			break;
		}
	}
	void Interpreter::opF(const Opcode &opcode) {
		switch (EXTRACT_VAL2(opcode)) {
		case 0x07:
			movd(EXTRACT_REGX(opcode));
			break;
		case 0x0A:
			key(EXTRACT_REGX(opcode));
			break;
		case 0x15:
			delay(EXTRACT_REGX(opcode));
			break;
		case 0x18:
			sound(EXTRACT_REGX(opcode));
			break;
		case 0x1E:
			adi(EXTRACT_REGX(opcode));
			break;
		case 0x29:
			sym(EXTRACT_REGX(opcode));
			break;
		case 0x33:
			bcd(EXTRACT_REGX(opcode));
			break;
		case 0x55:
			movms(EXTRACT_REGX(opcode));
			break;
		case 0x65:
			movrs(EXTRACT_REGX(opcode));
			break;
		}
	}


	void (Interpreter::*const Interpreter::macroCodesLUT[0x10]) (const Opcode &) = {
		&Interpreter::op0, &Interpreter::op1, &Interpreter::op2, &Interpreter::op3,
		&Interpreter::op4, &Interpreter::op5, &Interpreter::op6, &Interpreter::op7,
		&Interpreter::op8, &Interpreter::op9, &Interpreter::opA, &Interpreter::opB,
		&Interpreter::opC, &Interpreter::opD, &Interpreter::opE, &Interpreter::opF
	};
} // namespace chip8