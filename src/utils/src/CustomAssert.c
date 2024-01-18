#include "Include_i.h"

VOID customAssert(INT32 condition, const CHAR* fileName, INT32 lineNumber, const CHAR* functionName) {
    printf("Using customAssert\n");
    if (!condition) {
        fprintf(stderr, "Assertion failed in file %s, function %s, line %d \n", fileName, functionName, lineNumber);
        abort();
    }
}
