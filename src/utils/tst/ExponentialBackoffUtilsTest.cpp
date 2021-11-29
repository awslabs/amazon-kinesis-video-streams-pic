#include "UtilTestFixture.h"
#include <vector>
#include <chrono>
#include <thread>

typedef struct __Range {
    double low;
    double high;
} Range;

class ExponentialBackoffUtilsTest : public UtilTestBase {
public:
    bool inRange(double number, Range& range) {
        return (range.low <= number && number <= range.high);
    }

    void getTestRetryConfiguration(PExponentialBackoffRetryStrategyConfig pExponentialBackoffRetryStrategyConfig) {
        // Set time to reset state to 5 seconds
        pExponentialBackoffRetryStrategyConfig->minTimeToResetRetryState = 5000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        // If testing bounded retries, set max retry count to 5
        pExponentialBackoffRetryStrategyConfig->maxRetryCount = KVS_INFINITE_EXPONENTIAL_RETRIES;
        // Set max exponential wait time to 3 seconds
        pExponentialBackoffRetryStrategyConfig->maxRetryWaitTime = 3000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        // Set retry factor to 100 milliseconds.
        // With this value, the exponential curve will be -
        // ((2^1)*X + jitter), ((2^1)*X + jitter), ((2^2)*X + jitter), ((2^3)*X + jitter) ...
        // where X = 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND
        pExponentialBackoffRetryStrategyConfig->retryFactorTime = 200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        // Set random jitter between [0, 4] * HUNDREDS_OF_NANOS_IN_A_MILLISECOND
        pExponentialBackoffRetryStrategyConfig->jitterFactor = 50;
        // Set jitter type as fixed jitter
        pExponentialBackoffRetryStrategyConfig->jitterType = FIXED_JITTER;

        DLOGD("\nThread Id [%lu]. Over writing exponential retry strategy config to - "
              "maxRetryCount: [%lu], "
              "maxRetryWaitTime: [%lu], "
              "retryFactorTime: [%lu], "
              "minTimeToResetRetryState: [%lu], "
              "jitterType: [%lu], "
              "jitterFactor: [%lu], ",
              GETTID(),
              pExponentialBackoffRetryStrategyConfig->maxRetryCount,
              pExponentialBackoffRetryStrategyConfig->maxRetryWaitTime,
              pExponentialBackoffRetryStrategyConfig->retryFactorTime,
              pExponentialBackoffRetryStrategyConfig->minTimeToResetRetryState,
              pExponentialBackoffRetryStrategyConfig->jitterType,
              pExponentialBackoffRetryStrategyConfig->jitterFactor);
    }

    void validateExponentialBackoffWaitTime(
            PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState,
            UINT64 retryWaitTimeToVerify,
            int expectedRetryCount,
            ExponentialBackoffStatus expectedExponentialBackoffStatus,
            Range& acceptableWaitTimeRange) {
        // Validate retry count is correctly incremented in ExponentialBackoffState
        EXPECT_EQ(expectedRetryCount, pExponentialBackoffRetryStrategyState->currentRetryCount);
        // validate that the status is still "In Progress"
        EXPECT_EQ(expectedExponentialBackoffStatus, pExponentialBackoffRetryStrategyState->status);
        // Record the actual wait time for validation
        EXPECT_EQ(retryWaitTimeToVerify, pExponentialBackoffRetryStrategyState->lastRetryWaitTime);
        EXPECT_TRUE(inRange(retryWaitTimeToVerify/HUNDREDS_OF_NANOS_IN_A_MILLISECOND, acceptableWaitTimeRange));

        UINT64 lastRetrySystemTimeMilliSec = pExponentialBackoffRetryStrategyState->lastRetrySystemTime/(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * 1.0);
        UINT64 currentTimeMilliSec = GETTIME()/HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        UINT64 diffInMilliSec = currentTimeMilliSec - lastRetrySystemTimeMilliSec;

        EXPECT_TRUE(currentTimeMilliSec >= lastRetrySystemTimeMilliSec);
        // To avoid any flakiness, we're checking >= condition and an
        // acceptable deviation of 20 milliseconds.
        EXPECT_TRUE(diffInMilliSec < 20);
    }
};

