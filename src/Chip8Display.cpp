#include "chip8\Chip8Display.h"

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
		invalidArea.x = 0;
		invalidArea.y = 0;
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


	word DefaultDisplay::area() const {
		return FRAME_WIDTH * FRAME_HEIGHT;
	}

	byte DefaultDisplay::width() const {
		return FRAME_WIDTH;
	}

	byte DefaultDisplay::height() const {
		return FRAME_HEIGHT;
	}


	byte *DefaultDisplay::getLine(byte index, byte offset) {
		return frame + index * FRAME_WIDTH + offset;
	}

} // namespace chip8