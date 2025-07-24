#include "ClientTestFixture.h"

class RetryStrategyConfigurationTest : public ClientTestBase {
  public:
    void setupCustomRetryConfiguration(PClientInfo pClientInfo, PExponentialBackoffRetryStrategyConfig pCustomConfig) {
        // Set up custom retry configuration values that are different from defaults
        // Values are in MILLISECONDS (will be converted internally to hundreds of nanos)
        pCustomConfig->maxRetryCount = 10; // Different from default KVS_INFINITE_EXPONENTIAL_RETRIES
        pCustomConfig->maxRetryWaitTime = 15000; // 15 seconds (min is 10000ms)
        pCustomConfig->retryFactorTime = 100; // 100ms (min is 50ms)
        pCustomConfig->minTimeToResetRetryState = 120000; // 120 seconds (min is 90000ms)
        pCustomConfig->jitterType = FIXED_JITTER; // Different from default FULL_JITTER
        pCustomConfig->jitterFactor = 100; // Custom jitter factor (min is 50)

        // Set up retry strategy with custom config
        pClientInfo->kvsRetryStrategy.pRetryStrategyConfig = (PRetryStrategyConfig) pCustomConfig;
        pClientInfo->kvsRetryStrategy.retryStrategyType = KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT;

        // Set up retry strategy callbacks
        pClientInfo->kvsRetryStrategyCallbacks.createRetryStrategyFn = createRetryStrategyFn;
        pClientInfo->kvsRetryStrategyCallbacks.freeRetryStrategyFn = freeRetryStrategyFn;
        pClientInfo->kvsRetryStrategyCallbacks.executeRetryStrategyFn = executeRetryStrategyFn;
        pClientInfo->kvsRetryStrategyCallbacks.getCurrentRetryAttemptNumberFn = getCurrentRetryAttemptNumberFn;
    }

    void verifyRetryConfiguration(CLIENT_HANDLE clientHandle, 
                                  PExponentialBackoffRetryStrategyConfig pExpectedConfig) {
        // Get the client from handle
        PKinesisVideoClient pKinesisVideoClient = FROM_CLIENT_HANDLE(clientHandle);
        
        // Get the actual retry strategy from the client
        PKvsRetryStrategy pActualRetryStrategy = &(pKinesisVideoClient->deviceInfo.clientInfo.kvsRetryStrategy);
        
        EXPECT_EQ(KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT, pActualRetryStrategy->retryStrategyType);
        EXPECT_NE(nullptr, pActualRetryStrategy->pRetryStrategy);

        // Get the actual configuration from the retry strategy state
        PExponentialBackoffRetryStrategyState pRetryState = TO_EXPONENTIAL_BACKOFF_STATE(pActualRetryStrategy->pRetryStrategy);
        EXPECT_NE(nullptr, pRetryState);

        PExponentialBackoffRetryStrategyConfig pActualConfig = &(pRetryState->exponentialBackoffRetryStrategyConfig);

        // Verify all custom configuration values are preserved
        // Note: Internal values are stored in hundreds of nanos, so we need to convert for comparison
        EXPECT_EQ(pExpectedConfig->maxRetryCount, pActualConfig->maxRetryCount);
        EXPECT_EQ(pExpectedConfig->maxRetryWaitTime * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, pActualConfig->maxRetryWaitTime);
        EXPECT_EQ(pExpectedConfig->retryFactorTime * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, pActualConfig->retryFactorTime);
        EXPECT_EQ(pExpectedConfig->minTimeToResetRetryState * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, pActualConfig->minTimeToResetRetryState);
        EXPECT_EQ(pExpectedConfig->jitterType, pActualConfig->jitterType);
        EXPECT_EQ(pExpectedConfig->jitterFactor, pActualConfig->jitterFactor);
    }

    void verifyRetryCallbacks(CLIENT_HANDLE clientHandle) {
        // Get the client from handle
        PKinesisVideoClient pKinesisVideoClient = FROM_CLIENT_HANDLE(clientHandle);
        
        // Verify retry strategy callbacks are preserved
        KvsRetryStrategyCallbacks* pActualCallbacks = &(pKinesisVideoClient->deviceInfo.clientInfo.kvsRetryStrategyCallbacks);
        
        EXPECT_EQ(createRetryStrategyFn, pActualCallbacks->createRetryStrategyFn);
        EXPECT_EQ(freeRetryStrategyFn, pActualCallbacks->freeRetryStrategyFn);
        EXPECT_EQ(executeRetryStrategyFn, pActualCallbacks->executeRetryStrategyFn);
        EXPECT_EQ(getCurrentRetryAttemptNumberFn, pActualCallbacks->getCurrentRetryAttemptNumberFn);
    }
};

/**
 * Test that custom retry strategy configuration is preserved through client creation
 * This test would have FAILED before the fix because fixupClientInfo wasn't copying
 * the kvsRetryStrategy and kvsRetryStrategyCallbacks fields.
 */
