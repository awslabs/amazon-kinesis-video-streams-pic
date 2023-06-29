#include "Include_i.h"

getTime globalGetTime = defaultGetTime;
getTime globalGetRealTime = defaultGetTime;

STATUS generateTimestampStrInMilliseconds(PCHAR formatStr, PCHAR pDestBuffer, UINT32 destBufferLen, PUINT32 pFormattedStrLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 formattedStrLen;
    UINT32 millisecondVal;
    struct tm* timeinfo;

#if defined _WIN32 || defined _WIN64
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);
    ULARGE_INTEGER uli;
    struct tm timeinfoWindows;

    uli.LowPart = fileTime.dwLowDateTime;
    uli.HighPart = fileTime.dwHighDateTime;


    ULONGLONG windowsTime = uli.QuadPart - TIME_DIFF_UNIX_WINDOWS_TIME;
    time_t unixTime = (time_t)(windowsTime / 10000000);  // Conversion to seconds
    gmtime_s(&timeinfo, &unixTime);

    millisecondVal = ((windowsTime / 10000) % 1000);  // Extract milliseconds


#elif defined __MACH__ || defined __CYGWIN__
    struct timeval tv;
    gettimeofday(&tv, NULL);  // Get current time

    timeinfo = GMTIME(&(tv.tv_sec));  // Convert to broken-down time

    millisecondVal = tv.tv_usec / HUNDREDS_OF_NANOS_IN_A_MICROSECOND;  // Convert microseconds to milliseconds

#else
    struct timespec nowTime;
    clock_gettime(CLOCK_REALTIME, &nowTime);

    timeinfo = GMTIME(&(nowTime.tv_sec));
    millisecondVal = nowTime.tv_nsec / DEFAULT_TIME_UNIT_IN_NANOS;  // Convert nanoseconds to milliseconds

#endif
    CHK(pDestBuffer != NULL, STATUS_NULL_ARG);
    CHK(STRNLEN(formatStr, MAX_TIMESTAMP_FORMAT_STR_LEN + 1) <= MAX_TIMESTAMP_FORMAT_STR_LEN, STATUS_MAX_TIMESTAMP_FORMAT_STR_LEN_EXCEEDED);

    formattedStrLen = 0;
    *pFormattedStrLen = 0;

    formattedStrLen = (UINT32) STRFTIME(pDestBuffer, destBufferLen - MAX_MILLISECOND_PORTION_LENGTH, formatStr, timeinfo);
    CHK(formattedStrLen != 0, STATUS_STRFTIME_FALIED);
    SNPRINTF(pDestBuffer + formattedStrLen, MAX_MILLISECOND_PORTION_LENGTH, ".%06d ", millisecondVal);
    formattedStrLen = (UINT32) STRLEN(pDestBuffer);
    pDestBuffer[formattedStrLen] = '\0';
    *pFormattedStrLen = formattedStrLen;

CleanUp:

    return retStatus;
}

STATUS generateTimestampStr(UINT64 timestamp, PCHAR formatStr, PCHAR pDestBuffer, UINT32 destBufferLen, PUINT32 pFormattedStrLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    time_t timestampSeconds;
    UINT32 formattedStrLen;
    CHK(pDestBuffer != NULL, STATUS_NULL_ARG);
    CHK(STRNLEN(formatStr, MAX_TIMESTAMP_FORMAT_STR_LEN + 1) <= MAX_TIMESTAMP_FORMAT_STR_LEN, STATUS_MAX_TIMESTAMP_FORMAT_STR_LEN_EXCEEDED);

    timestampSeconds = timestamp / HUNDREDS_OF_NANOS_IN_A_SECOND;
    formattedStrLen = 0;
    *pFormattedStrLen = 0;

    formattedStrLen = (UINT32) STRFTIME(pDestBuffer, destBufferLen, formatStr, GMTIME(&timestampSeconds));
    CHK(formattedStrLen != 0, STATUS_STRFTIME_FALIED);

    pDestBuffer[formattedStrLen] = '\0';
    *pFormattedStrLen = formattedStrLen;

CleanUp:

    return retStatus;
}

UINT64 defaultGetTime()
{
#if defined _WIN32 || defined _WIN64
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);

    return ((((UINT64) fileTime.dwHighDateTime << 32) | fileTime.dwLowDateTime) - TIME_DIFF_UNIX_WINDOWS_TIME);
#elif defined __MACH__ || defined __CYGWIN__
    struct timeval nowTime;
    if (0 != gettimeofday(&nowTime, NULL)) {
        return 0;
    }

    return (UINT64) nowTime.tv_sec * HUNDREDS_OF_NANOS_IN_A_SECOND + (UINT64) nowTime.tv_usec * HUNDREDS_OF_NANOS_IN_A_MICROSECOND;
#else
    struct timespec nowTime;
    clock_gettime(CLOCK_REALTIME, &nowTime);

    // The precision needs to be on a 100th nanosecond resolution
    return (UINT64) nowTime.tv_sec * HUNDREDS_OF_NANOS_IN_A_SECOND + (UINT64) nowTime.tv_nsec / DEFAULT_TIME_UNIT_IN_NANOS;
#endif
}
