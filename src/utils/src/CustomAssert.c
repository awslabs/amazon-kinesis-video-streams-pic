#include "Include_i.h"

VOID customAssert(INT32 condition, const CHAR* fileName, INT32 lineNumber, const CHAR* functionName, const CHAR* format, ...) {
    printf("Using customAssert\n");
    if (!condition) {
        fprintf(stderr, "Assertion failed in file %s, function %s, line %d. Message: ", fileName, functionName, lineNumber);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);

        fprintf(stderr, "\n");
        abort();
    }
}
