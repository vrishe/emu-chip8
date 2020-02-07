// emu-chip8.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "chip8\Chip8Interpreter.h"
#include "chip8\Chip8Display.h"

#include "NBufferedDisplay.h"
#include "VKMappedKeyPad.h"
#include "QueueThread.h"

#include "main.h"
#include "configuration.h"


///////////////////////////////////////////////////////////////////////////////////////
// Interpretation
///////////////////////////////////////////////////////////////////////////////////////


#define WM_INTERPRETATION (WM_USER + 1024)
// void InterpretationHandler(HWND hwnd, UINT what)
#define HANDLE_WM_INTERPRETATION(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (UINT)(wParam), lParam), 0L)

#define INTERPRETATION_EVENT_DISPLAY	1
#define INTERPRETATION_EVENT_SPEAKER	2
#define INTERPRETATION_EVENT_ERROR		3


class Interpretation {

	platform::NBufferedDisplay<4>	*display;
	platform::VKMappedKeypad		*keypad;
	chip8::Interpreter				*interpreter;
	platform::QueueThread			*executionThread;

	HWND hWndOwner;

	chip8::clock cyclesPerFrame;
	chip8::clock cycles;

	LARGE_INTEGER ticksPerFrame;
	LARGE_INTEGER ticks, ticksCurrent;

	void threadFunc() {
		bool isPlayingSound = interpreter->isPlayingSound();

		cycles = 0;
		while (cycles < cyclesPerFrame) {
			if (interpreter->isOk()) {
				cycles += interpreter->doCycle();
			}
			else {
				PostMessage(hWndOwner, WM_INTERPRETATION, INTERPRETATION_EVENT_ERROR, interpreter->getLastError());
				pause();

				return;
			}
		}
		if (display->isInvalid()) {
			PostMessage(hWndOwner, WM_INTERPRETATION, INTERPRETATION_EVENT_DISPLAY, 0);

			(++*display).validate();
		}
		interpreter->refreshTimers();

		if (interpreter->isPlayingSound() != isPlayingSound) {
			PostMessage(hWndOwner, WM_INTERPRETATION, INTERPRETATION_EVENT_SPEAKER, !isPlayingSound);
		}

		do {
			QueryPerformanceCounter(&ticks);
		} while (ticks.QuadPart - ticksCurrent.QuadPart < ticksPerFrame.QuadPart);

		QueryPerformanceCounter(&ticksCurrent);
	}

	Interpretation(const Interpretation &);
	Interpretation(const Interpretation &&);

public:

	Interpretation(const config::Configuration &settings)
		: cyclesPerFrame(settings.machineCyclesPerStep) {

		UNREFERENCED_PARAMETER(settings);

		display = new platform::NBufferedDisplay<4>();
		keypad = new platform::VKMappedKeypad();
		interpreter = new chip8::Interpreter(display, keypad);
		executionThread = new platform::QueueThread(&Interpretation::threadFunc, this);

		QueryPerformanceFrequency(&ticksPerFrame);
		ticksPerFrame.QuadPart = (ticksPerFrame.QuadPart + 30) / 60;
		ticksCurrent.QuadPart = 0;
	}

	~Interpretation() {
		delete executionThread;
		delete interpreter;
		delete keypad;
		delete display;
	}


	void assignOwnerWindow(HWND hWndOwner) {
		this->hWndOwner = hWndOwner;
	}


	const chip8::IDisplay *getDisplay() {
		return display;
	}

	const byte *consumeDisplayData() {
		return display->consume();
	}


	const chip8::IKeyPad *getKeypad() {
		return keypad;
	}

	void updateKeypadKey(UINT vk, BOOL fDown) {
		keypad->updateKey(vk, fDown);
	}


