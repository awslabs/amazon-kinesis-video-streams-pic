#include "ClientTestFixture.h"

class InputValidatorTest : public ClientTestBase {
  public:
    void setupTestClientInfo(PClientInfo pClientInfo, PExponentialBackoffRetryStrategyConfig pRetryConfig) {
        // Set up basic client info based on existing test patterns
        *pClientInfo = mDeviceInfo.clientInfo; // Copy from base test fixture

        // Set up custom retry strategy configuration with valid values (in milliseconds)
        pRetryConfig->maxRetryCount = 5;
        pRetryConfig->maxRetryWaitTime = 20000; // 20 seconds (min is 10000ms)
        pRetryConfig->retryFactorTime = 300; // 300ms (min is 50ms)
        pRetryConfig->minTimeToResetRetryState = 100000; // 100 seconds (min is 90000ms)
        pRetryConfig->jitterType = FIXED_JITTER;
        pRetryConfig->jitterFactor = 50; // Min is 50

        // Set up retry strategy
        pClientInfo->kvsRetryStrategy.pRetryStrategyConfig = (PRetryStrategyConfig) pRetryConfig;
        pClientInfo->kvsRetryStrategy.retryStrategyType = KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT;
        pClientInfo->kvsRetryStrategy.pRetryStrategy = nullptr; // Will be set during creation

        // Set up retry strategy callbacks
        pClientInfo->kvsRetryStrategyCallbacks.createRetryStrategyFn = createRetryStrategyFn;
        pClientInfo->kvsRetryStrategyCallbacks.freeRetryStrategyFn = freeRetryStrategyFn;
        pClientInfo->kvsRetryStrategyCallbacks.executeRetryStrategyFn = executeRetryStrategyFn;
        pClientInfo->kvsRetryStrategyCallbacks.getCurrentRetryAttemptNumberFn = getCurrentRetryAttemptNumberFn;
    }

    void verifyClientInfoCopy(PClientInfo pOriginal, PClientInfo pCopied, 
                              PExponentialBackoffRetryStrategyConfig pExpectedRetryConfig) {
        // Verify basic fields are copied
        EXPECT_EQ(pOriginal->version, pCopied->version);
        EXPECT_EQ(pOriginal->createClientTimeout, pCopied->createClientTimeout);
        EXPECT_EQ(pOriginal->createStreamTimeout, pCopied->createStreamTimeout);
        EXPECT_EQ(pOriginal->stopStreamTimeout, pCopied->stopStreamTimeout);
        EXPECT_EQ(pOriginal->loggerLogLevel, pCopied->loggerLogLevel);

        // Verify retry strategy configuration is copied (this was the bug!)
        EXPECT_EQ(pOriginal->kvsRetryStrategy.retryStrategyType, pCopied->kvsRetryStrategy.retryStrategyType);
        EXPECT_EQ(pOriginal->kvsRetryStrategy.pRetryStrategyConfig, pCopied->kvsRetryStrategy.pRetryStrategyConfig);

        // Verify the actual config values are accessible through the copied pointer
        // Note: Internal values are converted to hundreds of nanos, so we need to account for that
        PExponentialBackoffRetryStrategyConfig pCopiedConfig = 
            (PExponentialBackoffRetryStrategyConfig) pCopied->kvsRetryStrategy.pRetryStrategyConfig;
        EXPECT_NE(nullptr, pCopiedConfig);
        EXPECT_EQ(pExpectedRetryConfig->maxRetryCount, pCopiedConfig->maxRetryCount);
        EXPECT_EQ(pExpectedRetryConfig->maxRetryWaitTime, pCopiedConfig->maxRetryWaitTime);
        EXPECT_EQ(pExpectedRetryConfig->retryFactorTime, pCopiedConfig->retryFactorTime);
        EXPECT_EQ(pExpectedRetryConfig->minTimeToResetRetryState, pCopiedConfig->minTimeToResetRetryState);
        EXPECT_EQ(pExpectedRetryConfig->jitterType, pCopiedConfig->jitterType);
        EXPECT_EQ(pExpectedRetryConfig->jitterFactor, pCopiedConfig->jitterFactor);

        // Verify retry strategy callbacks are copied (this was also part of the bug!)
        EXPECT_EQ(pOriginal->kvsRetryStrategyCallbacks.createRetryStrategyFn, 
                  pCopied->kvsRetryStrategyCallbacks.createRetryStrategyFn);
        EXPECT_EQ(pOriginal->kvsRetryStrategyCallbacks.freeRetryStrategyFn, 
                  pCopied->kvsRetryStrategyCallbacks.freeRetryStrategyFn);
        EXPECT_EQ(pOriginal->kvsRetryStrategyCallbacks.executeRetryStrategyFn, 
                  pCopied->kvsRetryStrategyCallbacks.executeRetryStrategyFn);
        EXPECT_EQ(pOriginal->kvsRetryStrategyCallbacks.getCurrentRetryAttemptNumberFn, 
                  pCopied->kvsRetryStrategyCallbacks.getCurrentRetryAttemptNumberFn);
    }
};

/**
 * Test that fixupClientInfo properly copies retry strategy configuration
 * This is the core unit test that would have caught the original bug.
 * Before the fix, this test would FAIL because the MEMCPY calls were missing.
 */
