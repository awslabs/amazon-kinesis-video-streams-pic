#include "Include_i.h"

/**
 * Calculates power using Divide and Conquer style - O(log exponent) complexity
 *
 * To prevent overflows -
 * For smaller base values (<=5), the exponent limit is upto 27
 * For larger base values (<=100), the exponent limit is upto 9
 * For ultra large base values (<=1000), the exponent limit is upto 6
 */
STATUS computePower(UINT64 base, UINT64 exponent, PUINT64 result) {
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 _result = 1;

    CHK(result != NULL, STATUS_NULL_ARG);

    // base and exponent values checks to prevent overflow
    CHK((base <= 5 && exponent <= 27) ||
            (base <= 100 && exponent <= 9) ||
            (base <= 1000 && exponent <= 6), STATUS_INVALID_ARG);

    if (base == 0) {
        return 0;
    }

    while (exponent != 0) {
        // if exponent is odd, multiply the result by base
        if (exponent & 1) {
            _result *= base;
        }
        // divide exponent by 2
        exponent = exponent >> 1;
        // multiply base by itself
        base = base * base;
    }

    *result = _result;

CleanUp:
    return retStatus;
}
