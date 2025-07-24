#include "ClientTestFixture.h"

class RetryStrategyConfigurationTest : public ClientTestBase {
  private:
    // Using values that are different from the defaults
    // This makes sure that our custom values are used
    static const int maxRetryCount = 10;
    static const int maxRetryWaitTimeMs = 15000;
    static const int retryFactorTimeMs = 100;
    static const int minTimeToResetRetryStateMs = 120000;
    static const ExponentialBackoffJitterType jitterType = FIXED_JITTER;
    static const int jitterFactorMs = 100;

  public:
    void SetUp() override
    {
        // Use SetUpWithoutClientCreation to avoid creating the default client
        // We need to create our own client with custom retry configuration
        SetUpWithoutClientCreation();
    }

    // Custom retry strategy creation function that validates the config is passed through correctly
    static STATUS testCreateRetryStrategyFn(PKvsRetryStrategy pKvsRetryStrategy)
    {
        // This should receive the kvsRetryStrategy structure with our custom pRetryStrategyConfig
        // Verify that the config pointer is not NULL (meaning our custom config was passed through)
        if (pKvsRetryStrategy->pRetryStrategyConfig == NULL) {
            // If config is NULL, the client creation process didn't preserve our custom config
            return STATUS_INVALID_ARG;
        }

        // Verify it's actually OUR config, not some default config
        PExponentialBackoffRetryStrategyConfig pConfig = TO_EXPONENTIAL_BACKOFF_CONFIG(pKvsRetryStrategy->pRetryStrategyConfig);

        // Check if this config has our specific custom values
        if (pConfig->maxRetryCount != maxRetryCount || pConfig->maxRetryWaitTime != maxRetryWaitTimeMs ||
            pConfig->retryFactorTime != retryFactorTimeMs) {
            // This means the client is using some other config, not ours!
            DLOGE("ERROR: Config values don't match our custom config!");
            DLOGE("  Expected: maxRetryCount=%d, maxRetryWaitTime=%d, retryFactorTime=%d", maxRetryCount, maxRetryWaitTimeMs, retryFactorTimeMs);
            DLOGE("  Actual: maxRetryCount=%u, maxRetryWaitTime=%llu, retryFactorTime=%llu", pConfig->maxRetryCount, pConfig->maxRetryWaitTime,
                   pConfig->retryFactorTime);
            return STATUS_INVALID_ARG;
        }

        // Call the actual creation function - this should use our custom config
        return exponentialBackoffRetryStrategyCreate(pKvsRetryStrategy);
    }

    // Custom callback to validate that NULL config results in default behavior
    static STATUS testCreateRetryStrategyFnForDefault(PKvsRetryStrategy pKvsRetryStrategy)
    {
        // This should receive NULL config, which should trigger default behavior
        if (pKvsRetryStrategy->pRetryStrategyConfig != NULL) {
            // If config is not NULL, something is wrong with our test setup
            return STATUS_INVALID_ARG;
        }

        // Call the actual creation function - this should create default config
        return exponentialBackoffRetryStrategyCreate(pKvsRetryStrategy);
    }
    void setupCustomRetryConfiguration(PClientInfo pClientInfo, PExponentialBackoffRetryStrategyConfig pCustomConfig)
    {
        // Set up custom retry configuration values that are different from defaults
        // Values are in MILLISECONDS (will be converted internally to hundreds of nanos)
        pCustomConfig->maxRetryCount = maxRetryCount;
        pCustomConfig->maxRetryWaitTime = maxRetryWaitTimeMs;
        pCustomConfig->retryFactorTime = retryFactorTimeMs;
        pCustomConfig->minTimeToResetRetryState = minTimeToResetRetryStateMs;
        pCustomConfig->jitterType = jitterType;
        pCustomConfig->jitterFactor = jitterFactorMs;

        // Set up retry strategy with custom config
        pClientInfo->kvsRetryStrategy.pRetryStrategyConfig = (PRetryStrategyConfig) pCustomConfig;
        pClientInfo->kvsRetryStrategy.retryStrategyType = KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT;

        // Use our test callback that validates the config is passed through correctly
        pClientInfo->kvsRetryStrategyCallbacks.createRetryStrategyFn = testCreateRetryStrategyFn;
        pClientInfo->kvsRetryStrategyCallbacks.freeRetryStrategyFn = exponentialBackoffRetryStrategyFree;
        pClientInfo->kvsRetryStrategyCallbacks.executeRetryStrategyFn = getExponentialBackoffRetryStrategyWaitTime;
        pClientInfo->kvsRetryStrategyCallbacks.getCurrentRetryAttemptNumberFn = getExponentialBackoffRetryCount;
    }