TEST_F(ExponentialBackoffUtilsTest, testInitializeExponentialBackoffStateWithDefaultConfig)
{
    EXPECT_EQ(STATUS_NULL_ARG, exponentialBackoffRetryStrategyWithDefaultConfigCreate(NULL));

    PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState = NULL;
    PRetryStrategy pRetryStrategy = NULL;
    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyWithDefaultConfigCreate(&pRetryStrategy));
    pExponentialBackoffRetryStrategyState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy);

    EXPECT_TRUE(pExponentialBackoffRetryStrategyState != NULL);
    EXPECT_EQ(0, pExponentialBackoffRetryStrategyState->currentRetryCount);
    EXPECT_EQ(0, pExponentialBackoffRetryStrategyState->lastRetryWaitTime);
    EXPECT_EQ(BACKOFF_NOT_STARTED, pExponentialBackoffRetryStrategyState->status);

    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS,
              pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.retryFactorTime);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS,
              pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.maxRetryWaitTime);
    EXPECT_EQ(KVS_INFINITE_EXPONENTIAL_RETRIES, pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.maxRetryCount);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS,
              pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.minTimeToResetRetryState);
    EXPECT_EQ(FULL_JITTER, pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.jitterType);

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyFree(&pRetryStrategy));
    EXPECT_TRUE(pRetryStrategy == NULL);


    // Should call exponentialBackoffRetryStrategyWithDefaultConfigCreate if optional config parameter is NULL
    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyCreate(NULL, &pRetryStrategy));
    pExponentialBackoffRetryStrategyState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy);

    EXPECT_TRUE(pExponentialBackoffRetryStrategyState != NULL);
    EXPECT_EQ(0, pExponentialBackoffRetryStrategyState->currentRetryCount);
    EXPECT_EQ(0, pExponentialBackoffRetryStrategyState->lastRetryWaitTime);
    EXPECT_EQ(BACKOFF_NOT_STARTED, pExponentialBackoffRetryStrategyState->status);

    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS,
              pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.retryFactorTime);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS,
              pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.maxRetryWaitTime);
    EXPECT_EQ(KVS_INFINITE_EXPONENTIAL_RETRIES, pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.maxRetryCount);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS,
              pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.minTimeToResetRetryState);

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyFree(&pRetryStrategy));
    EXPECT_TRUE(pRetryStrategy == NULL);
}

TEST_F(ExponentialBackoffUtilsTest, testInitializeExponentialBackoffState)
{
    PRetryStrategy pRetryStrategy = NULL;
    PRetryStrategyConfig pRetryStrategyConfig = NULL;
    PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState = NULL;
    PExponentialBackoffRetryStrategyConfig pExponentialBackoffRetryStrategyConfig = NULL;

    EXPECT_EQ(STATUS_NULL_ARG, exponentialBackoffRetryStrategyCreate(NULL, NULL));

    pExponentialBackoffRetryStrategyConfig = (PExponentialBackoffRetryStrategyConfig) MEMALLOC(SIZEOF(ExponentialBackoffRetryStrategyConfig));
    EXPECT_TRUE(pExponentialBackoffRetryStrategyConfig != NULL);
    pRetryStrategyConfig = (PRetryStrategyConfig)pExponentialBackoffRetryStrategyConfig;

    EXPECT_EQ(STATUS_NULL_ARG, exponentialBackoffRetryStrategyCreate(pRetryStrategyConfig, NULL));

    pExponentialBackoffRetryStrategyConfig->minTimeToResetRetryState = 25;
    pExponentialBackoffRetryStrategyConfig->maxRetryCount = 5;
    pExponentialBackoffRetryStrategyConfig->maxRetryWaitTime = 10;
    pExponentialBackoffRetryStrategyConfig->retryFactorTime = 20;
    pExponentialBackoffRetryStrategyConfig->jitterFactor = 1;

    // validateExponentialBackoffConfig will return error since the config values are not within acceptable range
    EXPECT_EQ(STATUS_INVALID_ARG, exponentialBackoffRetryStrategyCreate(pRetryStrategyConfig, &pRetryStrategy));
    pExponentialBackoffRetryStrategyState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy);
    EXPECT_TRUE(pExponentialBackoffRetryStrategyState == NULL);

    // Use correct config values
    pExponentialBackoffRetryStrategyConfig->minTimeToResetRetryState = 90000;
    pExponentialBackoffRetryStrategyConfig->maxRetryCount = 5;
    pExponentialBackoffRetryStrategyConfig->maxRetryWaitTime = 20000;
    pExponentialBackoffRetryStrategyConfig->retryFactorTime = 1000;
    pExponentialBackoffRetryStrategyConfig->jitterType = FIXED_JITTER;
    pExponentialBackoffRetryStrategyConfig->jitterFactor = 600;

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyCreate(pRetryStrategyConfig, &pRetryStrategy));
    pExponentialBackoffRetryStrategyState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy);
    EXPECT_TRUE(pExponentialBackoffRetryStrategyState != NULL);

    EXPECT_EQ(90000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.minTimeToResetRetryState);
    EXPECT_EQ(5, pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.maxRetryCount);
    EXPECT_EQ(20000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.maxRetryWaitTime);
    EXPECT_EQ(1000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.retryFactorTime);
    EXPECT_EQ(FIXED_JITTER, pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.jitterType);
    EXPECT_EQ(600, pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.jitterFactor);

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyFree(&pRetryStrategy));
    EXPECT_TRUE(pRetryStrategy == NULL);
    SAFE_MEMFREE(pExponentialBackoffRetryStrategyConfig);
}

