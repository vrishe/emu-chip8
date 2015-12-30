// emu-chip8.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "main.h"

typedef struct tagChip8DisplayExtra {
	HDC		hDC;
	HGLRC	hGLRC;
} Chip8DisplayExtra;

static BOOL Chip8DisplayCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct) {
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,	//Flags
		PFD_TYPE_RGBA,												//The kind of framebuffer. RGBA or palette.
		32,															//Colordepth of the framebuffer.
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		24,															//Number of bits for the depthbuffer
		8,															//Number of bits for the stencilbuffer
		0,															//Number of Aux buffers in the framebuffer.
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};
	Chip8DisplayExtra *extra = new Chip8DisplayExtra;
	{
		HDC hDC = GetDC(hWnd);
		HGLRC hGLRC;
		{
			SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);

			hGLRC = wglCreateContext(hDC);
		}
		wglMakeCurrent(hDC, hGLRC);

		extra->hDC = hDC;
		extra->hGLRC = hGLRC;
	}
	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)extra);

	return TRUE;
}

static VOID Chip8DisplayDestroy(HWND hWnd) {
	delete (Chip8DisplayExtra*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	PostQuitMessage(0);
}


#define CHIP8_DISPLAY_WINDOW _T("Chip8Display")

static LRESULT CALLBACK Chip8DisplayWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	switch (Msg) {
		HANDLE_MSG(hWnd, WM_CREATE,		Chip8DisplayCreate);
		HANDLE_MSG(hWnd, WM_DESTROY,	Chip8DisplayDestroy);
	};
	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

DWORD CALLBACK ApplicationInitialization(HINSTANCE hInst, int nCmdShow) {
	WNDCLASS wc = {
		CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
		Chip8DisplayWndProc,
		0,
		sizeof(Chip8DisplayExtra*),
		hInst,
		0,
		LoadCursor(NULL, IDC_ARROW),
		0,
		0,
		CHIP8_DISPLAY_WINDOW
	};
	RegisterClass(&wc);

	HWND hWnd = CreateWindow(wc.lpszClassName, CHIP8_DISPLAY_WINDOW, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInst, NULL);
	
	if (hWnd == NULL)
		return GetLastError();

	ShowWindow(hWnd, nCmdShow);

	return ERROR_SUCCESS;
}

VOID CALLBACK ApplicationUninitialization() {
	/* Nothing to do */
}


int APIENTRY _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	DWORD dwReturnError = ERROR_SUCCESS;

	try {
		ApplicationInstance::Initialize(hInstance, nCmdShow, ApplicationInitialization);
		ApplicationInstance::RunMainLoop(NULL, &dwReturnError);
	}
	catch (DWORD &dwLastError) {
		dwReturnError = dwLastError;
	}
	ApplicationInstance::Release(ApplicationUninitialization);

	return (int)dwReturnError;
}


namespace ApplicationInstance {

	namespace {
		static HANDLE		hAppMutex = INVALID_HANDLE_VALUE;
		static HINSTANCE	hInstanceCurrent = NULL;

		// For the case when the Operating System is not supported
		static bool bPreventRelease = false;
	}

	HINSTANCE GetCurrentInstance() {
		_ASSERT_EXPR(hInstanceCurrent != NULL, _T("Application instance hasn't been initialized yet!"));
		return hInstanceCurrent;
	}


	VOID Initialize(HINSTANCE hInstance, int nCmdShow, LPINIT_FUNC lpInitFunc) {
		SetLastError(ERROR_SUCCESS);

		OSVERSIONINFOEX versionInfoEx;
		{
			versionInfoEx.dwMajorVersion = 5;
			versionInfoEx.dwMinorVersion = 1;

			versionInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		}
		ULONGLONG conditionMask = 0;
		{
			conditionMask = VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
			conditionMask = VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
		}
		if (!VerifyVersionInfo(&versionInfoEx, VER_MAJORVERSION | VER_MINORVERSION, conditionMask)) {
			bPreventRelease = true;

			MessageBox(NULL, _T("Sorry, your operating system is not supported."), _T("Fatal"), MB_ICONASTERISK | MB_OK);
			throw GetLastError();
		}
		// Check whether an attempt to run the application several times
#pragma warning( push )
#pragma warning( disable : 4996 )
		std::_tstring mutexName(_tpgmptr);
		mutexName = mutexName.substr(mutexName.find_last_of(_T('\\')) + 1);
#pragma warning ( pop )
		hAppMutex = CreateMutex(NULL, TRUE, mutexName.c_str()); {
			DWORD dwLastError = GetLastError();
			if (dwLastError != ERROR_SUCCESS)
				throw dwLastError;
		}
		// Store instance handle in our global variable
		hInstanceCurrent = hInstance;

		if (lpInitFunc != NULL) {
			DWORD dwLastError = lpInitFunc(hInstance, nCmdShow);
			if (dwLastError != ERROR_SUCCESS)
				throw dwLastError;
		}
	}

	VOID RunMainLoop(LPINTERCEPT_FUNC lpInterceptFunc, LPDWORD lpExitCode) {
		MSG  msg;
		BOOL bRet;

		const LPMSG lpMsg = &msg;
		while (!!(bRet = GetMessage(lpMsg, NULL, 0, 0))) {
			if (bRet == -1) throw GetLastError();

			if (lpInterceptFunc == NULL || !lpInterceptFunc(lpMsg)) {
				TranslateMessage(lpMsg);
				DispatchMessage(lpMsg);
			}
		}
		if (lpExitCode != NULL) {
			*lpExitCode = msg.wParam;
		}
	}

	VOID Release(LPRELEASE_FUNC lpReleaseFunc) {
		if (!bPreventRelease) {
			if (lpReleaseFunc != NULL) {
				lpReleaseFunc();
			}
			CloseHandle(hAppMutex);

			hInstanceCurrent = NULL;
			hAppMutex = INVALID_HANDLE_VALUE;
		}
	}
} // namespace ApplicationInstance