	void load(LPCTSTR programFile) {
		class LoadTask : public platform::ITask {

			chip8::Interpreter *interpreter;

			std::_tstring programFile;


		public:
			
			LoadTask(chip8::Interpreter *interpreter, LPCTSTR programFile)
				: interpreter(interpreter), programFile(programFile) {

				/* Nothing to do */
			}


			void perform() {
				interpreter->reset(std::ifstream(programFile, std::ios_base::binary));
			}
		};
		executionThread->post(std::shared_ptr<platform::ITask>(new LoadTask(interpreter, programFile)));
		executionThread->resume();
	}

	void reset() {
		class ResetTask : public platform::ITask {

			chip8::Interpreter *interpreter;


		public:

			ResetTask(chip8::Interpreter *interpreter)
				: interpreter(interpreter) {

				/* Nothing to do */
			}


			void perform() {
				interpreter->reset();
			}
		};
		executionThread->post(std::shared_ptr<platform::ITask>(new ResetTask(interpreter)));
		executionThread->resume();
	}



	void pause() {
		executionThread->pause();
	}

	void unpause() {
		executionThread->resume();
	}

};

static std::auto_ptr<Interpretation> interpretation;

static void InitializeInterpretation(const config::Configuration &settings) {
	interpretation.reset(new Interpretation(settings));
}

static void UninitializeInterpretation() {
	interpretation.release();
}


///////////////////////////////////////////////////////////////////////////////////////
// OpenGL Display
///////////////////////////////////////////////////////////////////////////////////////

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
	"uniform vec4 backgroundColor;"
	"uniform vec4 foregroundColor;"
	"uniform sampler2D frame;"
	"in vec2 frameCoord;"
	"\n"
	"void main() {"
	"	vec4 intensityDirect = texture2D(frame, frameCoord).rrra;"
	"	vec4 intensityInverse = vec4(1, 1, 1, 1) - intensityDirect;"
	"\n"
	"	gl_FragColor = foregroundColor * intensityDirect + backgroundColor * intensityInverse;"
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

static void CreateGLContext(Chip8DisplayExtra *extra) {
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
			0,															//Number of bits for the stencilbuffer
			0,															//Number of Aux buffers in the framebuffer.
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
		};
		auto pixelFormat = ChoosePixelFormat(extra->hDC, &pfd);

		SetPixelFormat(extra->hDC, pixelFormat, &pfd);

		hGLRC = wglCreateContext(extra->hDC);
	}
	wglMakeCurrent(extra->hDC, hGLRC);

	PRINT_OPENGL_VERSIOND();
	glewInit();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DITHER);
	glDisable(GL_BLEND);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	extra->hGLRC = hGLRC;
}

static void DestroyGLContext(Chip8DisplayExtra *extra) {
	wglMakeCurrent(extra->hDC, NULL);
	wglDeleteContext(extra->hGLRC);
}


static float cellColor[3];
static float voidColor[3];

static void InitializeGLScene(const config::Configuration &settings) {
	cellColor[0] = settings.displayCellColor[0] / 255.0f;
	cellColor[1] = settings.displayCellColor[1] / 255.0f;
	cellColor[2] = settings.displayCellColor[2] / 255.0f;

	voidColor[0] = settings.displayVoidColor[0] / 255.0f;
	voidColor[1] = settings.displayVoidColor[1] / 255.0f;
	voidColor[2] = settings.displayVoidColor[2] / 255.0f;
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

	const chip8::IDisplay *display = interpretation->getDisplay();

	glGenTextures(1, &idTex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, idTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, display->width(), 
		display->height(), 0, GL_RED, GL_UNSIGNED_BYTE, NULL);

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
		glUseProgram(idShaderPrg);

		GLint uniformBackgroundColor = glGetUniformLocation(idShaderPrg, "backgroundColor");
		GLint uniformForegroundColor = glGetUniformLocation(idShaderPrg, "foregroundColor");
		GLint uniformFrame = glGetUniformLocation(idShaderPrg, "frame");

		GLfloat colors[][4] = {
			{ cellColor[0], cellColor[1], cellColor[2], 1.0	},
			{ voidColor[0], voidColor[1], voidColor[2], 1.0	}
		};
		glUniform4fv(uniformBackgroundColor, 1, colors[1]);
		glUniform4fv(uniformForegroundColor, 1, colors[0]);
		glUniform1i(uniformFrame, 0);
	}
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

