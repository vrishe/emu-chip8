
#ifndef INCLUDE_HEADER_STRING_13072015_
#define INCLUDE_HEADER_STRING_13072015_


#include "unicode.h"

namespace string {

	class InvalidFormatException : std::exception {

		std::_tstring msg;

	public:

		InvalidFormatException() {
			/* default ctor */
		}

		InvalidFormatException(const _TCHAR *const where, const _TCHAR *const what) {
			assert(where != NULL && what != NULL);

			std::_tstringstream sstream;

			sstream << this->what() << " { " << where << ": '" << what << "' }" << std::endl;
			msg = sstream.str();
		}

	
		const _TCHAR *const message() const {
			return msg.c_str();
		}


		const char *what() const {
			return "InvalidFormatException";
		}
	}; // class InvalidFormatException


	std::_tstring format(const _TCHAR *fmt, ...);
	std::_tstring format(const _TCHAR *fmt, va_list args);

} // namespace string

#endif // INCLUDE_HEADER_STRING_13072015_
