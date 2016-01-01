// emu-chip8.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "main.h"
#include "chip8\Chip8Interpreter.h"

typedef struct tagChip8DisplayExtra {
	HDC		hDC;
	HGLRC	hGLRC;

	GLuint screenVBO;
	GLuint screenEBO;
	GLuint screenTex;
	GLuint screenShaderV;
	GLuint screenShaderF;
	GLuint screenShaderPrg;
} Chip8DisplayExtra;

typedef struct tagPoint2D {
	GLfloat x;
	GLfloat y;

	GLfloat u;
	GLfloat v;
} Point2D;

static chip8::Interpreter::Frame testFrame;

// Shader sources
static const GLchar* vertexSource =
	"#version 150 compatibility\n"
	"out vec2 frameCoord;"
	"\n"
	"void main() {"
	"	frameCoord = vec2(gl_MultiTexCoord0[0], 1 - gl_MultiTexCoord0[1]);"
	"   gl_Position = ftransform();"
	"}";
static const GLchar* fragmentSource =
	"#version 150 compatibility\n"
	"uniform sampler2D frame;"
	"in vec2 frameCoord;"
	"\n"
	"void main() {"
	"	gl_FragColor = texture2D(frame, frameCoord).rrra;"
	"}";

void PrintOpenGLVersion();
void PrintOpenGLShaderLog(GLuint shaderId);

#ifdef _DEBUG
#define PRINT_OPENGL_VERSIOND()				PrintOpenGLVersion()
#define PRINT_OPENGL_SHADER_LOG(shaderId)	PrintOpenGLShaderLog(shaderId);
#else
#define PRINT_OPENGL_VERSIOND()
#define PRINT_OPENGL_SHADER_LOG(shaderId)
#endif // _DEBUG

static void CreateGLContext(HDC hDC, Chip8DisplayExtra *extra) {
	HGLRC hGLRC;
	{
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
		auto pixelFormat = ChoosePixelFormat(hDC, &pfd);

		SetPixelFormat(hDC, pixelFormat, &pfd);

		hGLRC = wglCreateContext(hDC);
	}
	wglMakeCurrent(hDC, hGLRC);

	PRINT_OPENGL_VERSIOND();
	glewInit();

	glDisable(GL_DEPTH_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	extra->hDC = hDC;
	extra->hGLRC = hGLRC;
}

static void DestroyGLContext(Chip8DisplayExtra *extra) {
	wglMakeCurrent(extra->hDC, NULL);
	wglDeleteContext(extra->hGLRC);
}

static void CreateGLScene(Chip8DisplayExtra *extra, double l, double t, double r, double b) {
	Point2D points[] = {
		{ l, t, 0, 1 },
		{ r, t, 1, 1 },
		{ r, b, 1, 0 },
		{ l, b, 0, 0 },
	};
	GLuint element[] = {
		0, 1, 2,
		2, 3, 0
	};
	GLuint idVBO;
	GLuint idEBO;

	glGenBuffers(1, &idVBO);
	glGenBuffers(1, &idEBO);
	glBindBuffer(GL_ARRAY_BUFFER, idVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idEBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(element), element, GL_STATIC_DRAW);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2, GL_FLOAT, sizeof(Point2D), NULL);
	glTexCoordPointer(2, GL_FLOAT, sizeof(Point2D), (void*)(2 * sizeof(GLfloat)));

	GLuint idTex;
	GLuint idShaderV, idShaderF, idShaderPrg;

	glGenTextures(1, &idTex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, idTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, chip8::Interpreter::FRAME_WIDTH, 
		chip8::Interpreter::FRAME_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, testFrame);

	idShaderV = glCreateShader(GL_VERTEX_SHADER);
	idShaderF = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(idShaderV, 1, &vertexSource,   NULL); glCompileShader(idShaderV); PRINT_OPENGL_SHADER_LOG(idShaderV);
	glShaderSource(idShaderF, 1, &fragmentSource, NULL); glCompileShader(idShaderF); PRINT_OPENGL_SHADER_LOG(idShaderF);

	idShaderPrg = glCreateProgram();
	glAttachShader(idShaderPrg, idShaderV);
	glAttachShader(idShaderPrg, idShaderF);
	glLinkProgram(idShaderPrg);

	int linked;
	glGetProgramiv(idShaderPrg, GL_LINK_STATUS, &linked);
	assert(linked);
	{
		GLint uniformFrame = glGetUniformLocation(idShaderPrg, "frame");

		glUniform1i(uniformFrame, 0);
	}
	glUseProgram(idShaderPrg);

	extra->screenVBO = idVBO;
	extra->screenEBO = idEBO;
	extra->screenTex = idTex;
	extra->screenShaderV = idShaderV;
	extra->screenShaderF = idShaderF;
	extra->screenShaderPrg = idShaderPrg;
}

static void DestroyGLScene(Chip8DisplayExtra *extra) {
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glUseProgram(0);
	glDeleteProgram(extra->screenShaderPrg);
	glDeleteShader(extra->screenShaderF);
	glDeleteShader(extra->screenShaderV);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &extra->screenEBO);
	glDeleteBuffers(1, &extra->screenVBO);
}

static DWORD time;

static void CALLBACK TestTickHandler() {
	testFrame[16] ^= 0xFF;

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, chip8::Interpreter::FRAME_WIDTH,
		chip8::Interpreter::FRAME_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, testFrame);
}

