#pragma once

#include "chip8\Chip8Display.h"


namespace platform {

	template <size_t bufferSize, bool = (bufferSize > 0)>
	class RingBufferedDisplay;


	using namespace chip8;

	template<size_t bufferSize>
	class RingBufferedDisplay<bufferSize, true> : public DisplayBase {

	public:

		RingBufferedDisplay(chip8::byte width = DefaultDisplay::FRAME_WIDTH,
			chip8::byte height = DefaultDisplay::FRAME_HEIGHT) {

			assert(width > 0 && height > 0);

			this->frameWidth	= width;
			this->frameHeight	= height;
			this->frameArea		= width * height;

			buffer = new chip8::byte[frameArea * bufferSize];
			memset(buffer, 0x00, frameArea * bufferSize);

			producerIndex = 0;
			consumerIndex = 0;
		}

		~RingBufferedDisplay() {
			delete[] buffer;
		}


		chip8::word area() const {
			return frameArea;
		}
		chip8::byte width() const {
			return frameWidth;
		}
		chip8::byte height() const {
			return frameHeight;
		}


		void invalidate() {
			DisplayBase::invalidate();

			do {
				producerIndex = (producerIndex + 1) % bufferSize;
			} while (producerIndex == consumerIndex);
		}

		void invalidate(chip8::byte x, chip8::byte y, chip8::byte w, chip8::byte h) {
			DisplayBase::invalidate(x, y, w, h);

			do {
				producerIndex = (producerIndex + 1) % bufferSize;
			} while (producerIndex == consumerIndex);
		}

		const chip8::byte *consume() {
			const chip8::byte *result = buffer + consumerIndex * area();
			{
				consumerIndex = (consumerIndex + 1) % bufferSize;

				validate();
			}
			return result;
		}


		chip8::byte *getLine(chip8::byte index, chip8::byte offset) {
			return buffer + producerIndex * area() + index * width() + offset;
		}

		operator chip8::byte*() {
			return buffer + producerIndex * area();
		}

	private:

		chip8::byte frameWidth;
		chip8::byte frameHeight;

		chip8::word frameArea;


		std::atomic<size_t> consumerIndex;
		size_t producerIndex;

		chip8::byte *buffer;
	};

} // namespace platform