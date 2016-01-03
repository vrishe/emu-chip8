#include "chip8/Chip8Interpreter.h"


namespace chip8 {

#define FONT_SYMBOL_HEIGHT 5

	// Default system font. See: http://mattmik.com/chip8.html example.
	byte Interpreter::font[0x50] = {
		/* 0 */ 0xF0, 0x90, 0x90, 0x90, 0xF0,  /* 1 */ 0x20, 0x60, 0x20, 0x20, 0x70,  /* 2 */ 0xF0, 0x10, 0xF0, 0x80, 0xF0,  /* 3 */ 0xF0, 0x10, 0xF0, 0x10, 0xF0,
		/* 4 */ 0x90, 0x90, 0xF0, 0x10, 0x10,  /* 5 */ 0xF0, 0x80, 0xF0, 0x10, 0xF0,  /* 6 */ 0xF0, 0x80, 0xF0, 0x90, 0xF0,  /* 7 */ 0xF0, 0x10, 0x20, 0x40, 0x40,
		/* 8 */ 0xF0, 0x90, 0xF0, 0x90, 0xF0,  /* 9 */ 0xF0, 0x90, 0xF0, 0x10, 0xF0,  /* A */ 0xF0, 0x90, 0xF0, 0x90, 0x90,  /* B */ 0xE0, 0x90, 0xE0, 0x90, 0xE0,
		/* C */ 0xF0, 0x80, 0x80, 0x80, 0xF0,  /* D */ 0xE0, 0x90, 0x90, 0x90, 0xE0,  /* E */ 0xF0, 0x80, 0xF0, 0x80, 0xF0,  /* F */ 0xF0, 0x80, 0xF0, 0x80, 0x80
	};

	Interpreter::Interpreter(size_t memorySize)
		: memorySize(memorySize), carry(registers[REGISTERS_COUNT - 1]), rndSeed(reinterpret_cast<word>(this)) {

		memory = new byte[memorySize];
		memcpy(memory, font, sizeof(font));

		reset(nullptr, -1);
	}

	Interpreter::~Interpreter() {
		delete[] memory;
	}


#define PROGRAM_COUNTER_STEP sizeof(Opcode)
#define EXTRACT_OP(opcode) (opcode.hi >> 4)

	void Interpreter::doCycle() {
		if (!isKeyAwaited()) {
			Opcode opcode = *reinterpret_cast<Opcode *>(const_cast<byte*>(memory + pc));

			pc += PROGRAM_COUNTER_STEP;
			countCycles += (this->*Interpreter::macroCodesLUT[EXTRACT_OP(opcode)]) (opcode);

			++rndSeed;
		}
		else {
			// This branch is responsile for key debounce emulation.
			// Not sure, it'll take just 9 cycles.
			//
			// See: http://laurencescotford.co.uk/?p=347 for details.
			countCycles += 9;
		}
		refreshTimers();
	}


	void Interpreter::reset(std::istream &prgStream)
	{
		reset_impl();

		byte *clientMemory = memory + pc;
		std::streamsize readLen = memorySize - pc, fillExcessLen = readLen;

		if (prgStream.good()) {
			prgStream.read(reinterpret_cast<char *>(clientMemory), readLen);
			
			clientMemory += prgStream.gcount();
			fillExcessLen -= prgStream.gcount();
		}
		if (fillExcessLen == readLen) {
			pc = 0x00;
		}
		memset(clientMemory, 0x00, size_t(fillExcessLen));
	}


	void Interpreter::setKeyHit(Keyboard key) {
		if (isKeyAwaited() 
			// This check allows to trigger this method on each loop pass.
			// So, simply there's no need to track keyboard hit externally.
			&& key == KEY_NONE && kb != KEY_NONE) {

			byte keyIdx = 0;
			while (((kb >> keyIdx) & 0x01) == 0) {
				++keyIdx;
			}
			registers[keyHaltRegister] = keyIdx;
			keyHaltRegister = KEY_HALT_UNSET;

			// Reset timer sound as it will be overridden by key hit await routine.
			timers[TIMER_SOUND].value = 0;
		}
		kb = key;
	}


	Interpreter::Frame &Interpreter::getFrame() {
		frameUpdate = false;

		return frame;
	}


	void Interpreter::reset_impl() {
		flush();

		timers[TIMER_DELAY].value = 0;
		timers[TIMER_DELAY].timestamp = TIMESTAMP_UNDEFINED;

		timers[TIMER_SOUND].value = 0;
		timers[TIMER_SOUND].timestamp = TIMESTAMP_UNDEFINED;

		countCycles = 0;

		sp = STACK_DEPTH;
		keyHaltRegister = KEY_HALT_UNSET;

		carry = 0;
		index = 0;
		pc = OFFSET_PROGRAM_START;
		kb = KEY_NONE;
	}


#define TIMER_TICK_CYCLES size_t(29333)

