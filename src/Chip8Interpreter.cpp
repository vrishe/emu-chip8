#include "chip8/Chip8Interpreter.h"

#include "logger.h"

#include <cassert>
#include <algorithm>


namespace chip8 {

#define FONT_SYMBOL_HEIGHT 5

	// Default system font. See: http://mattmik.com/chip8.html example.
	byte Interpreter::font[0x50] = {
		/* 0 */ 0xF0, 0x90, 0x90, 0x90, 0xF0,  /* 1 */ 0x20, 0x60, 0x20, 0x20, 0x70,  /* 2 */ 0xF0, 0x10, 0xF0, 0x80, 0xF0,  /* 3 */ 0xF0, 0x10, 0xF0, 0x10, 0xF0,
		/* 4 */ 0x90, 0x90, 0xF0, 0x10, 0x10,  /* 5 */ 0xF0, 0x80, 0xF0, 0x10, 0xF0,  /* 6 */ 0xF0, 0x80, 0xF0, 0x90, 0xF0,  /* 7 */ 0xF0, 0x10, 0x20, 0x40, 0x40,
		/* 8 */ 0xF0, 0x90, 0xF0, 0x90, 0xF0,  /* 9 */ 0xF0, 0x90, 0xF0, 0x10, 0xF0,  /* A */ 0xF0, 0x90, 0xF0, 0x90, 0x90,  /* B */ 0xE0, 0x90, 0xE0, 0x90, 0xE0,
		/* C */ 0xF0, 0x80, 0x80, 0x80, 0xF0,  /* D */ 0xE0, 0x90, 0x90, 0x90, 0xE0,  /* E */ 0xF0, 0x80, 0xF0, 0x80, 0xF0,  /* F */ 0xF0, 0x80, 0xF0, 0x80, 0x80
	};

	Interpreter::Interpreter(IDisplay *display, IKeyPad *keyPad, 	
		word aluProfile, size_t memorySize)

		: memorySize(memorySize), carry(registers[REGISTERS_COUNT - 1]) {

		assert(aluProfile == ALU_PROFILE_MODERN 
			|| aluProfile == ALU_PROFILE_ORIGINAL);
		applyALUProfile(aluProfile);

		assert(display);
		deviceDisplay = display;
		assert(keyPad);
		deviceKeyPad = keyPad;

		assert(memorySize > 0);
		memory = new byte[memorySize];
		memset(memory, 0x00, memorySize);
		memcpy(memory, font, sizeof(font));

		rndSeed = reinterpret_cast<word>(this);
		resetImpl();
	}

	Interpreter::~Interpreter() {
		delete[] memory;
	}


	void Interpreter::reset(const byte *prg, size_t prgLen)
	{
		assert(prg);
		resetImpl();

		byte *clientMemory = memory + pc;
		size_t countPadded = memorySize - pc;

		if (prgLen > 0) {
			lastError = INTERPRETER_ERROR_OK;

			if (prgLen < sizeof(Opcode)) {
				lastError = INTERPRETER_ERROR_PROGRAM_TOO_SMALL;
			}
			else if (prgLen > countPadded) {
				lastError = INTERPRETER_ERROR_PROGRAM_TOO_LARGE;
			}
			if (isOk()) {
				memcpy(clientMemory, prg, prgLen);

				clientMemory += prgLen;
				countPadded -= prgLen;
			}
		}
		if (countPadded > 0) {
			memset(clientMemory, 0x00, countPadded);
		}
		cls();
	}
	void Interpreter::reset(std::istream &prgStream)
	{
		assert(prgStream);
		resetImpl();

		byte *clientMemory = memory + pc;
		size_t countPadded = memorySize - pc;

		prgStream.read(reinterpret_cast<char *>(clientMemory), countPadded);

		auto prgLen = size_t(prgStream.gcount());

		if (prgLen > 0) {
			lastError = INTERPRETER_ERROR_OK;

			if (prgLen < sizeof(Opcode)) {
				lastError = INTERPRETER_ERROR_PROGRAM_TOO_SMALL;
			}
			else if (prgLen > countPadded) {
				lastError = INTERPRETER_ERROR_PROGRAM_TOO_LARGE;
			}
			if (isOk()) {
				clientMemory += prgLen;
				countPadded -= prgLen;
			}
		}
		if (countPadded > 0) {
			memset(clientMemory, 0x00, countPadded);
		}
		cls();
	}

