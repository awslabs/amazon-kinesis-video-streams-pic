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

    void getTestRetryConfiguration(PExponentialBackoffConfig pExponentialBackoffConfig) {
        // Set time to reset state to 5 seconds
        pExponentialBackoffConfig->minTimeToResetRetryState = 5000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        // If testing bounded retries, set max retry count to 5
        pExponentialBackoffConfig->maxRetryCount = KVS_INFINITE_EXPONENTIAL_RETRIES;
        // Set max exponential wait time to 3 seconds
        pExponentialBackoffConfig->maxRetryWaitTime = 3000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        // Set retry factor to 100 milliseconds.
        // With this value, the exponential curve will be -
        // ((2^1)*X + jitter), ((2^1)*X + jitter), ((2^2)*X + jitter), ((2^3)*X + jitter) ...
        // where X = 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND
        pExponentialBackoffConfig->retryFactorTime = 200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        // Set random jitter between [0, 4] * HUNDREDS_OF_NANOS_IN_A_MILLISECOND
        pExponentialBackoffConfig->jitterFactor = 50;
    }

    void validateExponentialBackoffWaitTime(
        PExponentialBackoffState pExponentialBackoffState,
        double currentTimeMilliSec,
        int expectedRetryCount,
        ExponentialBackoffStatus expectedExponentialBackoffStatus,
        Range& acceptableWaitTimeRange) {
        // Validate retry count is correctly incremented in ExponentialBackoffState
        EXPECT_EQ(expectedRetryCount, pExponentialBackoffState->currentRetryCount);
        // validate that the status is still "In Progress"
        EXPECT_EQ(expectedExponentialBackoffStatus, pExponentialBackoffState->status);
        // Record the actual wait time for validation
        double lastWaitTimeMilliSec = (pExponentialBackoffState->lastRetryWaitTime)/(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * 1.0);
        double actualWaitTimeMilliSec = lastWaitTimeMilliSec - currentTimeMilliSec;
        EXPECT_TRUE(inRange(lastWaitTimeMilliSec, acceptableWaitTimeRange));

        UINT64 lastRetrySystemTimeMilliSec = pExponentialBackoffState->lastRetrySystemTime/(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * 1.0);
        UINT64 diffInMilliSec = currentTimeMilliSec - lastRetrySystemTimeMilliSec;

        // Technically current time should be always be equal to last retry time
        // because we are capturing current time just before calling exponentialBackoffBlockingWait
        // But to avoid any flakiness, we're checking >= condition and an acceptable deviation
        // of 20 milliseconds.
        EXPECT_TRUE(currentTimeMilliSec >= lastRetrySystemTimeMilliSec);
        EXPECT_TRUE(diffInMilliSec < 20);
    }
};

TEST_F(ExponentialBackoffUtilsTest, testInitializeExponentialBackoffStateWithDefaultConfig)
{
    EXPECT_EQ(STATUS_NULL_ARG, exponentialBackoffStateWithDefaultConfigCreate(NULL));

    PExponentialBackoffState pExponentialBackoffState = NULL;
    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateWithDefaultConfigCreate(&pExponentialBackoffState));

    EXPECT_TRUE(pExponentialBackoffState != NULL);
    EXPECT_EQ(0, pExponentialBackoffState->currentRetryCount);
    EXPECT_EQ(0, pExponentialBackoffState->lastRetryWaitTime);
    EXPECT_EQ(BACKOFF_NOT_STARTED, pExponentialBackoffState->status);

    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS,
              pExponentialBackoffState->exponentialBackoffConfig.retryFactorTime);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS,
              pExponentialBackoffState->exponentialBackoffConfig.maxRetryWaitTime);
    EXPECT_EQ(KVS_INFINITE_EXPONENTIAL_RETRIES, pExponentialBackoffState->exponentialBackoffConfig.maxRetryCount);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS,
              pExponentialBackoffState->exponentialBackoffConfig.minTimeToResetRetryState);

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateFree(&pExponentialBackoffState));
    EXPECT_TRUE(pExponentialBackoffState == NULL);
}

TEST_F(ExponentialBackoffUtilsTest, testInitializeExponentialBackoffState)
{
    PExponentialBackoffState pExponentialBackoffState = NULL;
    PExponentialBackoffConfig pExponentialBackoffConfig = NULL;

    EXPECT_EQ(STATUS_NULL_ARG, exponentialBackoffStateCreate(NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, exponentialBackoffStateCreate(&pExponentialBackoffState, NULL));

    pExponentialBackoffConfig = (PExponentialBackoffConfig) MEMALLOC(SIZEOF(ExponentialBackoffConfig));
    EXPECT_TRUE(pExponentialBackoffConfig != NULL);

    EXPECT_EQ(STATUS_NULL_ARG, exponentialBackoffStateCreate(NULL, pExponentialBackoffConfig));

    pExponentialBackoffConfig->minTimeToResetRetryState = 25;
    pExponentialBackoffConfig->maxRetryCount = 5;
    pExponentialBackoffConfig->maxRetryWaitTime = 10;
    pExponentialBackoffConfig->retryFactorTime = 20;
    pExponentialBackoffConfig->jitterFactor = 1;
    // validateExponentialBackoffConfig will return error since the config values are not within acceptable range
    EXPECT_EQ(STATUS_INVALID_ARG, exponentialBackoffStateCreate(&pExponentialBackoffState, pExponentialBackoffConfig));
    EXPECT_TRUE(pExponentialBackoffState == NULL);

    // Use correct config values
    pExponentialBackoffConfig->minTimeToResetRetryState = 25000;
    pExponentialBackoffConfig->maxRetryCount = 5;
    pExponentialBackoffConfig->maxRetryWaitTime = 15000;
    pExponentialBackoffConfig->retryFactorTime = 300;
    pExponentialBackoffConfig->jitterFactor = 300;

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateCreate(&pExponentialBackoffState, pExponentialBackoffConfig));
    EXPECT_TRUE(pExponentialBackoffState != NULL);

    EXPECT_EQ(25000, pExponentialBackoffState->exponentialBackoffConfig.minTimeToResetRetryState);
    EXPECT_EQ(5, pExponentialBackoffState->exponentialBackoffConfig.maxRetryCount);
    EXPECT_EQ(15000, pExponentialBackoffState->exponentialBackoffConfig.maxRetryWaitTime);
    EXPECT_EQ(300, pExponentialBackoffState->exponentialBackoffConfig.retryFactorTime);
    EXPECT_EQ(300, pExponentialBackoffState->exponentialBackoffConfig.jitterFactor);

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateFree(&pExponentialBackoffState));
    EXPECT_TRUE(pExponentialBackoffState == NULL);
    SAFE_MEMFREE(pExponentialBackoffConfig);
}

