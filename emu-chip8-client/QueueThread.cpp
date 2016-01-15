#include "stdafx.h"

#include "QueueThread.h"


namespace platform {

#define AS_CRITICAL_SECTION(m) \
	reinterpret_cast<LPCRITICAL_SECTION>(m);

	void QueueThread::initialize(std::function<void()> && f) {
		auto lpsection = new CRITICAL_SECTION();
		{
			InitializeCriticalSection(lpsection);
		}
		mutex = lpsection;
		alive = true;

		thread = std::move(std::thread([this](std::function<void()> &func) { this->run(func); }, f));
	}

	QueueThread::~QueueThread() {
		alive = false;
		if (thread.joinable()) {
			thread.join();
		}
		delete AS_CRITICAL_SECTION(mutex);
	}


	void QueueThread::run(std::function<void()> &f) {
		while (alive) {
			std::shared_ptr<ITask> task;
			{
				auto lpsection = AS_CRITICAL_SECTION(mutex);

				EnterCriticalSection(lpsection);
				{
					if (!backlog.empty()) {
						task = std::move(backlog.front());

						backlog.pop();
					}
				}
				LeaveCriticalSection(lpsection);
			}
			if (task) {
				task->perform();

				task.reset();
			}
			f();
		}
	}


	void QueueThread::post(const std::shared_ptr<ITask> &task) {
		auto lpsection = AS_CRITICAL_SECTION(mutex);

		EnterCriticalSection(lpsection);
		{
			backlog.push(task);
		}
		LeaveCriticalSection(lpsection);
	}

} // namespace platform