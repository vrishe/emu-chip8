#pragma once

#ifndef CHIP8_BASE_
#define CHIP8_BASE_

namespace chip8 {

	typedef unsigned char	byte, *pbyte;
	typedef unsigned short	word, *pword;

	typedef unsigned long long clock;

	typedef struct rect_ {
		byte x, y, w, h;
	} rect, *prect;

} // namespace chip8

#endif // CHIP8_BASE_