static void DrawGLScene(Chip8DisplayExtra *extra) {
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}


///////////////////////////////////////////////////////////////////////////////////////
// DirectSound tone generation and playback
///////////////////////////////////////////////////////////////////////////////////////

static unsigned short sound[] = {
	0x0000, 0x0960, 0x12a1, 0x1c25, 0x2538, 0x2ef8, 0x37b1, 0x4510,
	0x26a9, 0xb409, 0xc2f1, 0xcd5b, 0xd59a, 0xdf64, 0xe884, 0xf1fc,
	0xfb41, 0x04aa, 0x0def, 0x1766, 0x2087, 0x2a4f, 0x3293, 0x3cf2,
	0x4c23, 0xda88, 0xbabb, 0xc837, 0xd0f6, 0xdab1, 0xe3c6, 0xed49,
	0xf68a, 0xffea, 0x094b, 0x128b, 0x1c10, 0x2522, 0x2ee5, 0x3798,
	0x44dc, 0x27d6, 0xb43a, 0xc2d3, 0xcd48, 0xd582, 0xdf50, 0xe86e,
	0xf1e7, 0xfb2b, 0x0494, 0x0dda, 0x174f, 0x2073, 0x2a37, 0x3281,
	0x3cd6, 0x4c4a, 0xdbbe, 0xba86, 0xc81e, 0xd0e4, 0xda9b, 0xe3b1,
	0xed34, 0xf675, 0xffd5, 0x0936, 0x1275, 0x1bfb, 0x250b, 0x2ed2,
	0x377f, 0x44a9, 0x28fe, 0xb46f, 0xc2b6, 0xcd36, 0xd56b, 0xdf3c,
	0xe858, 0xf1d2, 0xfb16, 0x047e, 0x0dc5, 0x1739, 0x205f, 0x2a20,
	0x326f, 0x3cb9, 0x4c6d, 0xdcf9, 0xba50, 0xc805, 0xd0d1, 0xda84,
	0xe39c, 0xed1e, 0xf660, 0xffbf, 0x0920, 0x1260, 0x1be6, 0x24f5,
	0x2ebf, 0x3765, 0x4477, 0x2a21, 0xb4a9, 0xc298, 0xcd23, 0xd554,
	0xdf27, 0xe841, 0xf1bd, 0xfb00, 0x0469, 0x0db0, 0x1723, 0x204b,
	0x2a08, 0x325d, 0x3c9d, 0x4c8b, 0xde39, 0xba19, 0xc7ed, 0xd0bf,
	0xda6d, 0xe387, 0xed08, 0xf64a, 0xffaa, 0x090b, 0x124a, 0x1bd1,
	0x24de, 0x2eac, 0x374c, 0x4447, 0x2b3e, 0xb4e8, 0xc279, 0xcd10,
	0xd53d, 0xdf13, 0xe82b, 0xf1a8, 0xfaea, 0x0453, 0x0d9b, 0x170d,
	0x2036, 0x29f0, 0x324c, 0x3c81, 0x4ca6, 0xdf7c, 0xb9e1, 0xc7d5,
	0xd0ad, 0xda56, 0xe372, 0xecf2, 0xf635, 0xff94, 0x08f5, 0x1234,
	0x1bbc, 0x24c8, 0x2e99, 0x3733, 0x4417, 0x2c57, 0xb52b, 0xc25b,
	0xccfd, 0xd526, 0xdefe, 0xe815, 0xf192, 0xfad5, 0x043d, 0x0d86,
	0x16f6, 0x2022, 0x29d9, 0x323a, 0x3c66, 0x4cbc, 0xe0c4, 0xb9a8,
	0xc7bd, 0xd09b, 0xda3f, 0xe35e, 0xecdc, 0xf620, 0xff7e, 0x08e0,
	0x121f, 0x1ba7, 0x24b2, 0x2e85, 0x3719, 0x43e8, 0x2d6b, 0xb574,
	0xc23b, 0xcce9, 0xd50f, 0xdeea, 0xe7ff, 0xf17d, 0xfabf, 0x0428,
	0x0d71, 0x16e0, 0x200e, 0x29c1, 0x3229, 0x3c4b, 0x4ccd, 0xe210,
	0xb96f, 0xc7a5, 0xd089, 0xda28, 0xe349, 0xecc7, 0xf60b, 0xff69,
	0x08cb, 0x1209, 0x1b91, 0x249b, 0x2e72, 0x3700, 0x43ba, 0x2e79,
	0xb5c1, 0xc21c, 0xccd6, 0xd4f8, 0xded5, 0xe7e9, 0xf168, 0xfaaa,
	0x0412, 0x0d5b, 0x16ca, 0x1ffa, 0x29a9, 0x3218, 0x3c30, 0x4cdb,
	0xe361, 0xb936, 0xc78d, 0xd078, 0xda11, 0xe334, 0xecb1, 0xf5f5,
	0xff53, 0x08b5, 0x11f3, 0x1b7c, 0x2485, 0x2e5e, 0x36e6, 0x438d,
	0x2f82, 0xb613, 0xc1fc, 0xccc2, 0xd4e1, 0xdec1, 0xe7d3, 0xf153,
	0xfa94, 0x03fd, 0x0d46, 0x16b4, 0x1fe6, 0x2991, 0x3207, 0x3c15,
	0x4ce5, 0xe4b4, 0xb8fc, 0xc775, 0xd066, 0xd9fa, 0xe31f, 0xec9b,
	0xf5e0, 0xff3d, 0x08a0, 0x11de, 0x1b67, 0x246f, 0x2e4b, 0x36cc,
	0x4361, 0x3086, 0xb66a, 0xc1dc, 0xccae, 0xd4ca, 0xdeac, 0xe7bd,
	0xf13e, 0xfa7e, 0x03e7, 0x0d31, 0x169d, 0x1fd2, 0x297a, 0x31f6,
	0x3bfb, 0x4cea, 0xe60c, 0xb8c1, 0xc75d, 0xd055, 0xd9e3, 0xe30b,
	0xec85, 0xf5cb, 0xff28, 0x088b, 0x11c8, 0x1b52, 0x2459, 0x2e37,
	0x36b2, 0x4337, 0x3185, 0xb6c6, 0xc1bc, 0xcc99, 0xd4b3, 0xde98,
	0xe7a7, 0xf129, 0xfa69, 0x03d1, 0x0d1c, 0x1687, 0x1fbe, 0x2962,
	0x31e6, 0x3be1, 0x4cec, 0xe767, 0xb887, 0xc746, 0xd044, 0xd9cc,
	0xe2f6, 0xec6f, 0xf5b6, 0xff12, 0x0875, 0x11b2, 0x1b3d, 0x2443,
	0x2e23, 0x3698, 0x430d, 0x327e, 0xb727, 0xc19b, 0xcc85, 0xd49d,
	0xde83, 0xe791, 0xf114, 0xfa53, 0x03bc, 0x0d07, 0x1671, 0x1faa,
	0x294a, 0x31d5, 0x3bc7, 0x4cea, 0xe8c6, 0xb84c, 0xc72e, 0xd033,
	0xd9b5, 0xe2e1, 0xec59, 0xf5a0, 0xfefd,
};

