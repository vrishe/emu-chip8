#pragma once

#include "resource.h"
#include "logger.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

typedef DWORD	(CALLBACK* LPINIT_FUNC)			(HINSTANCE, int);
typedef BOOL	(CALLBACK* LPINTERCEPT_FUNC)	(const LPMSG msg);
typedef VOID	(CALLBACK* LPRELEASE_FUNC)		();

namespace ApplicationInstance {

	class WindowsLogger : public logger::ILogger {

		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

	public:
		void printLn() {
			OutputDebugString(_T("\n"));
		}
		void printLn(const std::string &text) {
			OutputDebugString(string::format(_T("%d: "), GetTickCount64()).c_str());
			OutputDebugString(
#ifndef UNICODE
				text.c_str()
#else
				converter.from_bytes(text).c_str()
#endif // UNICODE
				);
			printLn();
		}
		void printLn(const std::wstring &text) {
			OutputDebugString(string::format(_T("%d: "), GetTickCount64()).c_str());
			OutputDebugString(
#ifdef UNICODE
				text.c_str()
#else
				converter.to_bytes(text).c_str()
#endif // UNICODE
				);
			printLn();
		}
		void printLn(const std::string &text, va_list args) {
			printLn(string::format(
#ifndef UNICODE
				text.c_str()
#else
				converter.from_bytes(text).c_str()
#endif // UNICODE
				, args));
		}
		void printLn(const std::wstring &text, va_list args) {
			printLn(string::format(
#ifdef UNICODE
				text.c_str()
#else
				converter.to_bytes(text).c_str()
#endif // UNICODE
				, args));
		}
	};

	VOID Initialize(HINSTANCE hInstance, int nCmdShow, LPINIT_FUNC lpInitFunc);
	VOID RunMainLoop(LPINTERCEPT_FUNC lpInterceptFunc, LPDWORD lpExitCode);
	VOID Release(LPRELEASE_FUNC lpReleaseFunc);

	HINSTANCE GetCurrentInstance();

	inline LPCTSTR GetString(UINT resId) {
		LPWSTR result = NULL;
		{
			LoadString(GetCurrentInstance(), resId, (LPWSTR)&result, 0);
		}
		return result;
	}
} // namespace ApplicationInstance