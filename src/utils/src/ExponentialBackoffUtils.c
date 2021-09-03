#include "Include_i.h"

STATUS initializeDefaultExponentialBackoffConfig(PExponentialBackoffConfig pExponentialBackoffConfig) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pExponentialBackoffConfig != NULL, STATUS_NULL_ARG);

    pExponentialBackoffConfig->maxRetryCount = KVS_INFINITE_EXPONENTIAL_RETRIES;
    pExponentialBackoffConfig->maxRetryWaitTime = HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS;
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
    pExponentialBackoffState->lastRetrySystemTime = 0;
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

    pExponentialBackoffState = (PExponentialBackoffState) MEMCALLOC(1, SIZEOF(ExponentialBackoffState));
    CHK(pExponentialBackoffState != NULL, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(initializeDefaultExponentialBackoffConfig(&(pExponentialBackoffState->exponentialBackoffConfig)));
    CHK_STATUS(resetExponentialBackoffRetryState(pExponentialBackoffState));

CleanUp:
    if (STATUS_SUCCEEDED(retStatus)) {
        (*ppExponentialBackoffState) = pExponentialBackoffState;
    } else {
        SAFE_MEMFREE(pExponentialBackoffState);
    }

    LEAVES();
    return retStatus;
}

STATUS inRange(UINT64 value, UINT64 low, UINT64 high) {
    return value >= low && value <= high ? STATUS_SUCCESS : STATUS_INVALID_ARG;
}

STATUS validateExponentialBackoffConfig(PExponentialBackoffConfig pBackoffConfig) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pBackoffConfig != NULL, STATUS_NULL_ARG);

    CHK_STATUS(inRange(pBackoffConfig->maxRetryWaitTime, DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS, KVS_BACKEND_STREAMING_IDLE_TIMEOUT_MILLISECONDS));
    CHK_STATUS(inRange(pBackoffConfig->retryFactorTime, DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS, LIMIT_KVS_RETRY_TIME_FACTOR_MILLISECONDS));
    CHK_STATUS(inRange(pBackoffConfig->minTimeToResetRetryState, DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS, KVS_BACKEND_STREAMING_IDLE_TIMEOUT_MILLISECONDS));
    CHK_STATUS(inRange(pBackoffConfig->jitterFactor, DEFAULT_KVS_JITTER_FACTOR_MILLISECONDS, LIMIT_KVS_JITTER_FACTOR_MILLISECONDS));

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

    pExponentialBackoffState = (PExponentialBackoffState) MEMCALLOC(1, SIZEOF(ExponentialBackoffState));
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

STATUS calculateWaitTime(PExponentialBackoffState pRetryState, PExponentialBackoffConfig pRetryConfig, PUINT64 pWaitTime) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 power = 0, waitTime = 0;

    CHK(pWaitTime != NULL, STATUS_NULL_ARG);

    CHK_STATUS(computePower(DEFAULT_KVS_EXPONENTIAL_FACTOR, pRetryState->currentRetryCount, &power));
    waitTime = power * pRetryConfig->retryFactorTime;

    *pWaitTime = waitTime;

CleanUp:
    LEAVES();
    return retStatus;
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
    UINT64 currentSystemTime = 0, currentRetryWaitTime = 0, jitter = 0;
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pRetryState != NULL, STATUS_NULL_ARG);

    CHK_STATUS(validateAndUpdateExponentialBackoffStatus(pRetryState));

    pRetryConfig = &(pRetryState->exponentialBackoffConfig);

    // If retries is exhausted, return error to the application
    CHK(pRetryConfig->maxRetryCount == KVS_INFINITE_EXPONENTIAL_RETRIES ||
         pRetryConfig->maxRetryCount > pRetryState->currentRetryCount, STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED);

    // check the last retry time. If the difference between last retry and current time is greater than
    // minTimeToResetRetryState, then reset the retry state
    //
    // Proceed if this is not the first retry
    currentSystemTime = GETTIME();
    if (pRetryState->currentRetryCount != 0 &&
        currentSystemTime > pRetryState->lastRetrySystemTime && // sanity check
        currentSystemTime - pRetryState->lastRetrySystemTime > pRetryConfig->minTimeToResetRetryState) {
        CHK_STATUS(resetExponentialBackoffRetryState(pRetryState));
        pRetryState->status = BACKOFF_IN_PROGRESS;
    }

    // Bound the exponential curve to maxRetryWaitTime. Once we reach
    // maxRetryWaitTime, then we always wait for maxRetryWaitTime time
    // till the state is reset.
    if (pRetryState->lastRetryWaitTime >= pRetryConfig->maxRetryWaitTime) {
        currentRetryWaitTime = pRetryConfig->maxRetryWaitTime;
    } else {
        CHK_STATUS(calculateWaitTime(pRetryState, pRetryConfig, &currentRetryWaitTime));
        if (currentRetryWaitTime > pRetryConfig->maxRetryWaitTime) {
            currentRetryWaitTime = pRetryConfig->maxRetryWaitTime;
        }
    }

    // Note: Do not move the call to getRandomJitter in calculateWaitTime.
    // This is because we need randomization for wait time after we reach maxRetryWaitTime
    jitter = getRandomJitter(pRetryState->exponentialBackoffConfig.jitterFactor);
    currentRetryWaitTime += jitter;
    THREAD_SLEEP(currentRetryWaitTime);

    // Update retry state's count and wait time values
    pRetryState->lastRetryWaitTime = currentRetryWaitTime;
    pRetryState->lastRetrySystemTime = currentSystemTime;
    pRetryState->currentRetryCount++;

    DLOGD("Number of retries done [%"PRIu64"], "
          "Last retry wait time [%"PRIu64"], "
          "Last retry system time [%"PRIu64"]",
          pRetryState->currentRetryCount, pRetryState->lastRetryWaitTime, pRetryState->lastRetrySystemTime);

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
