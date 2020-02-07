#include "chip8\Chip8Display.h"

#include <cassert>


namespace chip8 {

	void DisplayBase::validate() {
		invalid = false;
	}

	void DisplayBase::invalidate() {
		invalidArea.x = 0;
		invalidArea.y = 0;
		invalidArea.w = width();
		invalidArea.h = height();

		invalid = true;
	}
	void DisplayBase::invalidate(byte x, byte y, byte w, byte h) {
		invalidArea.x = x;
		invalidArea.y = y;
		invalidArea.w = w;
		invalidArea.h = h;

		invalid = true;
	}


	void DisplayBase::getInvalidArea(rect &r) const {
		r = invalidArea;
	}

	bool DisplayBase::isInvalid() const {
		return invalid;
	}


	DefaultDisplay::DefaultDisplay(byte width, byte height)
		: w(width), h(height), a(width * height) {

		assert(a > 0);
		buffer = new byte[a];
	}

	DefaultDisplay::~DefaultDisplay() {
		delete[] buffer;
	}


	word DefaultDisplay::area() const {
		return a;
	}

	byte DefaultDisplay::width() const {
		return w;
	}

	byte DefaultDisplay::height() const {
		return h;
	}


	byte *DefaultDisplay::getLine(byte index, byte offset) {
		return buffer + index * w + offset;
	}

} // namespace chip8