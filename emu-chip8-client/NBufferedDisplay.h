#pragma once

#include "chip8\Chip8Display.h"


namespace platform {

	template<size_t n, bool = (n > 1)>
	class NBufferedDisplay;

	template <size_t n>
	class NBufferedDisplay < n, true > : public chip8::IDisplay {

		chip8::byte w, h;
		chip8::word a;

		bool invalid;

		struct tagPlane {
			chip8::rect invalidRect;

			chip8::byte *buffer;
		} planes[n];

		chip8::byte *buffer;
	
		size_t indexProducer;
		std::atomic<size_t> indexConsumer;


		NBufferedDisplay(const NBufferedDisplay<n> &);
		NBufferedDisplay(const NBufferedDisplay<n> &&);

	public:

		NBufferedDisplay(chip8::byte width = chip8::DefaultDisplay::FRAME_WIDTH,
			chip8::byte height = chip8::DefaultDisplay::FRAME_HEIGHT)
				: a(width * height), w(width), h(height) {

			assert(a > 0);
			buffer = new chip8::byte[a * n];
			for (size_t i = 0; i < n; ++i) {
				planes[i].buffer = buffer + i * a;
			}
			indexProducer = 0;
			indexConsumer = n - 1;
		}
  
		~NBufferedDisplay() {
			delete[] buffer;
		}

		chip8::word area() const {
			return a;
		}
		chip8::byte width() const {
			return w;
		}
		chip8::byte height() const {
			return h;
		}

		chip8::byte *getLine(chip8::byte index, chip8::byte offset) {
			return planes[indexProducer].buffer + index * w + offset;
		}


		chip8::byte *consume() {
			return planes[indexConsumer = (indexConsumer + 1) % n].buffer;
		}


		void validate() {
			invalid = false;
		}

		void invalidate() {
			invalidate(0, 0, w, h);
		}
		void invalidate(chip8::byte x, chip8::byte y, chip8::byte w, chip8::byte h) {		
			planes[indexConsumer].invalidRect.x = x;
			planes[indexConsumer].invalidRect.y = y;
			planes[indexConsumer].invalidRect.w = w;
			planes[indexConsumer].invalidRect.h = h;

			invalid = true;
		}

		void getInvalidArea(chip8::rect &r) const {
			r = planes[indexConsumer].invalidRect;
		}
		bool isInvalid() const {
			return invalid;
		}

		NBufferedDisplay &operator ++() {
			auto indexProducerCurrent = indexProducer;
			auto &planeCurrent = planes[indexProducerCurrent];

			do {
				indexProducer = (indexProducer + 1) % n;
			} while (indexProducer == indexConsumer);

			memcpy(planes[indexProducer].buffer, planeCurrent.buffer, a);

			return *this;
		}
	};

} // namespace platform