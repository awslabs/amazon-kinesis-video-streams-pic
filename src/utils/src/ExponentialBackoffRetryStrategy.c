#include "Include_i.h"

STATUS normalizeExponentialBackoffConfig(PExponentialBackoffRetryStrategyConfig pExponentialBackoffRetryStrategyConfig) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pExponentialBackoffRetryStrategyConfig != NULL, STATUS_NULL_ARG);
    pExponentialBackoffRetryStrategyConfig->maxRetryWaitTime *= HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    pExponentialBackoffRetryStrategyConfig->retryFactorTime *= HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    pExponentialBackoffRetryStrategyConfig->minTimeToResetRetryState *= HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    DLOGD("Thread Id [%"PRIu64"]. Exponential backoff retry strategy config - "
            "maxRetryCount: [%"PRIu64"], "
            "maxRetryWaitTime: [%"PRIu64"], "
            "retryFactorTime: [%"PRIu64"], "
            "minTimeToResetRetryState: [%"PRIu64"], "
            "jitterType: [%"PRIu64"], "
            "jitterFactor: [%"PRIu64"], ",
            GETTID(),
            pExponentialBackoffRetryStrategyConfig->maxRetryCount,
            pExponentialBackoffRetryStrategyConfig->maxRetryWaitTime,
            pExponentialBackoffRetryStrategyConfig->retryFactorTime,
            pExponentialBackoffRetryStrategyConfig->minTimeToResetRetryState,
            pExponentialBackoffRetryStrategyConfig->jitterType,
            pExponentialBackoffRetryStrategyConfig->jitterFactor);

    // Seed rand to generate random number for jitter
    SRAND(GETTIME());
CleanUp:
    LEAVES();
    return retStatus;
}

STATUS resetExponentialBackoffRetryState(PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pExponentialBackoffRetryStrategyState != NULL, STATUS_NULL_ARG);
    DLOGD("Thread Id [%"PRIu64"]. Resetting Exponential Backoff State. Last retry system time [%"PRIu64"]. Current system time [%"PRIu64"]",
          GETTID(), pExponentialBackoffRetryStrategyState->lastRetrySystemTime, GETTIME());
    pExponentialBackoffRetryStrategyState->currentRetryCount = 0;
    pExponentialBackoffRetryStrategyState->lastRetryWaitTime = 0;
    pExponentialBackoffRetryStrategyState->lastRetrySystemTime = 0;
    pExponentialBackoffRetryStrategyState->status = BACKOFF_NOT_STARTED;

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
    pExponentialBackoffRetryStrategyState->retryStrategyLock = MUTEX_CREATE(FALSE);

    pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig = DEFAULT_EXPONENTIAL_BACKOFF_CONFIGURATION;

    // Normalize the time parameters in config to be in HUNDREDS_OF_NANOS_IN_A_MILLISECOND
    CHK_STATUS(normalizeExponentialBackoffConfig(&pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig));

    CHK_STATUS(resetExponentialBackoffRetryState(pExponentialBackoffRetryStrategyState));
    DLOGD("Created exponential backoff retry strategy state with default configuration.");

CleanUp:
    if (STATUS_SUCCEEDED(retStatus)) {
        (*ppRetryStrategy) = (PRetryStrategy)pExponentialBackoffRetryStrategyState;
    }

    LEAVES();
    return retStatus;
}

STATUS validateExponentialBackoffConfig(PExponentialBackoffRetryStrategyConfig pExponentialBackoffRetryStrategyConfig) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pExponentialBackoffRetryStrategyConfig != NULL, STATUS_NULL_ARG);

    CHK(CHECK_IN_RANGE(pExponentialBackoffRetryStrategyConfig->maxRetryWaitTime,
                       MIN_KVS_MAX_WAIT_TIME_MILLISECONDS, LIMIT_KVS_MAX_WAIT_TIME_MILLISECONDS), STATUS_INVALID_ARG);
    CHK(CHECK_IN_RANGE(pExponentialBackoffRetryStrategyConfig->retryFactorTime,
                       MIN_KVS_RETRY_TIME_FACTOR_MILLISECONDS, LIMIT_KVS_RETRY_TIME_FACTOR_MILLISECONDS), STATUS_INVALID_ARG);
    CHK(CHECK_IN_RANGE(pExponentialBackoffRetryStrategyConfig->minTimeToResetRetryState,
                       MIN_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS, LIMIT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS), STATUS_INVALID_ARG);
    CHK(pExponentialBackoffRetryStrategyConfig->jitterType == FULL_JITTER ||
        pExponentialBackoffRetryStrategyConfig->jitterType == FIXED_JITTER ||
        pExponentialBackoffRetryStrategyConfig->jitterType == NO_JITTER, STATUS_INVALID_ARG);

    // Validate jitterFactor only if jitter type is jitterFactor
    if (pExponentialBackoffRetryStrategyConfig->jitterType == FIXED_JITTER) {
        CHK(CHECK_IN_RANGE(pExponentialBackoffRetryStrategyConfig->jitterFactor,
                           MIN_KVS_JITTER_FACTOR_MILLISECONDS, LIMIT_KVS_JITTER_FACTOR_MILLISECONDS), STATUS_INVALID_ARG);
    }


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

    // Normalize the time parameters in config to be in HUNDREDS_OF_NANOS_IN_A_MILLISECOND
    CHK_STATUS(normalizeExponentialBackoffConfig(pExponentialBackoffConfig));

    pExponentialBackoffRetryStrategyState = (PExponentialBackoffRetryStrategyState) MEMCALLOC(1, SIZEOF(ExponentialBackoffRetryStrategyState));
    CHK(pExponentialBackoffRetryStrategyState != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pExponentialBackoffRetryStrategyState->retryStrategyLock = MUTEX_CREATE(FALSE);

    pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig = *pExponentialBackoffConfig;
    CHK_STATUS(resetExponentialBackoffRetryState(pExponentialBackoffRetryStrategyState));
    DLOGD("Created exponential backoff retry strategy state with provided retry configuration.");

CleanUp:
    if (STATUS_SUCCEEDED(retStatus)) {
        (*ppRetryStrategy) = (PRetryStrategy)pExponentialBackoffRetryStrategyState;
    }

    LEAVES();
    return retStatus;
}