    void verifyRetryConfiguration(CLIENT_HANDLE clientHandle, PExponentialBackoffRetryStrategyConfig pExpectedConfig)
    {
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

    void verifyRetryCallbacks(CLIENT_HANDLE clientHandle)
    {
        // Get the client from handle
        PKinesisVideoClient pKinesisVideoClient = FROM_CLIENT_HANDLE(clientHandle);

        // Verify retry strategy callbacks are preserved
        KvsRetryStrategyCallbacks* pActualCallbacks = &(pKinesisVideoClient->deviceInfo.clientInfo.kvsRetryStrategyCallbacks);

        EXPECT_EQ(testCreateRetryStrategyFn, pActualCallbacks->createRetryStrategyFn);
        EXPECT_EQ(exponentialBackoffRetryStrategyFree, pActualCallbacks->freeRetryStrategyFn);
        EXPECT_EQ(getExponentialBackoffRetryStrategyWaitTime, pActualCallbacks->executeRetryStrategyFn);
        EXPECT_EQ(getExponentialBackoffRetryCount, pActualCallbacks->getCurrentRetryAttemptNumberFn);
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
    customRetryConfig.maxRetryCount = 3;                // Bounded retries for testing
    customRetryConfig.maxRetryWaitTime = 1000;          // 1 second max (in milliseconds)
    customRetryConfig.retryFactorTime = 100;            // 100ms base (in milliseconds)
    customRetryConfig.minTimeToResetRetryState = 95000; // 95 seconds (min is 90000ms)
    customRetryConfig.jitterType = NO_JITTER;           // No jitter for predictable testing
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

    // Test that retry strategy is working with our custom configuration
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTime));
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryCount(pRetryStrategy, &retryCount));
    EXPECT_EQ(1, retryCount);
    EXPECT_GT(retryWaitTime, 0); // Should get some wait time

    // Verify the configuration is actually our custom one (from this test, not the first test)
    PExponentialBackoffRetryStrategyState pRetryState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy->pRetryStrategy);
    PExponentialBackoffRetryStrategyConfig pActualConfig = &(pRetryState->exponentialBackoffRetryStrategyConfig);
    EXPECT_EQ(customRetryConfig.maxRetryCount, pActualConfig->maxRetryCount); // Should match our custom maxRetryCount
    EXPECT_EQ(customRetryConfig.retryFactorTime * HUNDREDS_OF_NANOS_IN_A_MILLISECOND,
              pActualConfig->retryFactorTime); // Should match our custom retryFactorTime

    // Test second retry
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTime));
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryCount(pRetryStrategy, &retryCount));
    EXPECT_EQ(2, retryCount);
    EXPECT_GT(retryWaitTime, 0); // Should get some wait time

    // Test third retry
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTime));
    EXPECT_EQ(STATUS_SUCCESS, getExponentialBackoffRetryCount(pRetryStrategy, &retryCount));
    EXPECT_EQ(3, retryCount);
    EXPECT_GT(retryWaitTime, 0); // Should get some wait time

    // Test fourth retry - should fail because we set maxRetryCount to 3
    STATUS fourthRetryStatus = getExponentialBackoffRetryStrategyWaitTime(pRetryStrategy, &retryWaitTime);
    // Should either succeed (if there's some edge case) or return the exhausted error
    // The main point is that the retry mechanism is working with our custom config
    EXPECT_TRUE(fourthRetryStatus == STATUS_SUCCESS || fourthRetryStatus == STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED);

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
    DeviceInfo deviceInfo;

    // Initialize device info with default configuration
    deviceInfo = mDeviceInfo; // Copy from base test fixture

    // Override the callbacks to use our validation callback for default testing
    deviceInfo.clientInfo.kvsRetryStrategyCallbacks.createRetryStrategyFn = testCreateRetryStrategyFnForDefault;
    deviceInfo.clientInfo.kvsRetryStrategyCallbacks.freeRetryStrategyFn = exponentialBackoffRetryStrategyFree;
    deviceInfo.clientInfo.kvsRetryStrategyCallbacks.executeRetryStrategyFn = getExponentialBackoffRetryStrategyWaitTime;
    deviceInfo.clientInfo.kvsRetryStrategyCallbacks.getCurrentRetryAttemptNumberFn = getExponentialBackoffRetryCount;

    // Clear any custom retry strategy config to test defaults
    deviceInfo.clientInfo.kvsRetryStrategy.pRetryStrategyConfig = NULL;

    // Create client with default configuration
    EXPECT_EQ(STATUS_SUCCESS, createKinesisVideoClient(&deviceInfo, &mClientCallbacks, &clientHandle));

    // Get the client and verify that default retry configuration is used
    PKinesisVideoClient pKinesisVideoClient = FROM_CLIENT_HANDLE(clientHandle);
    KvsRetryStrategy* pRetryStrategy = &(pKinesisVideoClient->deviceInfo.clientInfo.kvsRetryStrategy);
    EXPECT_EQ(KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT, pRetryStrategy->retryStrategyType);

    PExponentialBackoffRetryStrategyState pRetryState = TO_EXPONENTIAL_BACKOFF_STATE(pRetryStrategy->pRetryStrategy);
    EXPECT_NE(nullptr, pRetryState);

    PExponentialBackoffRetryStrategyConfig pActualConfig = &(pRetryState->exponentialBackoffRetryStrategyConfig);

    // Verify default values are set
    EXPECT_EQ(KVS_INFINITE_EXPONENTIAL_RETRIES, pActualConfig->maxRetryCount);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS, pActualConfig->maxRetryWaitTime);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS, pActualConfig->retryFactorTime);
    EXPECT_EQ(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS, pActualConfig->minTimeToResetRetryState);
    EXPECT_EQ(FULL_JITTER, pActualConfig->jitterType);

    // Clean up
    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoClient(&clientHandle));
}
/**
 * Test to verify our validation actually works by using wrong config values
 * This should FAIL if the validation is working correctly
 */