static bool hasSpeaker;

static void InitializeSpeaker(const config::Configuration &settings) {
	hasSpeaker = settings.hasSpeaker;
}

typedef struct tagChip8SpeakerExtra {
	LPDIRECTSOUND device;
	LPDIRECTSOUNDBUFFER buffer;
} Chip8SpeakerExtra;

static Chip8SpeakerExtra speaker;

static void CreateDSSpeaker(HWND hWnd) {
	if (hasSpeaker) {
		HRESULT hr = DirectSoundCreate(NULL, &speaker.device, NULL);

		if (SUCCEEDED(hr)) {
			speaker.device->SetCooperativeLevel(hWnd, DSSCL_PRIORITY);

			WAVEFORMATEX wfx;
			{
				// Set up WAV format structure. 

				memset(&wfx, 0, sizeof(WAVEFORMATEX));
				wfx.wFormatTag = WAVE_FORMAT_PCM;
				wfx.nChannels = 1;
				wfx.wBitsPerSample = 16;
				wfx.nSamplesPerSec = 22050;
				wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
				wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
			}
			DSBUFFERDESC dsbdesc;
			{
				// Set up DSBUFFERDESC structure. 

				memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
				dsbdesc.dwSize = sizeof(DSBUFFERDESC);
				dsbdesc.dwBufferBytes = sizeof(sound);
				dsbdesc.lpwfxFormat = &wfx;
			}
			// Create buffer. 

			hr = speaker.device->CreateSoundBuffer(&dsbdesc, &speaker.buffer, NULL);

			if (SUCCEEDED(hr)) {
				LPVOID pdata1, pdata2;
				DWORD  pdataSize1, pdataSize2;

				hr = speaker.buffer->Lock(0, dsbdesc.dwBufferBytes, &pdata1, &pdataSize1, &pdata2, &pdataSize2, DSBLOCK_ENTIREBUFFER);

				if (SUCCEEDED(hr)) {
					memcpy(pdata1, sound, dsbdesc.dwBufferBytes);

					hr = speaker.buffer->Unlock(pdata1, pdataSize1, pdata2, pdataSize2);

					if (SUCCEEDED(hr)) {
						return;
					}
				}
				speaker.buffer->Release();
				speaker.buffer = NULL;
			}
			speaker.device->Release();
			speaker.device = NULL;
		}
	}
}

