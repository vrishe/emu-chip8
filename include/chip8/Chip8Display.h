#pragma once

#ifndef CHIP8_DISPLAY_
#define CHIP8_DISPLAY_

#include "Chip8Base.h"

namespace chip8 {

	class IDisplay {
	public:
		virtual ~IDisplay() { /* Nothing to do */ }

		virtual word area() const = 0;
		virtual byte width() const = 0;
		virtual byte height() const = 0;

		virtual byte *getLine(byte index, byte offset) = 0;
		
		virtual void invalidate() = 0;
		virtual void invalidate(byte x, byte y, byte w, byte h) = 0;

		virtual void getInvalidArea(rect &r) const = 0;
		virtual bool isInvalid() const = 0;

		operator bool() const {
			return isInvalid();
		}
		virtual operator byte*() {
			return getLine(0, 0);
		}
	};

	class DisplayBase : public IDisplay {
		bool invalid;
		rect invalidArea;

	public:

		void validate();

		void invalidate();
		void invalidate(byte x, byte y, byte w, byte h);

		void getInvalidArea(rect &r) const;
		bool isInvalid() const;
	};

	class DefaultDisplay : public DisplayBase {		

	public:

		enum : byte {
			FRAME_HEIGHT = 0x20,
			FRAME_WIDTH  = 0x40
		};

		typedef byte(Frame)[FRAME_WIDTH * FRAME_HEIGHT];

		word area() const;
		byte width() const;
		byte height() const;

		byte *getLine(byte index, byte offset);

	private:

		Frame frame;
	};

} // namespace chip8


#endif // CHIP8_DISPLAY_