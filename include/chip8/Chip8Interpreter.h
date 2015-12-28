#pragma once 

#ifndef CHIP8_INTERPRETER_
#define CHIP8_INTERPRETER_

#include "Chip8Base.h"
#include "Chip8Timer.h"

#include <istream>

namespace chip8 {

	class Interpreter {
	public:
		enum : word {
			STACK_DEPTH				= 0x10,
			REGISTERS_COUNT			= 0x10,

			FRAME_HEIGHT			= 0x20,
			FRAME_WIDTH				= 0x40,

			OFFSET_FONT_DEFAULT		= 0x50,
			OFFSET_PROGRAM_START	= 0x200,

			ADDRESS_SPACE_MAX		= 0x1000,
			KEY_HALT_UNSET			= word(-1)
		};

		enum Keyboard : word {
			KEY_0 = 0x00,
			KEY_1 = 0x01,
			KEY_2 = 0x02,
			KEY_3 = 0x03,
			KEY_4 = 0x04,
			KEY_5 = 0x05,
			KEY_6 = 0x06,
			KEY_7 = 0x07,
			KEY_8 = 0x08,
			KEY_9 = 0x09,
			KEY_A = 0x0A,
			KEY_B = 0x0B,
			KEY_C = 0x0C,
			KEY_D = 0x0D,
			KEY_E = 0x0E,
			KEY_F = 0x0F,
		};

		class HardwareHandler {
		public:
			virtual void handle(word address) = 0;

			virtual ~HardwareHandler() { /* Nothing to do */ };
		};


		static byte font[0x50];


		Interpreter(HardwareHandler *hardwareHandler = nullptr);

		~Interpreter();


		void doCycle();


		void reset() {
			reset_impl();
		}

		template <size_t Count>
		void reset(const byte(&prg)[Count]) {
			reset(prg, Count);
		}
		void reset(const byte *prg, size_t prgLen) {
			reset_impl();

			byte *clientMemory = memory + pc;

			if (!!prg) {
				memcpy(clientMemory, prg, prgLen);
			}
			else {
				memset(clientMemory, 0x00, ADDRESS_SPACE_MAX - pc);

				pc = 0x00;
			}
		}

		void reset(std::istream &prgStream);


		bool isKeyAwaited() const {
			return keyHaltRegister != KEY_HALT_UNSET;
		}

		void setKeyHit(Keyboard key);


		typedef byte(&Frame)[FRAME_HEIGHT * FRAME_WIDTH];

		bool isFrameUpdated() const {
			return frameUpdate;
		}

		Frame &getFrame();


	private:

		enum : size_t {
			TIMER_DELAY = 0,
			TIMER_SOUND = 1
		};

		enum : unsigned long long {
			TIMESTAMP_UNDEFINED = ~0ull
		};

		struct countdown_timer {
			unsigned long long timestamp;

			word value;
		};

		countdown_timer timers[2];

		unsigned long long countCycles;

		OpcodeConverterBase *opcodeConverter;
		HardwareHandler		*hardwareHandler;

		size_t sp;
		size_t keyHaltRegister;

		word &carry;
		word index;
		word pc;
		word kb;

		word rndSeed;
		
		word stack[STACK_DEPTH];
		word registers[REGISTERS_COUNT];

		byte memory[ADDRESS_SPACE_MAX];
		byte frame[FRAME_WIDTH * FRAME_HEIGHT];

		bool frameUpdate;


		void reset_impl();


		void refreshTimers();
		void onTick(word timerId);


		// ========================================================
		// program interpretation
		// ========================================================

		void rca(word address);								// Call RCA 1802 program.

		void jmp(word address);								// Jump straight to address specified.
		void jmp0(word address);							// Jump to address + V0 location.
		void call(word address);							// Call a routine on address specified (current sp is placed on stack).
		void ret();											// Return after routine call (retrieves a previously stored pc from stack).

