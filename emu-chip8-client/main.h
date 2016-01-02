#pragma once

#include "resource.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

typedef DWORD	(CALLBACK* LPINIT_FUNC)			(HINSTANCE, int);
typedef BOOL	(CALLBACK* LPINTERCEPT_FUNC)	(const LPMSG msg);
typedef VOID	(CALLBACK* LPRELEASE_FUNC)		();

namespace ApplicationInstance {

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