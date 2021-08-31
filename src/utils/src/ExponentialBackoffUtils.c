#include "Include_i.h"

STATUS initializeDefaultExponentialBackOffConfig(PExponentialBackoffConfig pExponentialBackoffConfig) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pExponentialBackoffConfig != NULL, STATUS_NULL_ARG);

    pExponentialBackoffConfig->maxRetryCount = KVS_INFINITE_EXPONENTIAL_RETRIES;
    pExponentialBackoffConfig->maxWaitTime = HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS;
    pExponentialBackoffConfig->retryFactorTime = HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS;
    pExponentialBackoffConfig->minTimeToResetRetryState = HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS;
    pExponentialBackoffConfig->jitterFactor = DEFAULT_KVS_JITTER_FACTOR_MILLISECONDS;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS resetExponentialBackoffRetryState(PExponentialBackoffState pExponentialBackoffState) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pExponentialBackoffState != NULL, STATUS_NULL_ARG);

    pExponentialBackoffState->currentRetryCount = 0;
    pExponentialBackoffState->lastRetryWaitTime = 0;
    pExponentialBackoffState->status = BACKOFF_NOT_STARTED;
    DLOGD("Resetting Exponential Backoff State");

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS exponentialBackoffStateWithDefaultConfigCreate(PExponentialBackoffState* ppExponentialBackoffState) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PExponentialBackoffState pExponentialBackoffState = NULL;
    PExponentialBackoffConfig pExponentialBackoffConfig = NULL;

    CHK(ppExponentialBackoffState != NULL, STATUS_NULL_ARG);

    pExponentialBackoffState = (PExponentialBackoffState) MEMCALLOC(0x00, SIZEOF(ExponentialBackoffState));
    CHK(pExponentialBackoffState != NULL, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(initializeDefaultExponentialBackOffConfig(&(pExponentialBackoffState->exponentialBackoffConfig)));
    CHK_STATUS(resetExponentialBackoffRetryState(pExponentialBackoffState));

CleanUp:
    if (STATUS_SUCCEEDED(retStatus)) {
        (*ppExponentialBackoffState) = pExponentialBackoffState;
    } else {
        if (pExponentialBackoffState != NULL) {
            SAFE_MEMFREE(pExponentialBackoffState);
        }
    }

    LEAVES();
    return retStatus;
}

STATUS validateExponentialBackoffConfig(PExponentialBackoffConfig pBackoffConfig) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pBackoffConfig != NULL, STATUS_NULL_ARG);

    // The upper bound on wait time should be at least DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS
    CHK(pBackoffConfig->maxWaitTime >= DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS, STATUS_INVALID_ARG);
    // The retry factor time should be at least DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS
    CHK(pBackoffConfig->retryFactorTime >= DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS, STATUS_INVALID_ARG);
    // Minimum time between two consecutive calls for exponential wait before the state is
    // reset to start from count 0 must be at least DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS
    CHK(pBackoffConfig->minTimeToResetRetryState >= DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS, STATUS_INVALID_ARG);
    // Jitter factor must be at least DEFAULT_KVS_JITTER_FACTOR_MILLISECONDS
    CHK(pBackoffConfig->jitterFactor >= DEFAULT_KVS_JITTER_FACTOR_MILLISECONDS, STATUS_INVALID_ARG);

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS exponentialBackoffStateCreate(PExponentialBackoffState* ppBackoffState, PExponentialBackoffConfig pBackoffConfig) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PExponentialBackoffState pExponentialBackoffState = NULL;

    CHK(ppBackoffState != NULL && pBackoffConfig != NULL, STATUS_NULL_ARG);
    CHK_STATUS(validateExponentialBackoffConfig(pBackoffConfig));

    pExponentialBackoffState = (PExponentialBackoffState) MEMCALLOC(0x00, SIZEOF(ExponentialBackoffState));
    CHK(pExponentialBackoffState != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pExponentialBackoffState->exponentialBackoffConfig = *pBackoffConfig;
    CHK_STATUS(resetExponentialBackoffRetryState(pExponentialBackoffState));

CleanUp:
    if (STATUS_SUCCEEDED(retStatus)) {
        (*ppBackoffState) = pExponentialBackoffState;
    } else {
        SAFE_MEMFREE(pExponentialBackoffState);
    }

    LEAVES();
    return retStatus;
}

UINT64 getRandomJitter(UINT32 jitterFactor) {
    return (RAND() % jitterFactor) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
}