	void Interpreter::refreshTimers() {
		onTick(TIMER_DELAY);
		onTick(TIMER_SOUND);
	}

	void Interpreter::onTick(word timerId) {
		countdown_timer &timer = timers[timerId];

		if (timer.value > 0 && countCycles >= timer.timestamp) {
			size_t difference = (countCycles - timer.timestamp);

			timer.value -= difference / TIMER_TICK_CYCLES + 1;
			timer.timestamp = countCycles + TIMER_TICK_CYCLES - difference % TIMER_TICK_CYCLES;
		}
	}


	// ========================================================
	// Program interpretation
	// ========================================================

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


	inline bool Interpreter::se(size_t idx, word value) {
		if (registers[idx] == value) {
			pc += PROGRAM_COUNTER_STEP;

			return true;
		}
		return false;
	}
	inline bool Interpreter::sne(size_t idx, word value) {
		if (registers[idx] != value) {
			pc += PROGRAM_COUNTER_STEP;

			return true;
		}
		return false;
	}
	inline bool Interpreter::sxye(size_t idx, size_t idy) {
		if (registers[idx] == registers[idy]) {
			pc += PROGRAM_COUNTER_STEP;
			
			return true;
		}
		return false;	
	}
	inline bool Interpreter::sxyne(size_t idx, size_t idy) {
		if (registers[idx] != registers[idy]) {
			pc += PROGRAM_COUNTER_STEP;
			
			return true;
		}
		return false;	
	}
	inline bool Interpreter::skbh(size_t idx) {
		if (!!(kb & (0x01 << registers[idx]))) {
			pc += PROGRAM_COUNTER_STEP;
			
			return true;
		}
		return false;
	}
	inline bool Interpreter::skbnh(size_t idx) {
		if (!!(kb & (0x01 << registers[idx]))) {
			pc += PROGRAM_COUNTER_STEP;
			
			return true;
		}
		return false;
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
		registers[idx] = timers[TIMER_DELAY].value;
	}
	inline void Interpreter::movrs(size_t idx) {
		for (size_t i = 0; i <= idx; ++i, ++index) {
			registers[i] = memory[index];
		}
	}
	inline void Interpreter::movms(size_t idx) {
		for (size_t i = 0; i <= idx; ++i, ++index) {
			memory[index] = byte(registers[i]);
		}
	}


	inline void Interpreter::sti(word value) {
		index = value;
	}


	inline size_t Interpreter::bcd(size_t idx) {
		word value = registers[idx];

		int hundreds = value / 100; 
		value = value % 100;

		int tens = value / 10;
		int ones = value % 10;

		memory[index    ] = hundreds;
		memory[index + 1] = tens;
		memory[index + 2] = ones;

		return hundreds + tens + ones;
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
		word &src = registers[idy];

		carry = src & 0x01;
		src = src >> 1;

		registers[idx] = src;
	}
	inline void Interpreter::shl(size_t idx, size_t idy) {
		word &src = registers[idy];

		carry = src >> 7;
		src = (src << 1) & 0xFF;

		registers[idx] = src;
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
		timers[TIMER_SOUND].value = registers[idx] > 1 ? registers[idx] : 0;
		timers[TIMER_SOUND].timestamp = countCycles + TIMER_TICK_CYCLES;
	}
	inline void Interpreter::delay(size_t idx) {
		timers[TIMER_DELAY].value = registers[idx];
		timers[TIMER_DELAY].timestamp = countCycles + TIMER_TICK_CYCLES;
	}


	inline void Interpreter::flush() {
		memset(frame, 0x00, sizeof(frame));

		carry = 0x01;
		frameUpdate = true;
	}
	inline void Interpreter::sprite(size_t idx, size_t idy, word value) {
		size_t x = registers[idx] % FRAME_WIDTH, y = registers[idy] % FRAME_HEIGHT;
		size_t offsetX = FRAME_WIDTH - x;

		if (offsetX < 8) { 
			offsetX = 8 - offsetX; 
		}
		else {
			offsetX = 0;
		}
		bool collision = false;

		for (size_t i = index, imax = index + value; y < FRAME_HEIGHT && i < imax; ++i) {
			byte *line = frame + x + FRAME_WIDTH * y++;
			byte stride = memory[i] & (0xFF << offsetX);

			while (!!stride) {
				byte &pixel = *line;
				byte value = !!(stride & 0x80) ? 0xFF : 0x00;

				pixel ^= value;
				collision = (value != 0x00 && pixel == 0x00);

				++line;

				stride <<= 1;
			}
		}
		carry = collision ? 0x01 : 0x00;

		frameUpdate = true;
	}

