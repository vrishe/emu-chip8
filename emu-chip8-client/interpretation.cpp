#include "stdafx.h"

#include "interpretation.h"
#include "logger.h"


void InterpretationThread::run() {
	for (;;) {
		std::shared_ptr<ITask> task;

		tasksMutex.acquire();
		{
			if (!tasks.empty()) {
				tasks.front().swap(task);
				tasks.pop();
			}
		}
		tasksMutex.release();

		if (!!task) {
			task->perform();
			task.reset();
		}
		if (!waitFrameUpdate) {
			if (interpreter->isFrameUpdated()) {
				waitFrameUpdate = true;

				PostMessage(hwndOwner, WM_USER_INTERPRETATION, 0, (LPARAM)this);
				LOGGER_PRINTL_FORMATTED_TEXTLN("Frame update posted @ %d", snapshot.getCountCycles());
			}
			else if (interpreter->isOk()) {
				interpreter->doCycle(keyMapper->mapKey());
			}
		}
	}
}


class stop_exception : public std::exception {
	// Exception primitive that signalize thread to quit.
};

void InterpretationThread::start(LPCTSTR filePath) {
	class StartTask : public ITask {
	public:
		InterpretationThread * const thiz;
		const std::_tstring filePath;

		StartTask(InterpretationThread *thiz, const std::_tstring &filePath)
			: thiz(thiz), filePath(filePath) {
			/* Nothing to do */
		}

		virtual void perform() {
			thiz->interpreter->reset(std::ifstream(filePath, std::ios_base::binary));
			PostMessage(thiz->hwndOwner, WM_USER_INTERPRETATION, 0, (LPARAM)thiz);
		}
	};
	class Runner {
	public:
		static void run(InterpretationThread *target) {
			try {
				target->run();
			}
			catch (const stop_exception &) {
				/* Nothing to do */
			}
		}
	};
	stop();
	runner = std::thread(Runner::run, this);

	tasksMutex.acquire();
	{
		tasks.push(std::shared_ptr<ITask>(new StartTask(this, filePath)));
	}
	tasksMutex.release();
}

void InterpretationThread::reset() {
	class ResetTask : public ITask {
	public:
		InterpretationThread * const thiz;

		ResetTask(InterpretationThread *thiz)
			: thiz(thiz) {
			/* Nothing to do */
		}

		virtual void perform() {
			thiz->interpreter->reset();
			PostMessage(thiz->hwndOwner, WM_USER_INTERPRETATION, 0, (LPARAM)thiz);
		}
	};
	if (runner.joinable()) {
		tasksMutex.acquire();
		{
			tasks.push(std::shared_ptr<ITask>(new ResetTask(this)));
		}
		tasksMutex.release();
	}
}

void InterpretationThread::stop() {
	class StopTask : public ITask {
	public:
		virtual void perform() {
			throw stop_exception();
		}
	};
	if (runner.joinable()) {
		tasksMutex.acquire();
		{
			tasks.push(std::shared_ptr<ITask>(new StopTask()));
		}
		tasksMutex.release();

		runner.join();
	}
}


const chip8::Interpreter::Frame &InterpretationThread::beginFrameUpdate() {
	return interpreter->getFrame();
}

void InterpretationThread::endFrameUpdate() {
	waitFrameUpdate = false;
}