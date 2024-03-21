#if !defined(_WIN32) && !defined(_WIN64)

/**
 * Kinesis Video Producer Syslog based logger
 */
#define LOG_CLASS "SyslogLogger"
#include "Include_i.h"

PSyslogLogger gSyslogLogger = NULL;

VOID syslogLoggerLogPrintFn(UINT32 level, PCHAR tag, PCHAR fmt, ...)
{
    va_list valist;

    UNUSED_PARAM(tag);

    if (level >= GET_LOGGER_LOG_LEVEL() && gSyslogLogger != NULL) {
        // Translate log level from libkvs to syslog
        switch (level) {
            case LOG_LEVEL_VERBOSE:
                level = LOG_DEBUG;
                break;
            case LOG_LEVEL_DEBUG:
                level = LOG_DEBUG;
                break;
            case LOG_LEVEL_INFO:
                level = LOG_INFO;
                break;
            case LOG_LEVEL_WARN:
                level = LOG_WARNING;
                break;
            case LOG_LEVEL_ERROR:
                level = LOG_ERR;
                break;
            case LOG_LEVEL_FATAL:
                level = LOG_EMERG;
                break;
            default:
                // ignore unrecognized log levels
                return;
        }

        // Write to syslog
        va_start(valist, fmt);
        vsyslog(level, fmt, valist);
        va_end(valist);
    }
}

STATUS createSyslogLogger(UINT32 facility, BOOL printLog, BOOL setGlobalLogFn, logPrintFunc* pSyslogPrintFn)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(gSyslogLogger == NULL, retStatus); // dont allocate again if already allocated

    // allocate the struct
    CHK(NULL != (gSyslogLogger = (PSyslogLogger) MEMALLOC(SIZEOF(SyslogLogger))), retStatus);
    MEMSET(gSyslogLogger, 0x00, SIZEOF(SyslogLogger));
    gSyslogLogger->printLog = printLog;
    gSyslogLogger->syslogLoggerLogPrintFn = syslogLoggerLogPrintFn;

    // Open connection to syslog
    openlog(NULL, printLog ? LOG_PERROR : 0, facility);

    // See if we are required to set the global log function pointer as well
    if (setGlobalLogFn) {
        // Store the original one to be reset later
        gSyslogLogger->storedLoggerLogPrintFn = globalCustomLogPrintFn;
        // Overwrite with the syslog logger
        globalCustomLogPrintFn = syslogLoggerLogPrintFn;
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeSyslogLogger();
        gSyslogLogger = NULL;
    } else if (pSyslogPrintFn != NULL) {
        *pSyslogPrintFn = syslogLoggerLogPrintFn;
    }

    return retStatus;
}

STATUS freeSyslogLogger()
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(gSyslogLogger != NULL, retStatus);

    // Reset the original logger functionality
    if (gSyslogLogger->storedLoggerLogPrintFn != NULL) {
        globalCustomLogPrintFn = gSyslogLogger->storedLoggerLogPrintFn;
    }

    // Close connection to syslog
    closelog();

    MEMFREE(gSyslogLogger);
    gSyslogLogger = NULL;

CleanUp:

    return retStatus;
}

#endif // _POSIX_C_SOURCE >= 200112L