/**
 * This test validates un-bounded exponentially backed-off retries with an upper
 * bound on the actual retry wait time.
 * The test performs 7 consecutive and validates the wait time for each retry.
 * The retry config has an upper limit on
 * The test verifies reception of error upon calling exponentialBackoffRetryStrategyBlockingWait after
 * configured retries are exhausted.
 */
TEST_F(ExponentialBackoffUtilsTest, testExponentialBackoffBlockingWait_Unbounded)
{
    PRetryStrategy pRetryStrategy = NULL;
    PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState = NULL;

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyWithDefaultConfigCreate(&pRetryStrategy));
    pExponentialBackoffRetryStrategyState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy);
    EXPECT_TRUE(pExponentialBackoffRetryStrategyState != NULL);
    // Overwrite the default config values with test config values
    getTestRetryConfiguration(&(pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig));

    std::vector<double> actualRetryWaitTime;
    std::vector<Range> acceptableRetryWaitTimeRangeMilliSec {
        {200.0, 300.0},     // 1st retry 200ms + jitter
        {400.0, 500.0},     // 2st retry 400ms + jitter
        {800.0, 900.0},     // 3st retry 800ms + jitter
        {1600.0, 1700.0},   // 4st retry 1600ms + jitter
        {3000.0, 3100.0},   // 5st retry 3000ms + jitter
        {3000.0, 3100.0},   // 6st retry 3000ms + jitter
        {3000.0, 3100.0}    // 7st retry 3000ms + jitter
    };

    UINT64 retryWaitTimeToVerify;
    for (int retryCount = 0; retryCount < 7; retryCount++) {
        retryWaitTimeToVerify = 0;
        EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTimeToVerify));
        validateExponentialBackoffWaitTime(
                pExponentialBackoffRetryStrategyState,
                retryWaitTimeToVerify,
                retryCount+1,
                BACKOFF_IN_PROGRESS,
                acceptableRetryWaitTimeRangeMilliSec.at(retryCount));
    }

    // We have configured minTimeToResetRetryState as 5 seconds. So sleep for just over 5 seconds
    // and then verify the next retry has retry count as 1
    std::this_thread::sleep_for(std::chrono::milliseconds(5100));
    retryWaitTimeToVerify = 0;
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTimeToVerify));
    validateExponentialBackoffWaitTime(
            pExponentialBackoffRetryStrategyState,
            retryWaitTimeToVerify,
            1, // expected current retry count (1st retry)
            BACKOFF_IN_PROGRESS, // expected retry state
            acceptableRetryWaitTimeRangeMilliSec.at(0));

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyFree(&pRetryStrategy));
    EXPECT_TRUE(pRetryStrategy == NULL);
}

/**
 * Test to validate bounded exponentially backed-off retries.
 * The test performs 5 consecutive and validates the wait time for each retry.
 * The test verifies reception of error upon calling exponentialBackoffRetryStrategyBlockingWait after
 * configured retries are exhausted.
 */
