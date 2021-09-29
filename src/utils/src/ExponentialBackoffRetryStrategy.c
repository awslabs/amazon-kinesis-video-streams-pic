#include "Include_i.h"

STATUS initializeDefaultExponentialBackoffConfig(PExponentialBackoffRetryStrategyConfig pExponentialBackoffRetryStrategyConfig) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pExponentialBackoffRetryStrategyConfig != NULL, STATUS_NULL_ARG);

    pExponentialBackoffRetryStrategyConfig->maxRetryCount = KVS_INFINITE_EXPONENTIAL_RETRIES;
    pExponentialBackoffRetryStrategyConfig->maxRetryWaitTime = HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS;
    pExponentialBackoffRetryStrategyConfig->retryFactorTime = HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS;
    pExponentialBackoffRetryStrategyConfig->minTimeToResetRetryState = HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS;
    pExponentialBackoffRetryStrategyConfig->jitterFactor = DEFAULT_KVS_JITTER_FACTOR_MILLISECONDS;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS resetExponentialBackoffRetryState(PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pExponentialBackoffRetryStrategyState != NULL, STATUS_NULL_ARG);

    pExponentialBackoffRetryStrategyState->currentRetryCount = 0;
    pExponentialBackoffRetryStrategyState->lastRetryWaitTime = 0;
    pExponentialBackoffRetryStrategyState->lastRetrySystemTime = 0;
    pExponentialBackoffRetryStrategyState->status = BACKOFF_NOT_STARTED;
    DLOGD("Resetting Exponential Backoff State");

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS exponentialBackoffRetryStrategyWithDefaultConfigCreate(PRetryStrategy* ppRetryStrategy) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState = NULL;

    CHK(ppRetryStrategy != NULL, STATUS_NULL_ARG);

    pExponentialBackoffRetryStrategyState = (PExponentialBackoffRetryStrategyState) MEMCALLOC(1, SIZEOF(ExponentialBackoffRetryStrategyState));
    CHK(pExponentialBackoffRetryStrategyState != NULL, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(initializeDefaultExponentialBackoffConfig(&(pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig)));
    CHK_STATUS(resetExponentialBackoffRetryState(pExponentialBackoffRetryStrategyState));
    DLOGD("Created exponential backoff retry strategy state with default configuration.");

CleanUp:
    if (STATUS_SUCCEEDED(retStatus)) {
        (*ppRetryStrategy) = (PRetryStrategy)pExponentialBackoffRetryStrategyState;
    } else {
        SAFE_MEMFREE(pExponentialBackoffRetryStrategyState);
    }

    LEAVES();
    return retStatus;
}

STATUS inRange(UINT64 value, UINT64 low, UINT64 high) {
    return value >= low && value <= high ? STATUS_SUCCESS : STATUS_INVALID_ARG;
}

STATUS validateExponentialBackoffConfig(PExponentialBackoffRetryStrategyConfig pExponentialBackoffRetryStrategyConfig) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pExponentialBackoffRetryStrategyConfig != NULL, STATUS_NULL_ARG);

    CHK_STATUS(inRange(pExponentialBackoffRetryStrategyConfig->maxRetryWaitTime,
                       DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS, KVS_BACKEND_STREAMING_IDLE_TIMEOUT_MILLISECONDS));
    CHK_STATUS(inRange(pExponentialBackoffRetryStrategyConfig->retryFactorTime,
                       DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS, LIMIT_KVS_RETRY_TIME_FACTOR_MILLISECONDS));
    CHK_STATUS(inRange(pExponentialBackoffRetryStrategyConfig->minTimeToResetRetryState,
                       DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS, KVS_BACKEND_STREAMING_IDLE_TIMEOUT_MILLISECONDS));
    CHK_STATUS(inRange(pExponentialBackoffRetryStrategyConfig->jitterFactor,
                       DEFAULT_KVS_JITTER_FACTOR_MILLISECONDS, LIMIT_KVS_JITTER_FACTOR_MILLISECONDS));

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS exponentialBackoffRetryStrategyCreate(PRetryStrategyConfig pRetryStrategyConfig, PRetryStrategy* ppRetryStrategy) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState = NULL;
    PExponentialBackoffRetryStrategyConfig pExponentialBackoffConfig = NULL;

    CHK(ppRetryStrategy != NULL, STATUS_NULL_ARG);

    // If no config provided, create retry strategy with default config
    if (pRetryStrategyConfig == NULL) {
        return exponentialBackoffRetryStrategyWithDefaultConfigCreate(ppRetryStrategy);
    }

    pExponentialBackoffConfig = TO_EXPONENTIAL_BACKOFF_CONFIG(pRetryStrategyConfig);
    CHK_STATUS(validateExponentialBackoffConfig(pExponentialBackoffConfig));

    pExponentialBackoffRetryStrategyState = (PExponentialBackoffRetryStrategyState) MEMCALLOC(1, SIZEOF(ExponentialBackoffRetryStrategyState));
    CHK(pExponentialBackoffRetryStrategyState != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig = *pExponentialBackoffConfig;
    CHK_STATUS(resetExponentialBackoffRetryState(pExponentialBackoffRetryStrategyState));
    DLOGD("Created exponential backoff retry strategy state with provided retry configuration.");

CleanUp:
    if (STATUS_SUCCEEDED(retStatus)) {
        (*ppRetryStrategy) = (PRetryStrategy)pExponentialBackoffRetryStrategyState;
    } else {
        SAFE_MEMFREE(pExponentialBackoffRetryStrategyState);
    }

    LEAVES();
    return retStatus;
}

