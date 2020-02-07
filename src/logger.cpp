#include "logger.h"

#include <cstdarg>

namespace logger {

	namespace {
		std::auto_ptr<ILogger> logger;
	}
	
	void init(const LoggerInitializerBase &initializer) {
		logger.reset(initializer.newInstance());
	}


	void printLn() {
		auto loggerLocal = logger.get();

		if (!!loggerLocal) {
			loggerLocal->printLn();
		}
	}
	//void printLn(const std::string &text) {
	//	auto loggerLocal = logger.get();

	//	if (!!loggerLocal) {
	//		loggerLocal->printLn(text);
	//	}
	//}
	//void printLn(const std::wstring &text) {
	//	auto loggerLocal = logger.get();

	//	if (!!loggerLocal) {
	//		loggerLocal->printLn(text);
	//	}
	//}
	void printLn(const std::string &text, int unused, ...) {
		auto loggerLocal = logger.get();

		if (!!loggerLocal) {
			va_list args;
			va_start(args, unused);
			loggerLocal->printLn(text, args);
			va_end(args);
		}
	}
	void printLn(const std::wstring &text, int unused, ...) {
		auto loggerLocal = logger.get();

		if (!!loggerLocal) {
			va_list args;
			va_start(args, unused);
			loggerLocal->printLn(text, args);
			va_end(args);
		}
	}
}