#define CHIP8_SPEAKER_PLAYING (DSBSTATUS_PLAYING | DSBSTATUS_LOOPING)

static void EnableDSSpeaker() {
	if (!!speaker.buffer) {
		DWORD status;

		HRESULT hr = speaker.buffer->GetStatus(&status);
		if (SUCCEEDED(hr) && (status & CHIP8_SPEAKER_PLAYING) != CHIP8_SPEAKER_PLAYING)
		{
			speaker.buffer->Play(0, 0, DSBPLAY_LOOPING);
		}
	}
}

static void DisableDSSpeaker() {
	if (!!speaker.buffer) {
		DWORD status;

		HRESULT hr = speaker.buffer->GetStatus(&status);
		if (SUCCEEDED(hr) && (status & CHIP8_SPEAKER_PLAYING) == CHIP8_SPEAKER_PLAYING)
		{
			speaker.buffer->Stop();
		}
	}
}

static void ReleaseDSSpeaker() {
	if (!!speaker.buffer) {
		speaker.buffer->Release();
	}
	if (!!speaker.device) {
		speaker.device->Release();
	}
}


///////////////////////////////////////////////////////////////////////////////////////
// Windows application
///////////////////////////////////////////////////////////////////////////////////////

#define ASSIGN_DISPLAY_EXTRA(hwnd, extraPtr)	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)extraPtr)
#define OBTAIN_DISPLAY_EXTRA(hwnd)				((Chip8DisplayExtra*)GetWindowLongPtr(hWnd, GWLP_USERDATA))

static BOOL Chip8DisplayCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct) {
	interpretation->assignOwnerWindow(hWnd);

	auto extra = new Chip8DisplayExtra;
	{
		extra->hDC = GetDC(hWnd);

		CreateGLContext(extra);
		CreateGLScene(extra, 0, 0, 0, 0); // Not sure we need this.
	}
	ASSIGN_DISPLAY_EXTRA(hWnd, extra);
	wglSwapIntervalEXT(1);

	CreateDSSpeaker(hWnd);

	return TRUE;
}