UINT64 getRandomJitter(UINT32 jitterFactor) {
    return (RAND() % jitterFactor) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
}

STATUS calculateWaitTime(PExponentialBackoffRetryStrategyState pRetryState, PExponentialBackoffRetryStrategyConfig pRetryConfig, PUINT64 pWaitTime) {
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

STATUS validateAndUpdateExponentialBackoffStatus(PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    switch (pExponentialBackoffRetryStrategyState->status) {
        case BACKOFF_NOT_STARTED:
            pExponentialBackoffRetryStrategyState->status = BACKOFF_IN_PROGRESS;
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
            DLOGD("Cannot execute exponentialBackoffBlockingWait. Unexpected state [%"PRIu64"]", pExponentialBackoffRetryStrategyState->status);
            CHK(FALSE, STATUS_EXPONENTIAL_BACKOFF_INVALID_STATE);
    }

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS exponentialBackoffRetryStrategyBlockingWait(PRetryStrategy pRetryStrategy) {
    ENTERS();
    PExponentialBackoffRetryStrategyState pRetryState = NULL;
    PExponentialBackoffRetryStrategyConfig pRetryConfig = NULL;
    UINT64 currentSystemTime = 0, currentRetryWaitTime = 0, jitter = 0;
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pRetryStrategy != NULL, STATUS_NULL_ARG);

    pRetryState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy);
    CHK_STATUS(validateAndUpdateExponentialBackoffStatus(pRetryState));

    pRetryConfig = &(pRetryState->exponentialBackoffRetryStrategyConfig);

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
    jitter = getRandomJitter(pRetryState->exponentialBackoffRetryStrategyConfig.jitterFactor);
    currentRetryWaitTime += jitter;
    THREAD_SLEEP(currentRetryWaitTime);

    // Update retry state's count and wait time values
    pRetryState->lastRetryWaitTime = currentRetryWaitTime;
    pRetryState->lastRetrySystemTime = currentSystemTime;
    pRetryState->currentRetryCount++;

    printf("\n Thread Id [%"PRIu64"] "
          "Number of retries done [%"PRIu64"], "
          "Last retry wait time [%"PRIu64"], "
          "Last retry system time [%"PRIu64"]",
          GETTID(), pRetryState->currentRetryCount, pRetryState->lastRetryWaitTime, pRetryState->lastRetrySystemTime);

CleanUp:
    if (retStatus == STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED) {
        DLOGD("Exhausted exponential retries");
        resetExponentialBackoffRetryState(pRetryState);
    }
    LEAVES();
    return retStatus;
}

STATUS exponentialBackoffRetryStrategyFree(PRetryStrategy* ppRetryStrategy) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState;

    if (ppRetryStrategy == NULL) {
        return retStatus;
    }

    pExponentialBackoffRetryStrategyState = TO_EXPONENTIAL_BACKOFF_STATE(*ppRetryStrategy);
    SAFE_MEMFREE(pExponentialBackoffRetryStrategyState);
    *ppRetryStrategy = NULL;

    LEAVES();
    return retStatus;
}