UINT64 getRandomJitter(ExponentialBackoffJitterType jitterType, UINT32 configuredJitterFactor, UINT64 currentRetryWaitTime) {
    UINT64 jitter = 0;
    switch (jitterType) {
        case FULL_JITTER:
            jitter = RAND() % currentRetryWaitTime;
            break;
        case FIXED_JITTER:
            jitter = RAND() % (configuredJitterFactor * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
            break;
        case NO_JITTER:
        default:
            return 0;

    }

    return jitter;
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
            CHK_ERR(FALSE, STATUS_EXPONENTIAL_BACKOFF_INVALID_STATE, "Exponential backoff is already terminated");
            // No 'break' needed since CHK(FALSE, ...) will always jump to CleanUp
        default:
            DLOGD("Cannot execute exponentialBackoffBlockingWait. Unexpected state [%"PRIu64"]", pExponentialBackoffRetryStrategyState->status);
            CHK_ERR(FALSE, STATUS_EXPONENTIAL_BACKOFF_INVALID_STATE, "Unexpected exponential backoff state");
    }

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS getExponentialBackoffRetryStrategyWaitTime(PRetryStrategy pRetryStrategy, PUINT64 retryWaitTime) {
    ENTERS();
    PExponentialBackoffRetryStrategyState pRetryState = NULL;
    PExponentialBackoffRetryStrategyConfig pRetryConfig = NULL;
    UINT64 currentSystemTime = 0, currentRetryWaitTime = 0, jitter = 0;
    STATUS retStatus = STATUS_SUCCESS;

    if (pRetryStrategy == NULL || retryWaitTime == NULL) {
        return STATUS_NULL_ARG;
    }

    pRetryState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy);
    MUTEX_LOCK(pRetryState->retryStrategyLock);

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
    jitter = getRandomJitter(pRetryConfig->jitterType,
                             pRetryConfig->jitterFactor,
                             currentRetryWaitTime);
    currentRetryWaitTime += jitter;

    // Update retry state's count and wait time values
    pRetryState->lastRetryWaitTime = currentRetryWaitTime;
    pRetryState->lastRetrySystemTime = currentSystemTime;
    pRetryState->currentRetryCount++;

    *retryWaitTime = currentRetryWaitTime;

    DLOGD("\n Thread Id [%"PRIu64"] "
          "Number of retries [%"PRIu64"], "
          "Retry wait time [%"PRIu64"], "
          "Retry system time [%"PRIu64"]",
          GETTID(), pRetryState->currentRetryCount, pRetryState->lastRetryWaitTime, pRetryState->lastRetrySystemTime);

CleanUp:
    if (retStatus == STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED) {
        DLOGD("Exhausted exponential retries");
        resetExponentialBackoffRetryState(pRetryState);
    }

    MUTEX_UNLOCK(pRetryState->retryStrategyLock);
    LEAVES();
    return retStatus;
}

STATUS exponentialBackoffRetryStrategyBlockingWait(PRetryStrategy pRetryStrategy) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 retryWaitTime;

    CHK_STATUS(getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTime));
    THREAD_SLEEP(retryWaitTime);

CleanUp:
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
    if (pExponentialBackoffRetryStrategyState != NULL) {
        MUTEX_FREE(pExponentialBackoffRetryStrategyState->retryStrategyLock);
    }

    SAFE_MEMFREE(pExponentialBackoffRetryStrategyState);
    *ppRetryStrategy = NULL;

    LEAVES();
    return retStatus;
}
