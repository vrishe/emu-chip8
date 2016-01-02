
#ifndef INCLUDE_HEADER_UNICODE_13072015_
#define INCLUDE_HEADER_UNICODE_13072015_


#include <string>

#ifdef _MSC_VER
#include <tchar.h>
#endif // _MSC_VER

#if defined(UNICODE) || defined(_UNICODE)

#	define _tstring			wstring
#	define _tstringstream	wstringstream

#ifdef _MSC_VER
#	define CF_TTEXT	CF_UNICODETEXT
#else

#ifndef __TCHAR_DEFINED
typedef wchar_t _TCHAR;
#define __TCHAR_DEFINED
#endif // __TCHAR_DEFINED

#endif // _MSC_VER

#else // defined(UNICODE) || defined(_UNICODE)
#	define _tstring			string
#	define _tstringstream	stringstream

#ifdef _MSC_VER
#	define CF_TTEXT	CF_TEXT
#else

#ifndef __TCHAR_DEFINED
typedef char _TCHAR;
#define __TCHAR_DEFINED
#endif // __TCHAR_DEFINED

#endif // _MSC_VER

#endif // defined(UNICODE) || defined(_UNICODE)

#endif // INCLUDE_HEADER_UNICODE_13072015_