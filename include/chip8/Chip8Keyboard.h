#pragma once

#ifndef CHIP8_KEYBOARD_
#define CHIP8_KEYBOARD_

#include "Chip8Base.h"

namespace chip8 {

	enum PadKeys : word {
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

	class IKeyPad {
	public:
		virtual ~IKeyPad() { /* Nothing to do */ }

		virtual PadKeys getState() const = 0;
	};

} // namespace chip8

#endif // CHIP8_KEYBOARD_