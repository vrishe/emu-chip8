#pragma once

#ifndef CHIP8_BASE_
#define CHIP8_BASE_

#include <new>

typedef unsigned char	byte, *pbyte;
typedef unsigned short	word, *pword;

namespace chip8 {

	enum ByteOrder : byte {
		LittleEndian = 1,
		BigEndian = 8
	};

	inline ByteOrder GetByteOrder() {
		static const word BOM = BigEndian << 8 | LittleEndian;

		return ByteOrder(reinterpret_cast<const byte*>(&BOM)[0]);
	}

	inline bool ByteOrderIs(ByteOrder order) {
		return GetByteOrder() == order;
	}


	template <typename T>
	struct const_reference {
		typedef const T &Type;
	};
	template <typename T>
	struct const_reference<T &> {
		typedef const T &Type;
	};
	template <typename T>
	struct const_reference<T *> {
		typedef const T *Type;
	};
	template <typename T>
	struct const_reference<T[]> {
		typedef const T *Type;
	};

	template <typename SrcT, typename DstT>
	class IConverter {
	public:
		virtual void convert(typename const_reference<SrcT>::Type, DstT) const = 0;
		virtual void convert(typename const_reference<DstT>::Type, SrcT) const = 0;

		virtual ~IConverter() { /* Nothing to do */ };
	};

	typedef union {
		struct {
			byte lo;
			byte hi;
		}				order;
		byte			index[2];
		word			value;
	} Opcode;


	typedef IConverter<byte *, Opcode &> OpcodeConverterBase;

	template <ByteOrder endianness>
	class OpcodeConverter;

	template <>
	class OpcodeConverter<BigEndian> : public OpcodeConverterBase {
	public:
		void convert(const byte *bytes, Opcode &opcode) const {
			opcode = *reinterpret_cast<Opcode *>(const_cast<byte*>(bytes));
		}

		void convert(const Opcode &opcode, byte *bytes) const {
			bytes = reinterpret_cast<byte *>(const_cast<Opcode *>(&opcode));
		}

		~OpcodeConverter() {
			/* Nothing to do */
		}
	};


	template <>
	class OpcodeConverter<LittleEndian> : public OpcodeConverterBase {
	public:
		void convert(const byte *bytes, Opcode &opcode) const {
			opcode.order.hi = bytes[0];
			opcode.order.lo = bytes[1];
		}

		void convert(const Opcode &opcode, byte *bytes) const {
			bytes[0] = opcode.order.hi;
			bytes[1] = opcode.order.lo;
		}

		~OpcodeConverter() {
			/* Nothing to do */
		}
	};


	static OpcodeConverterBase *ConstructOpcodeConverter(void *converterSpace) {
		if (!!converterSpace) {
			switch (GetByteOrder())
			{
			case LittleEndian:
				return new(converterSpace) OpcodeConverter<LittleEndian>;

			case BigEndian:
				return new(converterSpace) OpcodeConverter<BigEndian>;
			}
		}
		return nullptr;
	}
} // namespace chip8

#endif // CHIP8_BASE_