UINT64 calculateWaitTime(PExponentialBackoffState pRetryState, PExponentialBackoffConfig pRetryConfig) {
    // DEFAULT_KVS_EXPONENTIAL_FACTOR is 2 and if currentRetryCount > 64, then
    // power(2, currentRetryCount) will overflow UINT64 buffer. In such case we
    // always return the MAX_UINT64 value.
    // This case will occur only if the exponential retries are configured with no
    // upper bound on wait time. The implicit upper bound in that case will be MAX_UINT64.
    if (pRetryState->currentRetryCount > 64) {
        return MAX_UINT64;
    }
    return power(DEFAULT_KVS_EXPONENTIAL_FACTOR, pRetryState->currentRetryCount) * pRetryConfig->retryFactorTime;
}

STATUS validateAndUpdateExponentialBackoffStatus(PExponentialBackoffState pExponentialBackoffState) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    switch (pExponentialBackoffState->status) {
        case BACKOFF_NOT_STARTED:
            pExponentialBackoffState->status = BACKOFF_IN_PROGRESS;
            DLOGD("Status changed from BACKOFF_NOT_STARTED to BACKOFF_IN_PROGRESS");
            break;
        case BACKOFF_IN_PROGRESS:
            // Do nothing
            DLOGD("Current status is BACKOFF_IN_PROGRESS");
            break;
        case BACKOFF_TERMINATED:
            DLOGD("Cannot execute exponentialBackoffBlockingWait. Current status is BACKOFF_TERMINATED");
            CHK(FALSE, STATUS_EXPONENTIAL_BACKOFF_INVALID_STATE);
            // No 'break' needed since CHK(FALSE, ...) will always jump to CleanUp
        default:
            DLOGD("Cannot execute exponentialBackoffBlockingWait. Unexpected state [%"PRIu64"]", pExponentialBackoffState->status);
            CHK(FALSE, STATUS_EXPONENTIAL_BACKOFF_INVALID_STATE);
    }

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS exponentialBackoffBlockingWait(PExponentialBackoffState pRetryState) {
    ENTERS();
    PExponentialBackoffConfig pRetryConfig = NULL;
    UINT64 currentTime = 0, waitTime = 0, jitter = 0;
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pRetryState != NULL, STATUS_NULL_ARG);

    CHK_STATUS(validateAndUpdateExponentialBackoffStatus(pRetryState));

    pRetryConfig = &(pRetryState->exponentialBackoffConfig);

    // If retries is exhausted, return error to the application
    if (pRetryConfig->maxRetryCount != KVS_INFINITE_EXPONENTIAL_RETRIES) {
        CHK(!(pRetryConfig->maxRetryCount == pRetryState->currentRetryCount),
            STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED);
    }

    // check the last retry time. If the difference between last retry and current time is greater than
    // minTimeToResetRetryState, then reset the retry state
    //
    // Proceed if this is not the first retry
    currentTime = GETTIME();
    if (pRetryState->currentRetryCount != 0) {
        if (currentTime - pRetryState->lastRetryWaitTime > pRetryConfig->minTimeToResetRetryState) {
            CHK_STATUS(resetExponentialBackoffRetryState(pRetryState));
            pRetryState->status = BACKOFF_IN_PROGRESS;
        }
    }

    // Bound the exponential curve to maxWaitTime. Once we reach
    // maxWaitTime, then we always wait for maxWaitTime time
    // till the state is reset.
    if (pRetryState->lastRetryWaitTime == pRetryConfig->maxWaitTime) {
        waitTime = pRetryState->lastRetryWaitTime;
    } else {
        waitTime = calculateWaitTime(pRetryState, pRetryConfig);
        if (waitTime > pRetryConfig->maxWaitTime) {
            waitTime = pRetryConfig->maxWaitTime;
        }
    }

    jitter = getRandomJitter(pRetryConfig->jitterFactor);
    THREAD_SLEEP(waitTime + jitter);

    pRetryState->lastRetryWaitTime = currentTime + waitTime + jitter;
    pRetryState->currentRetryCount++;
    DLOGD("Number of retries done [%"PRIu64"]. Last retry wait time [%"PRIu64"]", pRetryState->currentRetryCount, pRetryState->lastRetryWaitTime);

CleanUp:
    if (retStatus == STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED) {
        DLOGD("Exhausted exponential retries");
        resetExponentialBackoffRetryState(pRetryState);
    }
    LEAVES();
    return retStatus;
}

STATUS exponentialBackoffStateFree(PExponentialBackoffState* ppExponentialBackoffState) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    if (ppExponentialBackoffState == NULL) {
        return retStatus;
    }

    SAFE_MEMFREE(*ppExponentialBackoffState);

    LEAVES();
    return retStatus;
}