static VOID Chip8DisplayDestroy(HWND hWnd) {
	ReleaseDSSpeaker();

	wglSwapIntervalEXT(0);
	auto extra = OBTAIN_DISPLAY_EXTRA(hWnd);
	{
		DestroyGLScene(extra);
		DestroyGLContext(extra);

		ReleaseDC(hWnd, extra->hDC);
	}
	delete extra;

	interpretation->assignOwnerWindow(NULL);
	PostQuitMessage(0);
}

static VOID Chip8DisplayPaint(HWND hWnd) {
	static PAINTSTRUCT ps;

	BeginPaint(hWnd, &ps);
	{
		auto extra = OBTAIN_DISPLAY_EXTRA(hWnd);

		DrawGLScene(extra);
		SwapBuffers(extra->hDC);
	}
	EndPaint(hWnd, &ps);
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

static VOID Chip8DisplayCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify) {
	if (codeNotify == 0) {
		auto extra = OBTAIN_DISPLAY_EXTRA(hWnd);

		switch (id) {
		case ID_FILE_LOAD:
		{
			OPENFILENAME ofn = { 0 };
			{
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
				ofn.hwndOwner = hWnd;

				ofn.nMaxFile = 32767;
				ofn.lpstrFile = new TCHAR[ofn.nMaxFile + 1];
				ofn.lpstrFile[0] = _T('\0');
			}
			interpretation->pause();

			if (GetOpenFileName(&ofn)) {
				interpretation->load(ofn.lpstrFile);
			}
			delete[] ofn.lpstrFile;

			HMENU hMenu = GetMenu(hWnd);
			{
				EnableMenuItem(hMenu, ID_FILE_RESET, MF_ENABLED);
				EnableMenuItem(hMenu, ID_FILE_PAUSE, MF_ENABLED);
			}
			CheckMenuItem(hMenu, ID_FILE_PAUSE, MF_UNCHECKED);
			break;
		}
		case ID_FILE_RESET:
			interpretation->reset();

			CheckMenuItem(GetMenu(hWnd), ID_FILE_PAUSE, MF_UNCHECKED);
			break;
		case ID_FILE_PAUSE:
		{
			auto hMenu = GetMenu(hWnd);
			{
				MENUITEMINFO mi;
				{
					mi.cbSize = sizeof(MENUITEMINFO);
					mi.fMask = MIIM_STATE;
				}
				assert(GetMenuItemInfo(hMenu, ID_FILE_PAUSE, FALSE, &mi));

				if ((mi.fState & MFS_ENABLED) == MFS_ENABLED) {
					if (mi.fState & MFS_CHECKED) {
						interpretation->unpause();

						CheckMenuItem(hMenu, ID_FILE_PAUSE, MF_UNCHECKED);
					}
					else {
						interpretation->pause();

						CheckMenuItem(hMenu, ID_FILE_PAUSE, MF_CHECKED);
					}
				}
			}
			break;
		}
		case ID_FILE_EXIT:
			DestroyWindow(hWnd);
			break;
		}
	}
}

static VOID Chip8DisplayKeyUpDown(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags) {
	if (vk == VK_PAUSE) {
		if (!fDown) {
			PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(ID_FILE_PAUSE, 0), (LPARAM)hWnd);
		}
	}
	else {
		interpretation->updateKeypadKey(vk, fDown);
	}
}

