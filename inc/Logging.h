#ifndef _LOGGING_H_
#define _LOGGING_H_

#include "OSAL.h"


class Logging
{
public:

    enum LoggingLevel
    {
        LOG_DISABLED,
        LOG_ERROR,
        LOG_WARNING,
        LOG_INFO,
        LOG_DEBUG,
    };

public:

    explicit Logging (const std::string & instanceName);
    virtual ~Logging () = default;

    void logDebug   (const char * message, ...) const;
    void logInfo    (const char * message, ...) const;
    void logWarning (const char * message, ...) const;
    void logError   (const char * message, ...) const;

    Logging* getNewLoggingInstance(const std::string subInstanceName) const;

    static void enableLoggingLevel(const LoggingLevel level);
    static void disableLoggingLevel(const LoggingLevel level);

private:

    void logFormatted(const std::string & logLevel,
                      const char * message,
                      va_list args) const;
    inline bool isEnabled(const LoggingLevel level) const;

private:

    std::string instanceName_;

    static std::map<LoggingLevel, bool> loggingLevels_;
};



#endif // _LOGGING_H_