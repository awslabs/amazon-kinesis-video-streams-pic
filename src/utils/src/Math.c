#include "Include_i.h"

/**
 * Calculates power using Divide and Conquer style - O(log exponent) complexity
 *
 * WARNING:
 * The result could overflow if the base ^ power exceeds UINT64 limit.
 * It is the callers responsibility to sanitize the values before calling this API
 */
UINT64 power(UINT32 base, UINT32 exponent) {
    UINT64 result = 1;

    if (base == 0) {
        return 0;
    }

    // loop till `exponent becomes 0
    while (exponent != 0) {
        // if exponent is odd, multiply the result by base
        if (exponent & 1) {
            result *= base;
        }
        // divide exponent by 2
        exponent = exponent >> 1;
        // multiply base by itself
        base = base * base;
    }

    return result;
}