/**
 * This test validates un-bounded exponentially backed-off retries with an upper
 * bound on the actual retry wait time.
 * The test performs 7 consecutive and validates the wait time for each retry.
 * The retry config has an upper limit on
 * The test verifies reception of error upon calling exponentialBackoffBlockingWait after
 * configured retries are exhausted.
 *
 * This test takes roughly 15 seconds to execute
 */
TEST_F(ExponentialBackoffUtilsTest, testExponentialBackoffBlockingWait_Unbounded)
{
    PExponentialBackoffState pExponentialBackoffState = NULL;

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateWithDefaultConfigCreate(&pExponentialBackoffState));
    EXPECT_TRUE(pExponentialBackoffState != NULL);
    // Overwrite the default config values with test config values
    getTestRetryConfiguration(&(pExponentialBackoffState->exponentialBackoffConfig));

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

    for (int retryCount = 0; retryCount < 7; retryCount++) {
        double currentTimeMilliSec = GETTIME()/(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * 1.0);
        EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffBlockingWait(pExponentialBackoffState));
        validateExponentialBackoffWaitTime(
            pExponentialBackoffState,
            currentTimeMilliSec,
            retryCount+1,
            BACKOFF_IN_PROGRESS,
            acceptableRetryWaitTimeRangeMilliSec.at(retryCount));
    }

    // We have configured minTimeToResetRetryState as 5 seconds. So sleep for just over 5 seconds
    // and then verify the next retry has retry count as 1
    std::this_thread::sleep_for(std::chrono::milliseconds(5100));
    double currentTimeMilliSec = GETTIME()/(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * 1.0);
    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffBlockingWait(pExponentialBackoffState));
    validateExponentialBackoffWaitTime(
        pExponentialBackoffState,
        currentTimeMilliSec,
        1, // expected current retry count (1st retry)
        BACKOFF_IN_PROGRESS, // expected retry state
        acceptableRetryWaitTimeRangeMilliSec.at(0));

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateFree(&pExponentialBackoffState));
    EXPECT_TRUE(pExponentialBackoffState == NULL);
}

/**
 * Test to validate bounded exponentially backed-off retries.
 * The test performs 5 consecutive and validates the wait time for each retry.
 * The test verifies reception of error upon calling exponentialBackoffBlockingWait after
 * configured retries are exhausted.
 *
 * This test takes roughly 6 seconds to execute
 */
TEST_F(ExponentialBackoffUtilsTest, testExponentialBackoffBlockingWait_Bounded)
{
    PExponentialBackoffState pExponentialBackoffState = NULL;
    UINT32 maxTestRetryCount = 5;

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateWithDefaultConfigCreate(&pExponentialBackoffState));
    EXPECT_TRUE(pExponentialBackoffState != NULL);
    // Overwrite the default config values with test config values
    getTestRetryConfiguration(&(pExponentialBackoffState->exponentialBackoffConfig));
    pExponentialBackoffState->exponentialBackoffConfig.maxRetryCount = maxTestRetryCount;

    std::vector<double> actualRetryWaitTime;
    std::vector<Range> acceptableRetryWaitTimeRangeMilliSec {
        {200.0, 300.0},     // 1st retry 200ms + jitter
        {400.0, 500.0},     // 2st retry 400ms + jitter
        {800.0, 900.0},     // 3st retry 800ms + jitter
        {1600.0, 1700.0},   // 4st retry 1600ms + jitter
        {3000.0, 3100.0},   // 5st retry 3000ms + jitter
    };

    for (int retryCount = 0; retryCount < maxTestRetryCount; retryCount++) {
        double currentTimeMilliSec = GETTIME()/(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * 1.0);
        EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffBlockingWait(pExponentialBackoffState));
        validateExponentialBackoffWaitTime(
            pExponentialBackoffState,
            currentTimeMilliSec,
            retryCount+1,
            BACKOFF_IN_PROGRESS,
            acceptableRetryWaitTimeRangeMilliSec.at(retryCount));
    }

    // After the retries are exhausted, subsequent call to exponentialBackoffBlockingWait should return an error
    EXPECT_EQ(STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED, exponentialBackoffBlockingWait(pExponentialBackoffState));
    EXPECT_EQ(0, pExponentialBackoffState->currentRetryCount);
    EXPECT_EQ(0, pExponentialBackoffState->lastRetryWaitTime);
    EXPECT_EQ(BACKOFF_NOT_STARTED, pExponentialBackoffState->status);

    EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateFree(&pExponentialBackoffState));
    EXPECT_TRUE(pExponentialBackoffState == NULL);
}
