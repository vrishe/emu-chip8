#pragma once 

#ifndef CHIP8_INTERPRETER_
#define CHIP8_INTERPRETER_

#include "Chip8Base.h"
#include "Chip8Timer.h"

#include <istream>

namespace chip8 {

	class Interpreter {
	public:

		static byte font[0x50];

		enum : word {
			STACK_DEPTH				= 0x20,
			REGISTERS_COUNT			= 0x10,

			FRAME_HEIGHT			= 0x20,
			FRAME_WIDTH				= 0x40,

			OFFSET_PROGRAM_START	= 0x200,
			ADDRESS_SPACE_DEFAULT	= 0x1000,
			
			ALU_PROFILE_ORIGINAL	= 0x1F01,
			ALU_PROFILE_MODERN		= 0x1F02
		};

		typedef byte(Frame)[FRAME_HEIGHT * FRAME_WIDTH];


		enum Keyboard : word {
			KEY_NONE = 0,
			KEY_0 = 0x0001,
			KEY_1 = 0x0002,
			KEY_2 = 0x0004,
			KEY_3 = 0x0008,
			KEY_4 = 0x0010,
			KEY_5 = 0x0020,
			KEY_6 = 0x0040,
			KEY_7 = 0x0080,
			KEY_8 = 0x0100,
			KEY_9 = 0x0200,
			KEY_A = 0x0400,
			KEY_B = 0x0800,
			KEY_C = 0x1000,
			KEY_D = 0x2000,
			KEY_E = 0x4000,
			KEY_F = 0x8000
		};
	

		Interpreter(word aluProfile = ALU_PROFILE_MODERN, size_t memorySize = ADDRESS_SPACE_DEFAULT);

		~Interpreter();


		word getALUProfile() const {
			return aluProfile;
		}


		void doCycle(Keyboard key);

		Frame &getFrame() {
			frameUpdate = false;

			return frame;
		}


		void reset() {
			reset_impl();
		}

		template <size_t Count>
		void reset(const byte(&prg)[Count]) {
			reset(prg, Count);
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
			return frameUpdate;
		}

		bool isOk() const {
			return !failState;
		}


	private:

		Interpreter(const Interpreter&);


		typedef void (Interpreter::*ALUFunc) (size_t, size_t);

		ALUFunc shl;
		ALUFunc shr;


		const word aluProfile;

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

		Frame frame;
		bool frameUpdate;

		const size_t memorySize;
		byte *memory;


		bool failState;


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


		void key(size_t idx);								// Wait for a key hit, then write a rey index to VX.

		void movn(size_t idx, word value);					// Set VX to NN value.
		void movy(size_t idx, size_t idy);					// Set VX to VY.
		void movd(size_t idx);								// Set VX to delay timer value.
		void movrs(size_t idx);								// Store the memory values that start at I into registers V0 trough VX.
		void movms(size_t idx);								// Store the values of registerx V0 through VX within memory, starting at I.

		void sti(word value);								// Store value to index register.

		size_t bcd(size_t idx);								// Store the value of register VX binary-decimal encoded within the memory located at [I, I + 1, I + 2].

		void add(size_t idx, word value);					// Add NN to VX.
		void adc(size_t idx, size_t idy);					// Add VY to VX with carry (1 - occurred, 0 - otherwise).
		void adi(size_t idx);
		void sbxyc(size_t idx, size_t idy);					// Subtract VY from VX with borrow (0 - occurref, 1 = otherwise).
		void sbyxc(size_t idx, size_t idy);					// Subtract VY from VX with borrow (0 - occurref, 1 = otherwise).

		void or(size_t idx, size_t idy);					// VX = VX  or  VY.
		void and(size_t idx, size_t idy);					// VX = VX  and VY.
		void xor(size_t idx, size_t idy);					// VX = VX  xo  VY.

		void shr_modern(size_t idx, size_t idy);			// Store VX shifted one bit right to VX, setting VF to least significant bit first. 
		void shl_modern(size_t idx, size_t idy);			// Store VX shifted one bit left to VX, setting VF to most significant bit first.
		void shr_original(size_t idx, size_t idy);			// Store VY shifted one bit right to VX, setting VF to least significant bit first. 
		void shl_original(size_t idx, size_t idy);			// Store VY shifted one bit left to VX, setting VF to most significant bit first.

		void rnd(size_t idx, word value);					// Set VX to RND and NN value.

		void sound(size_t idx);								// Set value of sound timer.
		void delay(size_t idx);								// Set value of delay timer.


		void flush();										// Clear the screen.
		void sprite(size_t idx, size_t idy, word value);	// Display a sprite on screen.

		void sym(size_t idx);								// Fetch an address of the sprite that corresponds to HEX digit, that is stored in VX.


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
			const byte &getFrameValue(word x, word y) const {
				return soc->frame[y * FRAME_WIDTH + x];
			}
			const byte &getFrameValue(word i) const {
				return soc->frame[i];
			}
		};
#endif // _DEBUG
	};
} // namespace chip8

#endif // CHIP8_INTERPRETER_