TEST_F(ExponentialBackoffUtilsTest, testExponentialBackoffBlockingWait_Bounded)
{
    PRetryStrategy pRetryStrategy = NULL;
    PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState = NULL;
    UINT32 maxTestRetryCount = 5;

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyWithDefaultConfigCreate(&pRetryStrategy));
    pExponentialBackoffRetryStrategyState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy);
    EXPECT_TRUE(pExponentialBackoffRetryStrategyState != NULL);
    // Overwrite the default config values with test config values
    getTestRetryConfiguration(&(pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig));
    pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.maxRetryCount = maxTestRetryCount;

    std::vector<double> actualRetryWaitTime;
    std::vector<Range> acceptableRetryWaitTimeRangeMilliSec {
        {200.0, 300.0},     // 1st retry 200ms + jitter
        {400.0, 500.0},     // 2st retry 400ms + jitter
        {800.0, 900.0},     // 3st retry 800ms + jitter
        {1600.0, 1700.0},   // 4st retry 1600ms + jitter
        {3000.0, 3100.0},   // 5st retry 3000ms + jitter
    };

    UINT64 retryWaitTimeToVerify;
    for (int retryCount = 0; retryCount < maxTestRetryCount; retryCount++) {
        retryWaitTimeToVerify = 0;
        EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTimeToVerify));
        validateExponentialBackoffWaitTime(
                pExponentialBackoffRetryStrategyState,
                retryWaitTimeToVerify,
                retryCount+1,
                BACKOFF_IN_PROGRESS,
                acceptableRetryWaitTimeRangeMilliSec.at(retryCount));
    }

    // After the retries are exhausted, subsequent call to exponentialBackoffRetryStrategyBlockingWait should return an error
    EXPECT_EQ(STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED, exponentialBackoffRetryStrategyBlockingWait(pRetryStrategy));
    EXPECT_EQ(0, pExponentialBackoffRetryStrategyState->currentRetryCount);
    EXPECT_EQ(0, pExponentialBackoffRetryStrategyState->lastRetryWaitTime);
    EXPECT_EQ(BACKOFF_NOT_STARTED, pExponentialBackoffRetryStrategyState->status);

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyFree(&pRetryStrategy));
    EXPECT_TRUE(pRetryStrategy == NULL);
}

/**
 * Test to validate bounded exponentially backed-off retries for full jitter variant
 * The test performs 5 consecutive and validates the wait time for each retry.
 * The test verifies reception of error upon calling exponentialBackoffBlockingWait after
 * configured retries are exhausted.
 */
TEST_F(ExponentialBackoffUtilsTest, testExponentialBackoffBlockingWait_FullJitter_Bounded)
{
    PRetryStrategy pRetryStrategy = NULL;
    PExponentialBackoffRetryStrategyState pExponentialBackoffRetryStrategyState = NULL;
    UINT32 maxTestRetryCount = 5;

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyWithDefaultConfigCreate(&pRetryStrategy));
    pExponentialBackoffRetryStrategyState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy);
    EXPECT_TRUE(pExponentialBackoffRetryStrategyState != NULL);

    // Overwrite the default config values with test config values
    getTestRetryConfiguration(&(pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig));
    pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.maxRetryCount = maxTestRetryCount;
    pExponentialBackoffRetryStrategyState->exponentialBackoffRetryStrategyConfig.jitterType = FULL_JITTER;

    std::vector<double> actualRetryWaitTime;
    std::vector<Range> acceptableRetryWaitTimeRangeMilliSec {
        {200.0, 399.0},     // 1st retry 200ms + jitter
        {400.0, 799.0},     // 2st retry 400ms + jitter
        {800.0, 1599.0},     // 3st retry 800ms + jitter
        {1600.0, 3199.0},   // 4st retry 1600ms + jitter
        {3000.0, 5999.0},   // 5st retry 3000ms + jitter
    };

    UINT64 retryWaitTimeToVerify;
    for (int retryCount = 0; retryCount < maxTestRetryCount; retryCount++) {
        retryWaitTimeToVerify = 0;
        EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTimeToVerify));
        validateExponentialBackoffWaitTime(
                pExponentialBackoffRetryStrategyState,
                retryWaitTimeToVerify,
                retryCount+1,
                BACKOFF_IN_PROGRESS,
                acceptableRetryWaitTimeRangeMilliSec.at(retryCount));
    }

    // After the retries are exhausted, subsequent call to exponentialBackoffRetryStrategyBlockingWait should return an error
    EXPECT_EQ(STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED, exponentialBackoffRetryStrategyBlockingWait(pRetryStrategy));
    EXPECT_EQ(0, pExponentialBackoffRetryStrategyState->currentRetryCount);
    EXPECT_EQ(0, pExponentialBackoffRetryStrategyState->lastRetryWaitTime);
    EXPECT_EQ(BACKOFF_NOT_STARTED, pExponentialBackoffRetryStrategyState->status);

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffRetryStrategyFree(&pRetryStrategy));
    EXPECT_TRUE(pRetryStrategy == NULL);
}