		void se(size_t idx, word value);					// Skip, if VX equal NN.
		void sne(size_t idx, word value);					// Skip, if VX NOT equal NN.
		void sxye(size_t idx, size_t idy);					// Skip, if VX equal VY.
		void sxyne(size_t idx, size_t idy);					// Skip, if VX NOT equal VY.
		void skbh(size_t idx);								// Skip, if keyboard key by value of VX IS pressed.
		void skbnh(size_t idx);								// Skip, if keyboard key by value of VX IS NOT pressed.


		void key(size_t idx);								// Wait for a key hit, then write a rey index to VX.

		void movn(size_t idx, word value);					// Set VX to NN value.
		void movy(size_t idx, size_t idy);					// Set VX to VY.
		void movd(size_t idx);								// Set VX to delay timer value.
		void movrs(size_t idx);								// Store the memory values that start at I into registers V0 trough VX.
		void movms(size_t idx);								// Store the values of registerx V0 through VX within memory, starting at I.

		void sti(word value);								// Store value to index register.

		void bcd(size_t idx);								// Store the value of register VX binary-decimal encoded within the memory located at [I, I + 1, I + 2].

		void add(size_t idx, word value);					// Add NN to VX.
		void adc(size_t idx, size_t idy);					// Add VY to VX with carry (1 - occurred, 0 - otherwise).
		void adi(size_t idx);
		void sbxyc(size_t idx, size_t idy);					// Subtract VY from VX with borrow (0 - occurref, 1 = otherwise).
		void sbyxc(size_t idx, size_t idy);					// Subtract VY from VX with borrow (0 - occurref, 1 = otherwise).

		void or(size_t idx, size_t idy);					// VX = VX  or  VY.
		void and(size_t idx, size_t idy);					// VX = VX  and VY.
		void xor(size_t idx, size_t idy);					// VX = VX  xo  VY.

		void shr(size_t idx, size_t idy);					// Store VY shifted one bit right, setting VF to least significant bit first. 
		void shl(size_t idx, size_t idy);					// Store VY shifted one bit left, setting VF to most significant bit first.

		void rnd(size_t idx, word value);					// Set VX to RND and NN value.

		void sound(size_t idx);								// Set value of sound timer.
		void delay(size_t idx);								// Set value of delay timer.


		void flush();										// Clear the screen.
		void sprite(size_t idx, size_t idy, word value);	// Display a sprite on screen.

		void sym(size_t idx);								// Fetch an address of the sprite that corresponds to HEX digit, that is stored in VX.


		// ========================================================
		// complementary methods
		// ========================================================

		void push(word value) {
			stack[--sp] = value;
		}

		word pop() {
			return stack[sp++];
		}


		// ========================================================
		// operation code decoder
		//
		// It follows the next idea:
		// all the operations are gathered within so-called
		// clusters. This allows to avoid switch-based branching.
		// ========================================================

		inline void op0(const Opcode &opcode); inline void op1(const Opcode &opcode);
		inline void op2(const Opcode &opcode); inline void op3(const Opcode &opcode);
		inline void op4(const Opcode &opcode); inline void op5(const Opcode &opcode);
		inline void op6(const Opcode &opcode); inline void op7(const Opcode &opcode);
		inline void op8(const Opcode &opcode); inline void op9(const Opcode &opcode);
		inline void opA(const Opcode &opcode); inline void opB(const Opcode &opcode);
		inline void opC(const Opcode &opcode); inline void opD(const Opcode &opcode);
		inline void opE(const Opcode &opcode); inline void opF(const Opcode &opcode);

		static void (Interpreter::*const macroCodesLUT[0x10]) (const Opcode &);


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
			byte getMemoryValue(word address) const {
				return soc->memory[address];
			}
			byte getFrameValue(word x, word y) const {
				return soc->frame[y * FRAME_WIDTH + x];
			}
			byte getFrameValue(word i) const {
				return soc->frame[i];
			}
		};
#endif // _DEBUG
	};
} // namespace chip8

#endif // CHIP8_INTERPRETER_