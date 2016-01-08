#pragma once

#include "chip8\Chip8Keyboard.h"


namespace platform {

	using namespace chip8;


	typedef PadKeys(VKMap)[256];

	class VKMappedKeypad : public IKeyPad {
		std::atomic<PadKeys> kbstate;

		VKMap map;

	public:

		class DefaultMapping {

			VKMap map;

		public:

			DefaultMapping();

			operator const VKMap&() const {
				return map;
			}
		};


		VKMappedKeypad(const VKMap &map = DefaultMapping()) {
			memcpy(this->map, map, sizeof(map));

			kbstate = chip8::PadKeys::KEY_NONE;
		}

		void updateKey(UINT vk, BOOL fDown) {
			assert(vk < sizeof(map));

			auto result = map[vk];

			if (result != kbstate) {
				kbstate = result;
			}
		}

		virtual chip8::PadKeys getState() const {
			return kbstate.load();
		}
	};

	VKMappedKeypad::DefaultMapping::DefaultMapping() {
		map['X'] = PadKeys::KEY_0;
		map['1'] = PadKeys::KEY_1;
		map['2'] = PadKeys::KEY_2;
		map['3'] = PadKeys::KEY_3;
		map['Q'] = PadKeys::KEY_4;
		map['W'] = PadKeys::KEY_5;
		map['E'] = PadKeys::KEY_6;
		map['A'] = PadKeys::KEY_7;
		map['S'] = PadKeys::KEY_8;
		map['D'] = PadKeys::KEY_9;
		map['Z'] = PadKeys::KEY_A;
		map['C'] = PadKeys::KEY_B;
		map['4'] = PadKeys::KEY_C;
		map['R'] = PadKeys::KEY_D;
		map['F'] = PadKeys::KEY_E;
		map['V'] = PadKeys::KEY_F;
	}

} // namespace platform