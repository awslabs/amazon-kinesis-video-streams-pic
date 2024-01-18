#include "Include_i.h"

VOID customAssert(INT32 condition, const CHAR* fileName, INT32 lineNumber, const CHAR* functionName, const CHAR* message) {
    printf("Using customAssert\n");
    if (!condition) {
        fprintf(stderr, "%s\n  In file: %s\n  Function: %s\n  Line: %d\n", message, fileName, functionName, lineNumber);
        abort();
    }
}