TEST_F(RetryStrategyConfigurationTest, CustomRetryConfigurationPreservedThroughClientCreation)
{
    ExponentialBackoffRetryStrategyConfig customRetryConfig;
    DeviceInfo deviceInfo;
    CLIENT_HANDLE clientHandle;

    // Initialize structures
    MEMSET(&customRetryConfig, 0x00, SIZEOF(ExponentialBackoffRetryStrategyConfig));
    MEMSET(&deviceInfo, 0x00, SIZEOF(DeviceInfo));

    // Set up device info based on existing test pattern
    deviceInfo = mDeviceInfo; // Copy from base test fixture
    
    // Set up custom retry configuration
    setupCustomRetryConfiguration(&deviceInfo.clientInfo, &customRetryConfig);

    // Create client - this triggers fixupClientInfo which should preserve our custom config
    EXPECT_EQ(STATUS_SUCCESS, createKinesisVideoClient(&deviceInfo, &mClientCallbacks, &clientHandle));

    // Verify that our custom retry configuration survived the client creation process
    verifyRetryConfiguration(clientHandle, &customRetryConfig);
    verifyRetryCallbacks(clientHandle);

    // Clean up
    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoClient(&clientHandle));
}

/**
 * Test that retry strategy configuration works end-to-end with actual retry behavior
 * This verifies that the preserved configuration is actually used during retries
 */
TEST_F(RetryStrategyConfigurationTest, CustomRetryConfigurationUsedInRetryBehavior)
{
    ExponentialBackoffRetryStrategyConfig customRetryConfig;
    DeviceInfo deviceInfo;
    CLIENT_HANDLE clientHandle;

    // Initialize structures
    MEMSET(&customRetryConfig, 0x00, SIZEOF(ExponentialBackoffRetryStrategyConfig));
    MEMSET(&deviceInfo, 0x00, SIZEOF(DeviceInfo));

    // Set up device info based on existing test pattern
    deviceInfo = mDeviceInfo; // Copy from base test fixture

    // Set up custom retry configuration with bounded retries for testing
    customRetryConfig.maxRetryCount = 3; // Bounded retries for testing
    customRetryConfig.maxRetryWaitTime = 1000; // 1 second max (in milliseconds)
    customRetryConfig.retryFactorTime = 100; // 100ms base (in milliseconds)
    customRetryConfig.minTimeToResetRetryState = 95000; // 95 seconds (min is 90000ms)
    customRetryConfig.jitterType = NO_JITTER; // No jitter for predictable testing
    customRetryConfig.jitterFactor = 0;

    // Set up retry configuration
    setupCustomRetryConfiguration(&deviceInfo.clientInfo, &customRetryConfig);

    // Create client
    EXPECT_EQ(STATUS_SUCCESS, createKinesisVideoClient(&deviceInfo, &mClientCallbacks, &clientHandle));

    // Get the client and retry strategy for testing actual retry behavior
    PKinesisVideoClient pKinesisVideoClient = FROM_CLIENT_HANDLE(clientHandle);
    KvsRetryStrategy* pRetryStrategy = &(pKinesisVideoClient->deviceInfo.clientInfo.kvsRetryStrategy);
    UINT64 retryWaitTime = 0;
    UINT32 retryCount = 0;

    // Test first retry - should succeed and return expected wait time
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTime));
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryCount(pRetryStrategy, &retryCount));
    EXPECT_EQ(1, retryCount);
    EXPECT_EQ(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, retryWaitTime); // Should match our custom base time

    // Test second retry
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTime));
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryCount(pRetryStrategy, &retryCount));
    EXPECT_EQ(2, retryCount);
    EXPECT_EQ(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, retryWaitTime); // 2^1 * base time

    // Test third retry
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTime));
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryCount(pRetryStrategy, &retryCount));
    EXPECT_EQ(3, retryCount);
    EXPECT_EQ(400 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, retryWaitTime); // 2^2 * base time

    // Test fourth retry - should fail because we set maxRetryCount to 3
    EXPECT_EQ(STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED, 
              getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTime));

    // Clean up
    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoClient(&clientHandle));
}

/**
 * Test that default configuration is used when no custom config is provided
 * This ensures we didn't break the default behavior
 */
TEST_F(RetryStrategyConfigurationTest, DefaultRetryConfigurationWhenNoCustomConfigProvided)
{
    CLIENT_HANDLE clientHandle;

    // Create client with default configuration (from mDeviceInfo)
    EXPECT_EQ(STATUS_SUCCESS, createKinesisVideoClient(&mDeviceInfo, &mClientCallbacks, &clientHandle));

    // Get the client and verify that default retry configuration is used
    PKinesisVideoClient pKinesisVideoClient = FROM_CLIENT_HANDLE(clientHandle);
    KvsRetryStrategy* pRetryStrategy = &(pKinesisVideoClient->deviceInfo.clientInfo.kvsRetryStrategy);
    EXPECT_EQ(KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT, pRetryStrategy->retryStrategyType);
    
    PExponentialBackoffRetryStrategyState pRetryState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy->pRetryStrategy);
    EXPECT_NE(nullptr, pRetryState);

    PExponentialBackoffRetryStrategyConfig pActualConfig = &(pRetryState->exponentialBackoffRetryStrategyConfig);

    // Verify default values are set
    EXPECT_EQ(KVS_INFINITE_EXPONENTIAL_RETRIES, pActualConfig->maxRetryCount);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS, 
              pActualConfig->maxRetryWaitTime);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS, 
              pActualConfig->retryFactorTime);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS, 
              pActualConfig->minTimeToResetRetryState);
    EXPECT_EQ(FULL_JITTER, pActualConfig->jitterType);

    // Clean up
    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoClient(&clientHandle));
}