static VOID Chip8DisplayInterpretation(HWND hWnd, UINT what, LPARAM lParam) {
	switch (what) {
	case INTERPRETATION_EVENT_DISPLAY:
	{
		const chip8::IDisplay *display = interpretation->getDisplay();

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, display->width(), display->height(),
			GL_RED, GL_UNSIGNED_BYTE, interpretation->consumeDisplayData());

		InvalidateRect(hWnd, NULL, FALSE);
		break;
	}
	case INTERPRETATION_EVENT_SPEAKER:
	{
		if (!!lParam) {
			EnableDSSpeaker();
		}
		else {
			DisableDSSpeaker();
		}
		break;
	}
	case INTERPRETATION_EVENT_ERROR:
	{
		MessageBeep(MB_ICONWARNING);

		HMENU hMenu = GetMenu(hWnd);
		{
			EnableMenuItem(hMenu, ID_FILE_RESET, MF_DISABLED);
			EnableMenuItem(hMenu, ID_FILE_PAUSE, MF_DISABLED);
		}
		CheckMenuItem(hMenu, ID_FILE_PAUSE, MF_UNCHECKED);

		LOGGER_PRINT_FORMATTED_TEXTLN("Interpreter failure: %d. Pausing...", lParam);
	}
	}
}

static LRESULT CALLBACK Chip8DisplayWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	switch (Msg) {
		HANDLE_MSG(hWnd, WM_CREATE, Chip8DisplayCreate);
		HANDLE_MSG(hWnd, WM_DESTROY, Chip8DisplayDestroy);
		HANDLE_MSG(hWnd, WM_SIZE, Chip8DisplaySize);
		HANDLE_MSG(hWnd, WM_PAINT, Chip8DisplayPaint);
		HANDLE_MSG(hWnd, WM_COMMAND, Chip8DisplayCommand);
		HANDLE_MSG(hWnd, WM_KEYDOWN, Chip8DisplayKeyUpDown);
		HANDLE_MSG(hWnd, WM_KEYUP, Chip8DisplayKeyUpDown);
		HANDLE_MSG(hWnd, WM_INTERPRETATION, Chip8DisplayInterpretation);
	};
	return DefWindowProc(hWnd, Msg, wParam, lParam);
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


#define CHIP8_DISPLAY_WINDOW		_T("Chip8DisplayWindow")
#define CHIP8_DISPLAY_WINDOW_STYLE	(WS_OVERLAPPEDWINDOW & ~(WS_SIZEBOX | WS_MAXIMIZEBOX))

DWORD CALLBACK ApplicationInitialization(HINSTANCE hInst, int nCmdShow) {
	config::Configuration settings;

	if (!config::read(std::istream(NULL), settings)) {
		settings = config::Configuration::DEFAULT;

		// TODO: write default settings to *.ini file.
	}
	InitializeGLScene(settings);
	InitializeSpeaker(settings);
	InitializeInterpretation(settings);

	InitCommonControls();

	HWND hWnd;
	{
		WNDCLASS wc = {
			CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
			Chip8DisplayWndProc,
			0,
			sizeof(Chip8DisplayExtra*),
			hInst,
			0,
			LoadCursor(NULL, IDC_ARROW),
			0,
			MAKEINTRESOURCE(IDR_MENU1),
			CHIP8_DISPLAY_WINDOW
		};
		RegisterClass(&wc);

		RECT windowRect = {
			0,
			0,
			chip8::DefaultDisplay::FRAME_WIDTH  * settings.displayCellSize,
			chip8::DefaultDisplay::FRAME_HEIGHT * settings.displayCellSize
		};
		AdjustWindowRect(&windowRect, CHIP8_DISPLAY_WINDOW_STYLE, TRUE);
		hWnd = CreateWindow(wc.lpszClassName, _T("CHIP-8"), CHIP8_DISPLAY_WINDOW_STYLE, CW_USEDEFAULT, 0,
			windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, NULL, NULL, hInst, NULL);
	}
	if (hWnd)
	{
		ShowWindow(hWnd, nCmdShow);

		return ERROR_SUCCESS;
	}
	return GetLastError();
}

VOID CALLBACK ApplicationUninitialization() {
	UninitializeInterpretation();
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

		LOGGER_INIT(WindowsLogger);
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