	void Interpreter::reset() {
		if (isOk()) {
			resetImpl();

			lastError = INTERPRETER_ERROR_OK;
		}
		cls();
	}

	void Interpreter::resetImpl() {
		memset(stack, 0x00, sizeof(stack));
		memset(registers, 0x00, sizeof(registers));

		timers[TIMER_DELAY].value = 0;
		timers[TIMER_DELAY].timestamp = 0ULL;

		timers[TIMER_SOUND].value = 0;
		timers[TIMER_SOUND].timestamp = 0ULL;

		countCycles = 0;

		sp = STACK_DEPTH;
		keyHaltRegister = KEY_HALT_UNSET;

		carry = 0;
		index = 0;
		pc = OFFSET_PROGRAM_START;
		kb = KEY_NONE;

		lastError = INTERPRETER_ERROR_NO_PROGRAM;
	}


	void Interpreter::applyALUProfile(word aluProfile) {
		switch (aluProfile) {
		case ALU_PROFILE_MODERN:
			shl = &Interpreter::shl_modern;
			shr = &Interpreter::shr_modern;
			break;

		case ALU_PROFILE_ORIGINAL:
			shl = &Interpreter::shl_original;
			shr = &Interpreter::shr_original;
			break;
		}
	}


	// This value is obtained like this:
	//
	// Each machine cycle took 8 clock cycles on RCA1802
	// Machine instructions take 2 to 3 machine cycles, so it takes 20 machine cycles in average.
	// We know that COSMAC VIP operated on 1.76MHz, thus it processed 1760000 / 20 ~ 88000 instructions per second.
	// Timer ticks at rate of 60Hz, so 88000 / 60 ~ 1467.
#define TIMER_TICK_CYCLES size_t(1467)
#define TIMER_TICKS(cycles) \
	(((cycles) / TIMER_TICK_CYCLES) * TIMER_TICK_CYCLES)

	void Interpreter::refreshTimers() {
		onTimerTick(TIMER_DELAY);
		onTimerTick(TIMER_SOUND);
	}

	void Interpreter::onTimerTick(word timerId) {
		countdown_timer &timer = timers[timerId];

		if (timer.value > 0) {
			timer.value--;
		}
	}


#define MODIFY_REGISTER_OP(idx, op, value)								\
	{																	\
		assert(0U <= (idx) && (idx) < Interpreter::REGISTERS_COUNT);	\
		registers[idx] op (value);										\
	}
#define READ_REGISTER(idx)												\
	(																	\
		assert(0U <= (idx) && (idx) < Interpreter::REGISTERS_COUNT),	\
		registers[idx]													\
	)

#define MODIFY_ARRAY_OP_(arr, idx, op, value, lower, higher, errc)	\
	(lastError = (!(lower <= (idx) && (idx) < higher) ? (errc) : INTERPRETER_ERROR_OK), arr[idx] op (value))
#define READ_ARRAY_(arr, idx, lower, higher, errc)					\
	(lastError = (!(lower <= (idx) && (idx) < higher) ? (errc) : INTERPRETER_ERROR_OK), arr[idx])

#define MODIFY_STACK(idx, value) \
	MODIFY_ARRAY_OP_(stack, idx, =, value, 0U, STACK_DEPTH, INTERPRETER_ERROR_STACK_OVERFLOW)
#define READ_STACK(idx) \
	READ_ARRAY_(stack, idx, 0U, STACK_DEPTH, INTERPRETER_ERROR_STACK_OVERFLOW)

#define MODIFY_MEMORY(idx, value) \
	MODIFY_ARRAY_OP_(memory, idx, =, value, OFFSET_PROGRAM_START, memorySize, INTERPRETER_ERROR_INDEX_OUT_OF_BOUNDS)
#define READ_MEMORY(idx) \
	READ_ARRAY_(memory, idx, 0U, memorySize, INTERPRETER_ERROR_INDEX_OUT_OF_BOUNDS)

#define FETCH_OPCODE(offset, lower, higher, errc) \
	(lastError = (!(lower <= (offset) && (offset) < higher) ? (errc) : INTERPRETER_ERROR_OK), (memory + (offset)))

#define PROGRAM_COUNTER_STEP sizeof(Opcode)
#define EXTRACT_OP(opcode) (opcode.hi >> 4)

