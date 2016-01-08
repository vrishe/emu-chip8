#pragma once 

#ifndef CHIP8_INTERPRETER_
#define CHIP8_INTERPRETER_

#include "Chip8Base.h"
#include "Chip8Display.h"
#include "Chip8Keyboard.h"

#include <istream>

namespace chip8 {

	class Interpreter {
	public:

		static byte font[0x50];

		enum : word {
			STACK_DEPTH				= 0x20,
			REGISTERS_COUNT			= 0x10,

			OFFSET_PROGRAM_START	= 0x200,
			ADDRESS_SPACE_DEFAULT	= 0x1000,
			
			ALU_PROFILE_ORIGINAL	= 0x1F01,
			ALU_PROFILE_MODERN		= 0x1F02
		};

		enum Error {
			INTERPRETER_ERROR_OK = 0,

			INTERPRETER_ERROR_NO_PROGRAM,
			INTERPRETER_ERROR_PROGRAM_TOO_LARGE,
			INTERPRETER_ERROR_PROGRAM_TOO_SMALL,

			INTERPRETER_ERROR_STACK_OVERFLOW,
			INTERPRETER_ERROR_STACK_UNDERFLOW,

			INTERPRETER_ERROR_INDEX_OUT_OF_BOUNDS,

			INTERPRETER_ERROR_UNEXPECTED
		};

		
	

		Interpreter(
			IDisplay *display,
			IKeyPad *keyPad,

			word aluProfile = ALU_PROFILE_MODERN, 
			size_t memorySize = ADDRESS_SPACE_DEFAULT
			);

		~Interpreter();


		Error getLastError() const {
			return lastError;
		}


		void doCycle();

		void reset() {
			reset_impl();
		}
		template <size_t prgLen>
		void reset(const byte(&prg)[prgLen]) {
			reset(prg, prgLen);
		}
		void reset(const byte *prg, size_t prgLen);
		void reset(std::istream &prgStream);


		bool isPlayingSound() const {
			return timers[TIMER_SOUND].value > 0;
		}

		bool isKeyAwaited() const {
			return keyHaltRegister != KEY_HALT_UNSET;
		}

		bool isFrameUpdated() const {
			return deviceDisplay->isInvalid();
		}

		bool isOk() const {
			return getLastError() == INTERPRETER_ERROR_OK;
		}


		operator bool() const {
			return isOk();
		}


	private:

		Interpreter(const Interpreter&);


		IDisplay	*deviceDisplay;
		IKeyPad		*deviceKeyPad;

		Error lastError;


		typedef void (Interpreter::*ALUFunc) (size_t, size_t);

		ALUFunc shl;
		ALUFunc shr;

		void applyALUProfile(word aluProfile);


		enum : size_t {
			TIMER_DELAY = 0,
			TIMER_SOUND = 1,

			KEY_HALT_UNSET = size_t(-1),

			TIMESTAMP_UNDEFINED = size_t(-1)
		};

		struct countdown_timer {
			size_t timestamp;

			byte value;
		};

		countdown_timer timers[2];
		size_t countCycles;

		size_t sp;
		size_t keyHaltRegister;

		byte &carry;
		word index;
		word pc;
		word kb;

		word rndSeed;
		
		word stack[STACK_DEPTH];
		byte registers[REGISTERS_COUNT];

		const size_t memorySize;
		byte *memory;


		void reset_impl();

		void refreshTimers();
		void onTick(word timerId);


		// ========================================================
		// program interpretation
		// ========================================================

		void jmp(word address);								// Jump straight to address specified.
		void jmp0(word address);							// Jump to address + V0 location.
		void call(word address);							// Call a routine on address specified (current sp is placed on stack).
		void ret();											// Return after routine call (retrieves a previously stored pc from stack).

		bool se(size_t idx, word value);					// Skip, if VX equal NN.
		bool sne(size_t idx, word value);					// Skip, if VX NOT equal NN.
		bool sxye(size_t idx, size_t idy);					// Skip, if VX equal VY.
		bool sxyne(size_t idx, size_t idy);					// Skip, if VX NOT equal VY.
		bool skbh(size_t idx);								// Skip, if keyboard key by value of VX IS pressed.
		bool skbnh(size_t idx);								// Skip, if keyboard key by value of VX IS NOT pressed.

		void movn(size_t idx, word value);					// Set VX to NN value.
		void movy(size_t idx, size_t idy);					// Set VX to VY.
		void movd(size_t idx);								// Set VX to delay timer value.
		void movrs(size_t idx);								// Store the memory values that start at I into registers V0 trough VX.
		void movms(size_t idx);								// Store the values of registerx V0 through VX within memory, starting at I.

