#pragma once

#ifndef LOGGER_H_
#define LOGGER_H_

#include <string>


#ifdef _DEBUG
#define LOGGER_INIT(Logger)							logger::init(logger::LoggerInitializer<Logger>())
#define LOGGER_PRINT_EMPTYLN()						logger::printLn()
#define LOGGER_PRINTL_TEXTLN(text)					logger::printLn(text) 
#define LOGGER_PRINTL_FORMATTED_TEXTLN(fmt, arg)	logger::printLn(text, arg) 
#else
#define LOGGER_INIT(Logger)
#define LOGGER_PRINT_EMPTYLN()
#define LOGGER_PRINTL_TEXTLN(text)
#define LOGGER_PRINTL_FORMATTED_TEXTLN(fmt, arg)
#endif // _DEBUG


namespace logger {

	class ILogger {
	protected:
		ILogger() { /* Nothing to do */ }

	public:
		virtual ~ILogger() { /* Nothing to do */ }

		virtual void printLn() = 0;
		virtual void printLn(const std::string &) = 0;
		virtual void printLn(const std::wstring &) = 0;
		virtual void printLn(const std::string &, va_list) = 0;
		virtual void printLn(const std::wstring &, va_list) = 0;
	};

	class LoggerInitializerBase {
	public:
		virtual ~LoggerInitializerBase() { /* Nothing to do */ }

		virtual ILogger *newInstance() const = 0;
	};

	namespace {
		template<class TLogger, bool = std::is_base_of<ILogger, TLogger>::value>
		class LoggerInitializer;

		template<class TLogger>
		class LoggerInitializer < TLogger, true > : public LoggerInitializerBase{
		public:
			ILogger *newInstance() const {
				return new TLogger;
			}
		};
	}

	void init(const LoggerInitializerBase &);

	void printLn();
	//void printLn(const std::string &);
	//void printLn(const std::wstring &);
	void printLn(const std::string &format, ...);
	void printLn(const std::wstring &format, ...);
}

#endif // LOGGER_H_