	clock Interpreter::doCycle() {
		assert(isOk());

		clock result = 0;

		// TODO: it may be allowed to have no keypad in future.
		PadKeys kbState = deviceKeyPad->getState();

		if (isKeyAwaited()) {
			auto &timer = timers[TIMER_SOUND];

			// This check allows to trigger this method on each loop pass.
			// So, simply there's no need to track keyboard hit externally.
			if (kbState == KEY_NONE && kb != KEY_NONE) {

				byte keyIdx = 0;
				while (((kb >> keyIdx) & 0x01) == 0) {
					++keyIdx;
				}
				registers[keyHaltRegister] = keyIdx;
				keyHaltRegister = KEY_HALT_UNSET;

				// Reset timer sound as it will be overridden by kbState hit await routine.
				timer.value = 0;
			}
			else if (kbState != KEY_NONE) {
				// Continue beep'ing until kbState is not released.
				if (timer.value == 0) {
					timer.value = 4;
				}
			}
			// This branch is responsile for kbState debounce emulation.
			// Not sure, it'll take just 9 cycles.
			//
			// See: http://laurencescotford.co.uk/?p=347 for details.
			result += 9;
		}
		kb = kbState;

		if (!isKeyAwaited()) {
			Opcode opcode = *reinterpret_cast<Opcode *>(const_cast<byte*>(
				FETCH_OPCODE(pc, OFFSET_PROGRAM_START, memorySize, INTERPRETER_ERROR_PROGRAM_COUNTER_CORRUPTED)));

			pc += PROGRAM_COUNTER_STEP;
			result += (this->*Interpreter::macroCodesLUT[EXTRACT_OP(opcode)]) (opcode);

			++rndSeed;
		}
		countCycles += result;

		return result;
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
		if (READ_REGISTER(idx) == value) {
			pc += PROGRAM_COUNTER_STEP;

			return true;
		}
		return false;
	}
	inline bool Interpreter::sne(size_t idx, word value) {
		if (READ_REGISTER(idx) != value) {
			pc += PROGRAM_COUNTER_STEP;

			return true;
		}
		return false;
	}
	inline bool Interpreter::sxye(size_t idx, size_t idy) {
		if (READ_REGISTER(idx) == READ_REGISTER(idy)) {
			pc += PROGRAM_COUNTER_STEP;
			
			return true;
		}
		return false;	
	}
	inline bool Interpreter::sxyne(size_t idx, size_t idy) {
		if (READ_REGISTER(idx) != READ_REGISTER(idy)) {
			pc += PROGRAM_COUNTER_STEP;
			
			return true;
		}
		return false;	
	}
	inline bool Interpreter::skbh(size_t idx) {
		if (!!(kb & (0x01 << READ_REGISTER(idx)))) {
			pc += PROGRAM_COUNTER_STEP;
			
			return true;
		}
		return false;
	}
	inline bool Interpreter::skbnh(size_t idx) {
		if (!(kb & (0x01 << READ_REGISTER(idx)))) {
			pc += PROGRAM_COUNTER_STEP;
			
			return true;
		}
		return false;
	}


	inline void Interpreter::movn(size_t idx, word value) {
		MODIFY_REGISTER_OP(idx, =, byte(value));
	}
	inline void Interpreter::movy(size_t idx, size_t idy) {
		MODIFY_REGISTER_OP(idx, =, READ_REGISTER(idy));
	}
	inline void Interpreter::movd(size_t idx) {
		MODIFY_REGISTER_OP(idx, =, timers[TIMER_DELAY].value);
	}
	inline void Interpreter::movrs(size_t idx) {
		for (size_t i = 0; i <= idx; ++i, ++index) {
			MODIFY_REGISTER_OP(i, =, READ_MEMORY(index));
		}
	}
	inline void Interpreter::movms(size_t idx) {
		for (size_t i = 0; i <= idx; ++i, ++index) {
			MODIFY_MEMORY(index, READ_REGISTER(i));
		}
	}