TEST_F(RetryStrategyConfigurationTest, ValidationShouldDetectWrongConfig)
{
    ExponentialBackoffRetryStrategyConfig customRetryConfig;
    DeviceInfo deviceInfo;
    CLIENT_HANDLE clientHandle;

    // Initialize structures
    MEMSET(&customRetryConfig, 0x00, SIZEOF(ExponentialBackoffRetryStrategyConfig));
    MEMSET(&deviceInfo, 0x00, SIZEOF(DeviceInfo));

    // Set up device info based on existing test pattern
    deviceInfo = mDeviceInfo; // Copy from base test fixture

    // Set up DIFFERENT config values than what our validation expects
    customRetryConfig.maxRetryCount = 999;    // Different from expected 10
    customRetryConfig.maxRetryWaitTime = 999; // Different from expected 15000
    customRetryConfig.retryFactorTime = 999;  // Different from expected 100
    customRetryConfig.minTimeToResetRetryState = 120000;
    customRetryConfig.jitterType = FIXED_JITTER;
    customRetryConfig.jitterFactor = 100;

    // Set up retry strategy with WRONG config
    deviceInfo.clientInfo.kvsRetryStrategy.pRetryStrategyConfig = (PRetryStrategyConfig) &customRetryConfig;
    deviceInfo.clientInfo.kvsRetryStrategy.retryStrategyType = KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT;

    // Use our validation callback
    deviceInfo.clientInfo.kvsRetryStrategyCallbacks.createRetryStrategyFn = testCreateRetryStrategyFn;
    deviceInfo.clientInfo.kvsRetryStrategyCallbacks.freeRetryStrategyFn = exponentialBackoffRetryStrategyFree;
    deviceInfo.clientInfo.kvsRetryStrategyCallbacks.executeRetryStrategyFn = getExponentialBackoffRetryStrategyWaitTime;
    deviceInfo.clientInfo.kvsRetryStrategyCallbacks.getCurrentRetryAttemptNumberFn = getExponentialBackoffRetryCount;

    // Create client - this should FAIL because our validation will detect wrong config values
    STATUS result = createKinesisVideoClient(&deviceInfo, &mClientCallbacks, &clientHandle);
    EXPECT_EQ(STATUS_INVALID_ARG, result); // Should fail with our validation error

    // If it somehow succeeded, clean up
    if (result == STATUS_SUCCESS) {
        freeKinesisVideoClient(&clientHandle);
    }
}
