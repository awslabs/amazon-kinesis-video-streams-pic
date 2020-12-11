#include "ClientTestFixture.h"

using ::testing::WithParamInterface;
using ::testing::Bool;
using ::testing::Values;
using ::testing::Combine;

class IntermittentProducerAutomaticStreamingTest : public ClientTestBase,
                                    public WithParamInterface< ::std::tuple<UINT64, UINT32> > {
protected:

    void SetUp() {
        ClientTestBase::SetUpWithoutClientCreation();
        UINT64 callbackInterval;
        UINT32 clientInfoVersion;
        std::tie(callbackInterval, clientInfoVersion) = GetParam();
        mDeviceInfo.clientInfo.version = CLIENT_INFO_CURRENT_VERSION;
        mDeviceInfo.clientInfo.reservedCallbackPeriod = (UINT64) callbackInterval * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
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

    THREAD_SLEEP(1.2 * INTERMITTENT_PRODUCER_TIMER_START_DELAY);

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

    THREAD_SLEEP(INTERMITTENT_PRODUCER_TIMER_START_DELAY + 1.20 * mDeviceInfo.clientInfo.reservedCallbackPeriod);

    EXPECT_EQ((int)client->timerCallbackInvocationCount, 2);
}

/*
This test fails, if I try to do only stopKinesisVideoStreamSync it fails there because the close
 stream CVAR is never signaled, the call to notifyStreamClosed is only made if I don't
 make any putFrame calls in the test
*/

TEST_P(IntermittentProducerAutomaticStreamingTest, ValidateTimerContinueInvocationsAfterStopStream) {
    // Create new client so param value of callbackPeriod can be applied
    ASSERT_EQ(STATUS_SUCCESS, CreateClient());

    PKinesisVideoClient client = FROM_CLIENT_HANDLE(mClientHandle);
    // Create synchronously
    CreateStreamSync();

    mStreamUploadHandle = 0;

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

    EXPECT_EQ(STATUS_SUCCESS, notifyStreamClosed(FROM_STREAM_HANDLE(mStreamHandle), mStreamUploadHandle));

    THREAD_SLEEP(2 *  HUNDREDS_OF_NANOS_IN_A_SECOND);

    EXPECT_EQ(STATUS_SUCCESS, stopKinesisVideoStreamSync(mStreamHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));

    int beforeCount = (int)client->timerCallbackInvocationCount;
    int m = 3;

    THREAD_SLEEP(m * mDeviceInfo.clientInfo.reservedCallbackPeriod);

    int afterCount = (int)client->timerCallbackInvocationCount;
    // make sure call back is still firing after this stream is closed out
    EXPECT_TRUE(afterCount > beforeCount);

    // make sure call back isn't over-firing
    EXPECT_TRUE(afterCount <= beforeCount + m + 1);
}



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

    THREAD_SLEEP(INTERMITTENT_PRODUCER_TIMER_START_DELAY + 5 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    EXPECT_EQ(val, (int)client->timerCallbackInvocationCount);
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

    THREAD_SLEEP(INTERMITTENT_PRODUCER_TIMER_START_DELAY + 2 * mDeviceInfo.clientInfo.reservedCallbackPeriod);

    EXPECT_TRUE(!IS_VALID_TIMER_QUEUE_HANDLE(client->timerQueueHandle));
    EXPECT_EQ(0, (int)client->timerCallbackInvocationCount);
}

INSTANTIATE_TEST_CASE_P(PermutatedStreamInfo, IntermittentProducerAutomaticStreamingTest,
        Combine(Values(1000,2000,3000,4000,5000), Values(0, CLIENT_INFO_CURRENT_VERSION)));