	inline void Interpreter::sti(word value) {
		index = value;
	}
	inline void Interpreter::adi(size_t idx) {
		index += READ_REGISTER(idx);
	}


	inline void Interpreter::add(size_t idx, word value) {
		MODIFY_REGISTER_OP(idx, += , value);
	}

	// As per: http://laurencescotford.co.uk/?p=266
	// carry flag is being assigned the last. This mean that if you
	// use REG F as output, the result will be overwritten by a status flag.
	inline void Interpreter::adc(size_t idx, size_t idy) {
		byte &dst = READ_REGISTER(idx);
		word result = dst + READ_REGISTER(idy);

		dst = byte(result);
		carry = !!(result & 0x100) ? 0x01 : 0x00;
	}
	inline void Interpreter::sbxyc(size_t idx, size_t idy) {
		byte &dst = READ_REGISTER(idx);
		word result = dst - READ_REGISTER(idy);

		dst = byte(result);
		carry = !!(result & 0x100) ? 0x00 : 0x01;
	}
	inline void Interpreter::sbyxc(size_t idx, size_t idy) {
		byte &dst = READ_REGISTER(idx);
		word result = READ_REGISTER(idy) - dst;

		dst = byte(result);
		carry = !!(result & 0x100) ? 0x00 : 0x01;
	}

	void Interpreter::shr_modern(size_t idx, size_t idy) {
		byte &dst = READ_REGISTER(idx);
		byte flag = dst & 0x01;

		dst >>= 1;
		carry = flag;		
	}
	void Interpreter::shl_modern(size_t idx, size_t idy) {
		byte &dst = READ_REGISTER(idx);
		byte flag = dst >> 7;

		dst <<= 1;
		carry = flag;	
	}
	void Interpreter::shr_original(size_t idx, size_t idy) {
		byte value = READ_REGISTER(idy);

		MODIFY_REGISTER_OP(idx, = , value >> 1);
		carry = value & 0x01;
	}
	void Interpreter::shl_original(size_t idx, size_t idy) {
		byte value = READ_REGISTER(idy);

		MODIFY_REGISTER_OP(idx, = , value << 1);
		carry = value >> 7;
	}


	inline void Interpreter::or(size_t idx, size_t idy) {
		MODIFY_REGISTER_OP(idx, |= , READ_REGISTER(idy));
	}
	inline void Interpreter::and(size_t idx, size_t idy) {
		MODIFY_REGISTER_OP(idx, &= , READ_REGISTER(idy));
	}
	inline void Interpreter::xor(size_t idx, size_t idy) {
		MODIFY_REGISTER_OP(idx, ^= , READ_REGISTER(idy));
	}


	inline void Interpreter::sym(size_t idx) {
		index = READ_REGISTER(idx) * FONT_SYMBOL_HEIGHT;
	}
	inline void Interpreter::rnd(size_t idx, word value) {
		word loSeed = rndSeed + 1 & 0x00FF;
		word result = READ_MEMORY(pc & 0xFF00U | loSeed);
		{	
			result = result + ((rndSeed + 1 & 0xFF00) >> 8) & 0xFF;
			result += result >> 1 | (result & 0x01) << 7;
		}
		rndSeed = ((result & 0xFF) << 8) | loSeed;

		MODIFY_REGISTER_OP(idx, = , byte(result & value));
	}


	inline size_t Interpreter::bcd(size_t idx) {
		word value = READ_REGISTER(idx);

		int hundreds = value / 100;
		value = value % 100;

		int tens = value / 10;
		int ones = value % 10;

		MODIFY_MEMORY(index, hundreds);
		MODIFY_MEMORY(index + 1U, tens);
		MODIFY_MEMORY(index + 2U, ones);

		return hundreds + tens + ones;
	}


