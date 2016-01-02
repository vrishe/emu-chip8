#pragma once

#include "chip8\Chip8Interpreter.h"

class IKeyMapper {
public:
	virtual chip8::Interpreter::Keyboard mapKey() = 0;
};

#define WM_USER_INTERPRETATION	(WM_USER + 1024)
#define HANDLE_WM_USER_INTERPRETATION(hWnd, wParam, lParam, fn) \
	((fn)(hWnd, (InterpretationThread*)lParam), 0L)

class Mutex {
	CRITICAL_SECTION criticalSection;

public:
	Mutex() {
		InitializeCriticalSection(&criticalSection);
	}
	~Mutex() {
		DeleteCriticalSection(&criticalSection);
	}

	void acquire() {
		EnterCriticalSection(&criticalSection);
	}
	void release() {
		LeaveCriticalSection(&criticalSection);
	}
};

class InterpretationThread {

	const HWND	hwndOwner;

	chip8::Interpreter *	const interpreter;
	IKeyMapper *			const keyMapper;

	std::atomic_bool waitFrameUpdate;


	class ITask {
	public:
		virtual void perform() = 0;
	};

	Mutex tasksMutex;

	std::queue<std::shared_ptr<ITask>> tasks;
	std::thread runner;

	void run();


	InterpretationThread(const InterpretationThread &);

public:
	InterpretationThread(HWND hwndOwner, chip8::Interpreter *interpreter, IKeyMapper *keyMapper)
		: hwndOwner(hwndOwner), interpreter(interpreter), keyMapper(keyMapper) {

		interpreter->reset();
	}

	~InterpretationThread() {
		stop();
	}


	void start(LPCTSTR filePath);
	void reset();
	void stop();

	const chip8::Interpreter::Frame &beginFrameUpdate();
	void endFrameUpdate();
};