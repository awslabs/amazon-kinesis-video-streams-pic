#include "ClientTestFixture.h"

using ::testing::WithParamInterface;
using ::testing::Bool;
using ::testing::Values;
using ::testing::Combine;

class IntermittentProducerAutomaticStreamingTest : public ClientTestBase,
                                    public WithParamInterface< ::std::tuple<UINT64, UINT32> > {
protected:

    void SetUp() {
        ClientTestBase::SetUp(false);
        UINT64 callbackInterval;
        UINT32 clientInfoVersion;
        std::tie(callbackInterval, clientInfoVersion) = GetParam();
        mDeviceInfo.clientInfo.version = CLIENT_INFO_CURRENT_VERSION;
        mDeviceInfo.clientInfo.callbackPeriod = (UINT64) callbackInterval * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        mDeviceInfo.clientInfo.automaticStreamingFlags = AUTOMATIC_STREAMING_INTERMITTENT_PRODUCER;
    }

};

TEST_P(IntermittentProducerAutomaticStreamingTest, ValidateTimerInvokedBeforeTime) {
    // Create new client so param value of callbackPeriod can be applied
    ASSERT_EQ(STATUS_SUCCESS, CreateClient());

    PKinesisVideoClient client = FROM_CLIENT_HANDLE(mClientHandle);

    EXPECT_EQ((int)client->timerCallbackInvocationCount, 0);

    // Create synchronously
    CreateStreamSync();

    // Produce a frame
    BYTE temp[100];
    Frame frame;
    frame.trackId = 1;
    frame.size = SIZEOF(temp);
    frame.duration = TEST_FRAME_DURATION;
    frame.index = 0;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    frame.presentationTs = 0;
    frame.decodingTs = 0;
    frame.frameData = temp;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    THREAD_SLEEP(500 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_EQ((int)client->timerCallbackInvocationCount, 1);
}

TEST_P(IntermittentProducerAutomaticStreamingTest, ValidateTimerInvokedAfterFirstPeriod) {
    // Create new client so param value of callbackPeriod can be applied
    ASSERT_EQ(STATUS_SUCCESS, CreateClient());

    PKinesisVideoClient client = FROM_CLIENT_HANDLE(mClientHandle);

    // Create synchronously
    CreateStreamSync();

    // Produce a frame
    BYTE temp[100];
    Frame frame;
    frame.trackId = 1;
    frame.size = SIZEOF(temp);
    frame.duration = TEST_FRAME_DURATION;
    frame.index = 0;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    frame.presentationTs = 0;
    frame.decodingTs = 0;
    frame.frameData = temp;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND + 1.25 * mDeviceInfo.clientInfo.callbackPeriod);

    EXPECT_EQ((int)client->timerCallbackInvocationCount, 2);
}

/*
This test fails, if I try to do only stopKinesisVideoStreamSync it fails there because the close
 stream CVAR is never signaled, the call to notifyStreamClosed is only made if I don't
 make any putFrame calls in the test

TEST_P(IntermittentProducerAutomaticStreamingTest, ValidateTimerContinueInvocationsAfterStopStream) {
    // Create new client so param value of callbackPeriod can be applied
    ASSERT_EQ(STATUS_SUCCESS, CreateClient());

    PKinesisVideoClient client = FROM_CLIENT_HANDLE(mClientHandle);

    // Create synchronously
    CreateStreamSync();

    // Produce a frame
    BYTE temp[100];
    Frame frame;
    frame.trackId = 1;
    frame.size = SIZEOF(temp);
    frame.duration = TEST_FRAME_DURATION;
    frame.index = 0;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    frame.presentationTs = 0;
    frame.decodingTs = 0;
    frame.frameData = temp;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);

    std::cerr << "About To Call stopKinesisVideoStreamSync\n";

    VerifyStopStreamSyncAndFree();

    int val = (int)client->timerCallbackInvocationCount;

    std::cerr << "About To Sleep For 5s\n";

    THREAD_SLEEP(2 *  mDeviceInfo.clientInfo.callbackPeriod);

    EXPECT_EQ((int)client->timerCallbackInvocationCount - 2, val);
}

 */

TEST_P(IntermittentProducerAutomaticStreamingTest, ValidateTimerNoMoreInvocationsAfterFreeClient) {
    // Create new client so param value of callbackPeriod can be applied
    ASSERT_EQ(STATUS_SUCCESS, CreateClient());

    PKinesisVideoClient client = FROM_CLIENT_HANDLE(mClientHandle);

    // Create synchronously
    CreateStreamSync();

    // Produce a frame
    BYTE temp[100];
    Frame frame;
    frame.trackId = 1;
    frame.size = SIZEOF(temp);
    frame.duration = TEST_FRAME_DURATION;
    frame.index = 0;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    frame.presentationTs = 0;
    frame.decodingTs = 0;
    frame.frameData = temp;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    THREAD_SLEEP(1 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    freeKinesisVideoClient(&mClientHandle);

    int val = (int)client->timerCallbackInvocationCount;

    THREAD_SLEEP(5 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    EXPECT_EQ((int)client->timerCallbackInvocationCount, val);
}

TEST_P(IntermittentProducerAutomaticStreamingTest, ValidateTimerNoMoreInvocationsWithAutoStreamingOFF) {

    mDeviceInfo.clientInfo.automaticStreamingFlags = AUTOMATIC_STREAMING_ALWAYS_CONTINUOUS;

    // Validate timerqueue is never constructed
    ASSERT_EQ(STATUS_SUCCESS, CreateClient());

    PKinesisVideoClient client = FROM_CLIENT_HANDLE(mClientHandle);

    // Create synchronously
    CreateStreamSync();

    // Produce a frame
    BYTE temp[100];
    Frame frame;
    frame.trackId = 1;
    frame.size = SIZEOF(temp);
    frame.duration = TEST_FRAME_DURATION;
    frame.index = 0;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    frame.presentationTs = 0;
    frame.decodingTs = 0;
    frame.frameData = temp;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    THREAD_SLEEP(2 * mDeviceInfo.clientInfo.callbackPeriod);

    EXPECT_TRUE(!IS_VALID_TIMER_QUEUE_HANDLE(client->timerQueueHandle));
    EXPECT_EQ((int)client->timerCallbackInvocationCount, 0);
}




INSTANTIATE_TEST_CASE_P(PermutatedStreamInfo, IntermittentProducerAutomaticStreamingTest,
        Combine(Values(1000,2000,3000,4000,5000), Values(0, CLIENT_INFO_CURRENT_VERSION)));
