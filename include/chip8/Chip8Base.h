#pragma once

#ifndef CHIP8_BASE_
#define CHIP8_BASE_

namespace chip8 {

	typedef unsigned char	byte, *pbyte;
	typedef unsigned short	word, *pword;

	typedef struct _tagOpcode {
		byte hi;
		byte lo;
	} Opcode;

} // namespace chip8

#endif // CHIP8_BASE_