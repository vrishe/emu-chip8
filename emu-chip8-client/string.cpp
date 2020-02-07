#include "stdafx.h"

#include "string.h"


namespace string {

	std::_tstring format(const _TCHAR *fmt, ...) {
		std::_tstring result;

		va_list args;
		va_start(args, fmt);

		result = std::move(format(fmt, args));

		va_end(args);

		return result;
	}

	std::_tstring format(const _TCHAR *fmt, va_list args) {

#ifdef _MSC_VER

#if (_MSC_VER > 1600)

		const _invalid_parameter_handler oldHandler = _set_invalid_parameter_handler([](
			const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t) -> void {
			/* Nothing to do */
		});

#else // _MSC_VER <= 1600

		class INVALID_PARAMETER_HANDLER {
		public:
			static void STUB(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t) {
				/* Nothing to do */
			}
		};
		const _invalid_parameter_handler oldHandler = _set_invalid_parameter_handler(INVALID_PARAMETER_HANDLER::STUB);

#endif // if _MSC_VER > 1600

		const int lastReportMode = _CrtSetReportMode(_CRT_ASSERT, 0);
		const int strLen = _vsctprintf(fmt, args);

		std::_tstring result(strLen + 1, 0x00);
		if (_vstprintf_s(&result[0], result.size(), fmt, args) == strLen) {
			result.resize(strLen);
		}
		else {
			result.resize(0);

			throw string::InvalidFormatException(_T("string::format(const _TCHAR *fmt, ...)"), fmt);
		}

		_CrtSetReportMode(_CRT_ASSERT, lastReportMode);
		_set_invalid_parameter_handler(oldHandler);

		std::_tstringstream ss;

#else // !_MSC_VER

		static_assert("Non-Windows environment is not supported yet.");

#endif // _MSC_VER

#ifdef _HAS_CPP0X
		result.shrink_to_fit();
#endif // _HAS_CPP0X

		return result;

	}
}