TEST_F(InputValidatorTest, fixupClientInfoCopiesRetryStrategyConfiguration)
{
    ClientInfo originalClientInfo, fixedUpClientInfo;
    ExponentialBackoffRetryStrategyConfig retryConfig;

    // Initialize structures
    MEMSET(&originalClientInfo, 0x00, SIZEOF(ClientInfo));
    MEMSET(&fixedUpClientInfo, 0x00, SIZEOF(ClientInfo));
    MEMSET(&retryConfig, 0x00, SIZEOF(ExponentialBackoffRetryStrategyConfig));

    // Set up original client info with custom retry configuration
    setupTestClientInfo(&originalClientInfo, &retryConfig);

    // Set up the "fixed up" client info with a different version to trigger the copy path
    fixedUpClientInfo.version = CLIENT_INFO_CURRENT_VERSION;

    // Call fixupClientInfo - this should copy the retry strategy fields
    fixupClientInfo(&fixedUpClientInfo, &originalClientInfo);

    // Verify that retry strategy configuration and callbacks were properly copied
    verifyClientInfoCopy(&originalClientInfo, &fixedUpClientInfo, &retryConfig);
}

/**
 * Test that fixupClientInfo handles NULL retry strategy config gracefully
 */
TEST_F(InputValidatorTest, fixupClientInfoHandlesNullRetryStrategyConfig)
{
    ClientInfo originalClientInfo, fixedUpClientInfo;

    // Initialize structures
    MEMSET(&originalClientInfo, 0x00, SIZEOF(ClientInfo));
    MEMSET(&fixedUpClientInfo, 0x00, SIZEOF(ClientInfo));

    // Set up original client info WITHOUT retry configuration
    originalClientInfo = mDeviceInfo.clientInfo; // Copy from base test fixture

    // Leave retry strategy fields as NULL/default
    originalClientInfo.kvsRetryStrategy.pRetryStrategyConfig = nullptr;
    originalClientInfo.kvsRetryStrategy.retryStrategyType = KVS_RETRY_STRATEGY_DISABLED;

    // Set up the "fixed up" client info
    fixedUpClientInfo.version = CLIENT_INFO_CURRENT_VERSION;

    // Call fixupClientInfo - this should handle NULL config gracefully
    fixupClientInfo(&fixedUpClientInfo, &originalClientInfo);

    // Verify that NULL config is preserved
    EXPECT_EQ(nullptr, fixedUpClientInfo.kvsRetryStrategy.pRetryStrategyConfig);
    EXPECT_EQ(KVS_RETRY_STRATEGY_DISABLED, fixedUpClientInfo.kvsRetryStrategy.retryStrategyType);
}

/**
 * Test that fixupClientInfo preserves retry strategy configuration across version upgrades
 * This tests the specific code path where version differences trigger the copy logic
 */
TEST_F(InputValidatorTest, fixupClientInfoPreservesRetryConfigAcrossVersions)
{
    ClientInfo originalClientInfo, fixedUpClientInfo;
    ExponentialBackoffRetryStrategyConfig retryConfig;

    // Initialize structures
    MEMSET(&originalClientInfo, 0x00, SIZEOF(ClientInfo));
    MEMSET(&fixedUpClientInfo, 0x00, SIZEOF(ClientInfo));
    MEMSET(&retryConfig, 0x00, SIZEOF(ExponentialBackoffRetryStrategyConfig));

    // Set up original client info with custom retry configuration
    setupTestClientInfo(&originalClientInfo, &retryConfig);

    // Simulate an older version client info that needs to be "fixed up"
    fixedUpClientInfo.version = 0; // Older version
    fixedUpClientInfo.createClientTimeout = 999; // Different value to verify it gets overwritten

    // Call fixupClientInfo - this should copy from original to fixed up
    fixupClientInfo(&fixedUpClientInfo, &originalClientInfo);

    // Verify that the version was updated and retry config was preserved
    EXPECT_EQ(CLIENT_INFO_CURRENT_VERSION, fixedUpClientInfo.version);
    verifyClientInfoCopy(&originalClientInfo, &fixedUpClientInfo, &retryConfig);
}

/**
 * Test the specific case that was broken: V2 to current version upgrade
 * This tests the exact code path that had the missing MEMCPY calls
 */
TEST_F(InputValidatorTest, fixupClientInfoV2ToCurrentVersionUpgrade)
{
    ClientInfo originalClientInfo, fixedUpClientInfo;
    ExponentialBackoffRetryStrategyConfig retryConfig;

    // Initialize structures
    MEMSET(&originalClientInfo, 0x00, SIZEOF(ClientInfo));
    MEMSET(&fixedUpClientInfo, 0x00, SIZEOF(ClientInfo));
    MEMSET(&retryConfig, 0x00, SIZEOF(ExponentialBackoffRetryStrategyConfig));

    // Set up original client info with custom retry configuration
    setupTestClientInfo(&originalClientInfo, &retryConfig);
    
    // Set the specific value we want to test in the ORIGINAL client info
    originalClientInfo.reservedCallbackPeriod = 999;

    // Set up fixed up client info as V2 (the version that was missing the copy)
    fixedUpClientInfo.version = 2;
    fixedUpClientInfo.automaticStreamingFlags = AUTOMATIC_STREAMING_INTERMITTENT_PRODUCER;
    fixedUpClientInfo.reservedCallbackPeriod = 888; // Different value to verify it gets overwritten

    // Call fixupClientInfo - this triggers the V2 -> current version path
    fixupClientInfo(&fixedUpClientInfo, &originalClientInfo);

    // Verify that V2 fields were copied from original AND retry config was copied
    EXPECT_EQ(AUTOMATIC_STREAMING_INTERMITTENT_PRODUCER, fixedUpClientInfo.automaticStreamingFlags);
    EXPECT_EQ(999, fixedUpClientInfo.reservedCallbackPeriod); // Should be copied from original
    
    // Most importantly, verify retry config was copied (this was the bug!)
    verifyClientInfoCopy(&originalClientInfo, &fixedUpClientInfo, &retryConfig);
}