	inline void Interpreter::sym(size_t idx) {
		index = registers[idx] * FONT_SYMBOL_HEIGHT;
	}


	// ========================================================
	// operation code decoder
	// ========================================================

#define EXTRACT_REGX(opcode) word(opcode.hi & 0x0F)
#define EXTRACT_REGY(opcode) word(opcode.lo >> 4)

#define EXTRACT_VAL1(opcode) word(opcode.lo & 0x0F)
#define EXTRACT_VAL2(opcode) word(opcode.lo)
#define EXTRACT_VAL3(opcode) word(((opcode.hi & 0x0F) << 8) | opcode.lo)


#define COUNT_CYCLES_GROUP0_DEFAULT 40
#define COUNT_CYCLES_GROUPN_DEFAULT 68

#define COUNT_CYCLES_TAKEN_FOR(fetch, exec)		((fetch) + (exec))
#define COUNT_CYCLES_TAKEN_BY_GROUP0(exec)		COUNT_CYCLES_TAKEN_FOR(COUNT_CYCLES_GROUP0_DEFAULT, (exec))
#define COUNT_CYCLES_TAKEN_BY_GROUPN(exec)		COUNT_CYCLES_TAKEN_FOR(COUNT_CYCLES_GROUPN_DEFAULT, (exec))

	size_t Interpreter::op0(const Opcode &opcode) {
		switch (opcode.lo) {
		case 0xE0:
			flush();
			return COUNT_CYCLES_TAKEN_BY_GROUP0(24);
		case 0XEE:
			ret();
			return COUNT_CYCLES_TAKEN_BY_GROUP0(10);

			// All other machine instructions are ignored.
			// TODO: define machine instructions precessing here.
		}
		return COUNT_CYCLES_GROUP0_DEFAULT;
	}
	size_t Interpreter::op1(const Opcode &opcode) {
		jmp(EXTRACT_VAL3(opcode));

		return COUNT_CYCLES_TAKEN_BY_GROUPN(12);
	}
	size_t Interpreter::op2(const Opcode &opcode) {
		call(EXTRACT_VAL3(opcode));

		return COUNT_CYCLES_TAKEN_BY_GROUPN(26);
	}

#define COUNT_CYCLES_TAKEN_BY_GROUPN_BRANCH(result, execTrue, execFalse) \
	COUNT_CYCLES_TAKEN_FOR(COUNT_CYCLES_GROUPN_DEFAULT, (result) ? (execTrue) : (execFalse))

