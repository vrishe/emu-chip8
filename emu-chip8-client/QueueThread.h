#pragma once

namespace platform {

	class ITask {
	public:
		virtual ~ITask() {
			/* Nothing to do */
		}


		virtual void perform() = 0;
	};

	class QueueThread {

		typedef void *pmutex, *pevent;

		pmutex mutex;
		pevent event;
		
		std::thread thread;
		std::queue<std::shared_ptr<ITask>> backlog;


		std::atomic_bool alive;

		void initialize(std::function<void()> && f);

		QueueThread(const QueueThread &);
		QueueThread(const QueueThread &&);


	protected:

		virtual void run(std::function<void()> &f);


	public:

		QueueThread() {
			initialize([]() { /* Nothing to do */ });
		}

		template<typename Fn, typename... Args>
		explicit QueueThread(Fn &&f, Args&&... args) {
			initialize(std::bind(std::_Decay_copy(std::forward<Fn>(f)),
				std::_Decay_copy(std::forward<Args>(args))...));
		}

		virtual ~QueueThread();

		void post(const std::shared_ptr<ITask> &task);

		void pause();
		void resume();
	};

} // namespace platform