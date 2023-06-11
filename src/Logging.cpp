#include "Logging.h"
#include <iostream>
#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <iomanip>


std::map <Logging::LoggingLevel, bool > Logging::loggingLevels_
{ { Logging::LoggingLevel::LOG_DEBUG, false },
  { Logging::LoggingLevel::LOG_ERROR, true },
  { Logging::LoggingLevel::LOG_INFO, true },
  { Logging::LoggingLevel::LOG_WARNING, true },
  { Logging::LoggingLevel::LOG_DISABLED, false }
};

Logging::Logging(const std::string & instanceName)
    : instanceName_(instanceName)
{
}


void Logging::logDebug (const char * message, ...) const
{
    if(!isEnabled(LOG_DEBUG)) return;

    va_list args;
    va_start(args, message);
    logFormatted("DEBUG>", message, args);
    va_end(args);
}


void Logging::logInfo (const char * message, ...) const
{
    if(!isEnabled(LOG_INFO)) return;

    va_list args;
    va_start(args, message);
    logFormatted("INFO >", message, args);
    va_end(args);
}


void Logging::logWarning (const char * message, ...) const
{
    if(!isEnabled(LOG_WARNING)) return;

    va_list args;
    va_start(args, message);
    logFormatted("WARN >", message, args);
    va_end(args);
}


void Logging::logError (const char * message, ...) const
{
    if(!isEnabled(LOG_ERROR)) return;

    va_list args;
    va_start(args, message);
    logFormatted("ERROR>", message, args);
    va_end(args);
}


Logging* Logging::getNewLoggingInstance(const std::string subInstanceName) const
{
    return new Logging(instanceName_ + "(" + subInstanceName + ")");
}


void Logging::enableLoggingLevel(const LoggingLevel level)
{
    loggingLevels_[level] = true;
}


void Logging::disableLoggingLevel(const LoggingLevel level)
{
    loggingLevels_[level] = false;
}

void Logging::logFormatted(const std::string & logLevel,
                           const char * message,
                           va_list args) const
{
    static OSAL::Mutex mutex;
    mutex.lock();

    char buffer[1024];
    vsprintf_s(buffer, sizeof(buffer), message, args);
    va_end(args);

    auto currentTime = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(currentTime);
    std::tm tm = *std::localtime(&time_t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

    std::cout << oss.str() << " " << instanceName_ << " " << logLevel << " " << std::string(buffer) << "\n";

    mutex.unlock();
}


inline bool Logging::isEnabled(const LoggingLevel level) const
{
    if (loggingLevels_[LoggingLevel::LOG_DISABLED])
    {
        return false;
    }

    return loggingLevels_[level];
}