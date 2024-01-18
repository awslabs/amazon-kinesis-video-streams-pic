#include "Include_i.h"

VOID customAssert(INT32 condition, const CHAR* fileName, INT32 lineNumber, const CHAR* functionName, const CHAR* format, ...) {
    printf("Using customAssert\n");
    if (!condition) {
        fprintf(stderr, "Assertion failed in %s (%s:%d)\nFunction: %s\nMessage: ", fileName, functionName, lineNumber);
        abort();
    }
}