	inline void Interpreter::cls() {
		memset(deviceDisplay->getLine(0, 0), 0x00, deviceDisplay->area());

		deviceDisplay->invalidate();
	}
	inline void Interpreter::sprite(size_t idx, size_t idy, word value) {
		bool collision = false;

		if (value > 0) {
			const size_t y = READ_REGISTER(idy) % deviceDisplay->height();
			const size_t x = READ_REGISTER(idx) % deviceDisplay->width();
			const size_t w = std::min<size_t>(deviceDisplay->width() - x, 8);
			const size_t h = std::min<size_t>(deviceDisplay->height() - y, value);
	
			const byte strideMask = (w < 8) ? (0xFF << (8 - w)) : 0xFF;
			for (size_t i = index, r = y, rmax = y + h; r < rmax; ++i, ++r) {

				byte *line = deviceDisplay->getLine(r, x);
				for (byte *column = line, data = (READ_MEMORY(i) & strideMask);
					!!data; ++column, data <<= 1) {

					if (!!(data & 0x80)) {
						collision |= !(*column ^= 0xFF);
					}
				}
			}
			deviceDisplay->invalidate(byte(x), byte(y), byte(w), byte(h));
		}
		carry = collision ? 0x01 : 0x00;
	}
	inline void Interpreter::sound(size_t idx) {
		word reg = READ_REGISTER(idx);

		timers[TIMER_SOUND].value = reg > 1 ? reg : 0;
		// 72 is an approx. number of machine cycles
		// that will pass before timer register is set.
		timers[TIMER_SOUND].timestamp = countCycles + 72; 
	}
	inline void Interpreter::delay(size_t idx) {
		timers[TIMER_DELAY].value = READ_REGISTER(idx);
		timers[TIMER_DELAY].timestamp = countCycles + 72;
	}
	inline void Interpreter::key(size_t idx) {
		keyHaltRegister = idx;
	}
	

	// ========================================================
	// complementary methods
	// ========================================================

	void Interpreter::push(word value) {
		size_t idx = --sp;

		MODIFY_STACK(idx, value);
	}

	word Interpreter::pop() {
		size_t idx = sp++;

		return READ_STACK(idx);
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
			cls();
			return COUNT_CYCLES_TAKEN_BY_GROUP0(24);
		case 0XEE:
			ret();
			return COUNT_CYCLES_TAKEN_BY_GROUP0(10);

			// All other machine instructions are ignored.
			// TODO: define machine instructions precessing here.

		default:
			lastError = INTERPRETER_ERROR_UNEXPECTED;
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
		else {
			lastError = INTERPRETER_ERROR_UNEXPECTED;
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
			(this->*shr)(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(44);
		case 0x07:
			sbyxc(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(44);
		case 0x0E:
			(this->*shl)(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode));
			return COUNT_CYCLES_TAKEN_BY_GROUPN(44);

		default:
			lastError = INTERPRETER_ERROR_UNEXPECTED;
		}
		return COUNT_CYCLES_GROUPN_DEFAULT;
	}
	size_t Interpreter::op9(const Opcode &opcode) {
		if (EXTRACT_VAL1(opcode) == 0) {
			return COUNT_CYCLES_TAKEN_BY_GROUPN_BRANCH(
				sxyne(EXTRACT_REGX(opcode), EXTRACT_REGY(opcode)), 18, 14);
		}
		else {
			lastError = INTERPRETER_ERROR_UNEXPECTED;
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
		// (((2533 + 3666) / 3) + 3812) / 15 ~ 412,
		// where first part is for avg. IDL await time,
		// the second is a worst case of time consumtion by drawing routine 
		// (15 rows, collision on each row, 7*14 pixels offscreen).
		//
		// See: http://laurencescotford.co.uk/?p=304 for details.
		return COUNT_CYCLES_TAKEN_BY_GROUPN(rowsCount * 412);
	}
	size_t Interpreter::opE(const Opcode &opcode) {
		switch (EXTRACT_VAL2(opcode)) {
		case 0x9E:
			return COUNT_CYCLES_TAKEN_BY_GROUPN_BRANCH(
				skbh(EXTRACT_REGX(opcode)), 18, 14);
		case 0xA1:
			return COUNT_CYCLES_TAKEN_BY_GROUPN_BRANCH(
				skbnh(EXTRACT_REGX(opcode)), 18, 14);

		default:
			lastError = INTERPRETER_ERROR_UNEXPECTED;
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

		default:
			lastError = INTERPRETER_ERROR_UNEXPECTED;
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