static void DrawGLScene(Chip8DisplayExtra *extra) {
	glClear(GL_COLOR_BUFFER_BIT);

	DWORD currentTime = GetTickCount();

	if (currentTime - time > 500) {
		time = currentTime;

		TestTickHandler();		
	}
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}


#define ASSIGN_DISPLAY_EXTRA(hwnd, extraPtr)	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)extraPtr)
#define OBTAIN_DISPLAY_EXTRA(hwnd)				((Chip8DisplayExtra*)GetWindowLongPtr(hWnd, GWLP_USERDATA))

static BOOL Chip8DisplayCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct) {

	auto extra = new Chip8DisplayExtra;
	{
		CreateGLContext(GetDC(hWnd), extra);
		CreateGLScene(extra, 0, 0, 0, 0);				// Not sure we need this.
	}
	ASSIGN_DISPLAY_EXTRA(hWnd, extra);

	return TRUE;
}

static VOID Chip8DisplayDestroy(HWND hWnd) {
	auto extra = OBTAIN_DISPLAY_EXTRA(hWnd);
	{
		DestroyGLScene(extra);
		DestroyGLContext(extra);

		ReleaseDC(hWnd, extra->hDC);
	}
	delete extra;

	PostQuitMessage(0);
}

static VOID Chip8DisplayPaint(HWND hWnd) {
	auto extra = OBTAIN_DISPLAY_EXTRA(hWnd);
	{
		DrawGLScene(extra);
	}
	SwapBuffers(extra->hDC);
}

static VOID Chip8DisplaySize(HWND hWnd, UINT state, int cx, int cy) {
	glViewport(0, 0, cx, cy);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	double l = -1.0, t = 1.0, r = 1.0, b = -1.0;

	if (cx > cy) {
		auto aspect = cx / (double)cy;

		l *= aspect;
		r *= aspect;
	}
	else if (cy > cx) {
		auto aspect = cy / (double)cx;

		b *= aspect;
		t *= aspect;
	}
	gluOrtho2D(l, r, b, t);

	auto extra = OBTAIN_DISPLAY_EXTRA(hWnd);

	DestroyGLScene(extra);
	CreateGLScene(extra, l, t, r, b);
}

#ifdef _DEBUG
static void PrintOpenGLVersion() {
	LPTSTR glVersionText;
	{
		auto glVer = std::string(reinterpret_cast<LPCCH>(glGetString(GL_VERSION)));

#ifdef UNICODE
		auto size = MultiByteToWideChar(CP_ACP, 0, glVer.c_str(), glVer.size(), NULL, 0);

		glVersionText = new TCHAR[size];
		MultiByteToWideChar(CP_ACP, 0, glVer.c_str(), glVer.size(), glVersionText, size);
#else
		auto size = glVer.size();
		glVersionText = new TCHAR[size];

		memcpy_s(glVersionText, size, glVer.c_str(), size);
#endif // UNICODE
	}
	OutputDebugString(_T("OpenGL version: "));
	OutputDebugString(glVersionText);
	OutputDebugString(_T("\n\n"));

	delete[] glVersionText;
}

void PrintOpenGLShaderLog(GLuint shaderId) {
	int infologLen = 0;

	glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infologLen);

	if (infologLen > 0) {
		char *infoLog = new GLchar[infologLen];

		if (infoLog == NULL)
		{
			OutputDebugString(_T("ERROR: Could not allocate InfoLog buffer\n"));

			exit(1);
		}
		else {
			int charsWritten;
			glGetShaderInfoLog(shaderId, infologLen, &charsWritten, infoLog);

			LPTSTR shaderLogText;
			{
				int size;

#ifdef UNICODE
				size = MultiByteToWideChar(CP_ACP, 0, infoLog, charsWritten, NULL, 0);

				shaderLogText = new TCHAR[size];
				MultiByteToWideChar(CP_ACP, 0, infoLog, charsWritten, shaderLogText, size);
#else
				size = charsWritten;
				shaderLogText = new TCHAR[size];

				memcpy_s(shaderLogText, size, infoLog, size);
#endif // UNICODE
			}			
			OutputDebugString(_T("Shader info log:"));
			OutputDebugString(shaderLogText);
			OutputDebugString(_T("\n\n"));

			delete[] shaderLogText;
		}
		delete[] infoLog;
	}
}
#endif


#define CHIP8_DISPLAY_WINDOW _T("Chip8Display")

static LRESULT CALLBACK Chip8DisplayWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	switch (Msg) {
		HANDLE_MSG(hWnd, WM_CREATE,		Chip8DisplayCreate);
		HANDLE_MSG(hWnd, WM_DESTROY,	Chip8DisplayDestroy);
		HANDLE_MSG(hWnd, WM_SIZE,		Chip8DisplaySize);
		HANDLE_MSG(hWnd, WM_PAINT,		Chip8DisplayPaint);
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

	auto windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_SIZEBOX | WS_MAXIMIZEBOX);

	RECT windowRect = { 0, 0, 640, 320 };
	AdjustWindowRect(&windowRect, windowStyle, FALSE);

	HWND hWnd = CreateWindow(wc.lpszClassName, _T("CHIP-8"), windowStyle, CW_USEDEFAULT, 0, 
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, NULL, NULL, hInst, NULL);
	
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

