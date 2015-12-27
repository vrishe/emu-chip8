#pragma once

#ifndef CHIP8_TIMER_
#define CHIP8_TIMER_

#include <memory>

class TimerListener {
protected:
	TimerListener() {
		/* Nothing to do */
	}


public:
	virtual ~TimerListener() { /* Nothing to do */ }

	virtual void onTick(int timerId, size_t tick) = 0;
};

class Timer {
	const int timerId;
	const TimerListener *listener;

	unsigned rate;


	Timer(const Timer &);

public:
	Timer(int timerId, const TimerListener &listener)
		: timerId(timerId), listener(&listener) {
		/* Nothing to do */
	}

	~Timer() { /* Nothing to do */ }


	virtual void start(unsigned rate) = 0;

	virtual void stop() = 0;
};


typedef std::auto_ptr<Timer>(*TimerFactory) (int timerId, const TimerListener &listener);

#endif // CHIP8_TIMER_