	size_t Interpreter::op3(const Opcode &opcode) {
		return COUNT_CYCLES_TAKEN_BY_GROUPN_BRANCH(
			se(EXTRACT_REGX(opcode), EXTRACT_VAL2(opcode)), 14, 10);
	}
	size_t Interpreter::op4(const Opcode &opcode) {
		return COUNT_CYCLES_TAKEN_BY_GROUPN_BRANCH(
			sne(EXTRACT_REGX(opcode), EXTRACT_VAL2(opcode)), 14, 10);
	}
	size_t Interpreter::op5(const Opcode &opcode) {
		if (EXTRACT_VAL1(opcode) == 0) {
			return COUNT_CYCLES_TAKEN_BY_GROUPN_BRANCH(
				sxye(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode)), 18, 14);
		}
		return COUNT_CYCLES_GROUPN_DEFAULT;
	}
	size_t Interpreter::op6(const Opcode &opcode) {
		movn(EXTRACT_REGX(opcode), EXTRACT_VAL2(opcode));

		return COUNT_CYCLES_TAKEN_BY_GROUPN(6);
	}
	size_t Interpreter::op7(const Opcode &opcode) {
		add(EXTRACT_REGX(opcode), EXTRACT_VAL2(opcode));

		return COUNT_CYCLES_TAKEN_BY_GROUPN(10);
	}
	size_t Interpreter::op8(const Opcode &opcode) {
		switch (EXTRACT_VAL1(opcode))
		{
		case 0x00:
			movy(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(12);
		case 0x01:
			or(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(44);
		case 0x02:
			and(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(44);
		case 0x03:
			xor(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(44);
		case 0x04:
			adc(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(44);
		case 0x05:
			sbxyc(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(44);
		case 0x06:
			shr(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(44);
		case 0x07:
			sbyxc(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(44);
		case 0x0E:
			shl(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(44);
		}
		return COUNT_CYCLES_GROUPN_DEFAULT;
	}
	size_t Interpreter::op9(const Opcode &opcode) {
		if (EXTRACT_VAL1(opcode) == 0) {
			return COUNT_CYCLES_TAKEN_BY_GROUPN_BRANCH(
				sxyne(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode)), 18, 14);
		}
		return COUNT_CYCLES_GROUPN_DEFAULT;
	}
	size_t Interpreter::opA(const Opcode &opcode) {
		sti(EXTRACT_VAL3(opcode));

		return COUNT_CYCLES_TAKEN_BY_GROUPN(12);
	}
	size_t Interpreter::opB(const Opcode &opcode) {
		jmp0(EXTRACT_VAL3(opcode));

		return COUNT_CYCLES_TAKEN_BY_GROUPN(22);
	}
	size_t Interpreter::opC(const Opcode &opcode) {
		rnd(EXTRACT_REGX(opcode), EXTRACT_VAL2(opcode));

		return COUNT_CYCLES_TAKEN_BY_GROUPN(36);
	}
	size_t Interpreter::opD(const Opcode &opcode) {
		auto rowsCount = EXTRACT_VAL1(opcode);

		sprite(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode), rowsCount);

		// Because it is really hard to estimate an exact number of cycles, 
		// we provide a linear approximation here.
		//
		// 3812 / 15 ~ 254
		// See: http://laurencescotford.co.uk/?p=304 for details.
		return COUNT_CYCLES_TAKEN_BY_GROUPN(rowsCount * 254);
	}
	size_t Interpreter::opE(const Opcode &opcode) {
		switch (EXTRACT_VAL2(opcode)) {
		case 0x9E:
			return COUNT_CYCLES_TAKEN_BY_GROUPN_BRANCH(
				skbh(EXTRACT_REGX(opcode)), 18, 14);
		case 0xA1:
			return COUNT_CYCLES_TAKEN_BY_GROUPN_BRANCH(
				skbnh(EXTRACT_REGX(opcode)), 18, 14);
		}
		return COUNT_CYCLES_GROUPN_DEFAULT;
	}

#define COUNT_CYCLES_TAKEN_BY_GROUPN_WEIGHTED(operation, opcode, base, perPoint)					\
	{																								\
		auto weight = operation(EXTRACT_REGX(opcode));												\
																									\
		return COUNT_CYCLES_TAKEN_FOR(COUNT_CYCLES_GROUPN_DEFAULT, base + perPoint * (weight));		\
	}	
#define COUNT_CYCLES_TAKEN_BY_GROUPN_SERIAL(operation, opcode, base, perCycle)						\
	{																								\
		auto index = EXTRACT_REGX(opcode);															\
																									\
		operation(index);																			\
		return COUNT_CYCLES_TAKEN_FOR(COUNT_CYCLES_GROUPN_DEFAULT, base + perCycle * (index + 2));	\
	}

	size_t Interpreter::opF(const Opcode &opcode) {
		switch (EXTRACT_VAL2(opcode)) {
		case 0x07:
			movd(EXTRACT_REGX(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(10);
		case 0x0A:
			key(EXTRACT_REGX(opcode));

			// This number is an average of theoretical minimum for 
			// every 0-F key.
			//
			// See: http://laurencescotford.co.uk/?p=347 for details.
			return COUNT_CYCLES_TAKEN_BY_GROUPN(17765);
		case 0x15:
			delay(EXTRACT_REGX(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(10);
		case 0x18:
			sound(EXTRACT_REGX(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(10);
		case 0x1E:
			adi(EXTRACT_REGX(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(16);
		case 0x29:
			sym(EXTRACT_REGX(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(20);
		case 0x33: COUNT_CYCLES_TAKEN_BY_GROUPN_WEIGHTED(bcd, opcode, 84, 16);
		case 0x55: COUNT_CYCLES_TAKEN_BY_GROUPN_SERIAL(movms, opcode, 4, 14);
		case 0x65: COUNT_CYCLES_TAKEN_BY_GROUPN_SERIAL(movrs, opcode, 4, 14);
		}
		return COUNT_CYCLES_GROUPN_DEFAULT;
	}


	size_t (Interpreter::*const Interpreter::macroCodesLUT[0x10]) (const Opcode &) = {
		&Interpreter::op0, &Interpreter::op1, &Interpreter::op2, &Interpreter::op3,
		&Interpreter::op4, &Interpreter::op5, &Interpreter::op6, &Interpreter::op7,
		&Interpreter::op8, &Interpreter::op9, &Interpreter::opA, &Interpreter::opB,
		&Interpreter::opC, &Interpreter::opD, &Interpreter::opE, &Interpreter::opF
	};
} // namespace chip8