		void sti(word value);								// Store value to index register.
		void adi(size_t idx);								// Add VX to index register.

		void add(size_t idx, word value);					// Add NN to VX.
		void adc(size_t idx, size_t idy);					// Add VY to VX with carry (1 - occurred, 0 - otherwise).
		void sbxyc(size_t idx, size_t idy);					// Subtract VY from VX with borrow (0 - occurref, 1 = otherwise).
		void sbyxc(size_t idx, size_t idy);					// Subtract VY from VX with borrow (0 - occurref, 1 = otherwise).
		void shr_modern(size_t idx, size_t idy);			// Store VX shifted one bit right to VX, setting VF to least significant bit first. 
		void shl_modern(size_t idx, size_t idy);			// Store VX shifted one bit left to VX, setting VF to most significant bit first.
		void shr_original(size_t idx, size_t idy);			// Store VY shifted one bit right to VX, setting VF to least significant bit first. 
		void shl_original(size_t idx, size_t idy);			// Store VY shifted one bit left to VX, setting VF to most significant bit first.

		void or(size_t idx, size_t idy);					// VX = VX  or  VY.
		void and(size_t idx, size_t idy);					// VX = VX  and VY.
		void xor(size_t idx, size_t idy);					// VX = VX  xo  VY.

		void sym(size_t idx);								// Fetch an address of the sprite that corresponds to HEX digit, that is stored in VX.
		void rnd(size_t idx, word value);					// Set VX to RND and NN value.
		
		size_t bcd(size_t idx);								// Store the value of register VX binary-decimal encoded within the memory located at [I, I + 1, I + 2].
		
		void cls();											// Clear screen.
		void sprite(size_t idx, size_t idy, word value);	// Display a sprite on screen.
		void sound(size_t idx);								// Set value of sound timer.
		void delay(size_t idx);								// Set value of delay timer.
		void key(size_t idx);								// Wait for a key hit, then write a rey index to VX.


		// ========================================================
		// complementary methods
		// ========================================================

		void push(word value);

		word pop();


		// ========================================================
		// operation code decoder
		//
		// It follows the next idea:
		// all the operations are gathered within so-called
		// clusters. This allows to avoid switch-based branching.
		// ========================================================

		struct Opcode {
			byte hi;
			byte lo;
		};

		inline size_t op0(const Opcode &opcode); inline size_t op1(const Opcode &opcode);
		inline size_t op2(const Opcode &opcode); inline size_t op3(const Opcode &opcode);
		inline size_t op4(const Opcode &opcode); inline size_t op5(const Opcode &opcode);
		inline size_t op6(const Opcode &opcode); inline size_t op7(const Opcode &opcode);
		inline size_t op8(const Opcode &opcode); inline size_t op9(const Opcode &opcode);
		inline size_t opA(const Opcode &opcode); inline size_t opB(const Opcode &opcode);
		inline size_t opC(const Opcode &opcode); inline size_t opD(const Opcode &opcode);
		inline size_t opE(const Opcode &opcode); inline size_t opF(const Opcode &opcode);

		static size_t (Interpreter::*const macroCodesLUT[0x10]) (const Opcode &);


		// ========================================================
		// Debug facility
		// ========================================================

#if defined(_DEBUG)
		public:

		class Snapshot {

			const Interpreter *soc;

		public:
			static Snapshot &obtain(Snapshot &snapshot, const Interpreter &soc) {
				snapshot.soc = &soc;

				return snapshot;
			}


			word getTimerDelay() const {
				return soc->timers[TIMER_DELAY].value;
			}
			word getTimerSound() const {
				return soc->timers[TIMER_SOUND].value;
			}

			unsigned long long getCountCycles() const {
				return soc->countCycles;
			}

			size_t getStackPointer() const {
				return soc->sp;
			}
			size_t getKeyHaltRegister() const {
				return soc->keyHaltRegister;
			}

			word getCarryValue() const {
				return soc->carry;
			}
			word getIndexValue() const {
				return soc->index;
			}
			word getProgramCounterValue() const {
				return soc->pc;
			}
			word getKeyboardValue() const {
				return soc->kb;
			}

			word getRegisterValue(size_t sp) const {
				return soc->registers[sp];
			}
			word getStackTipValue() const {
				return soc->stack[soc->sp + 1];
			}
			word getStackValue(size_t sp) const {
				return soc->stack[sp];
			}
			const byte &getMemoryValue(word address) const {
				return soc->memory[address];
			}
		};
#endif // _DEBUG
	};
} // namespace chip8

#endif // CHIP8_INTERPRETER_