#include "ClientTestFixture.h"

class StreamFragmentAggregationTest : public ClientTestBase {
};

VOID checkContentViewAggregated(STREAM_HANDLE streamHandle)
{
    // Validate that the last few view items are single aggregated fragments
    PKinesisVideoStream pKinesisVideoStream = FROM_STREAM_HANDLE(streamHandle);

    UINT64 i, index;
    PViewItem pViewItem;

    EXPECT_EQ(STATUS_SUCCESS, contentViewGetHead(pKinesisVideoStream->pView, &pViewItem));
    index = pViewItem->index;

    for (i = 0; i < 3; i++) {
        EXPECT_EQ(STATUS_SUCCESS, contentViewGetItemAt(pKinesisVideoStream->pView, index - i, &pViewItem));
        EXPECT_TRUE(CHECK_ITEM_FRAGMENT_START(pViewItem->flags));
        EXPECT_LT(50000, pViewItem->length);
    }
}

VOID checkContentViewNotAggregated(STREAM_HANDLE streamHandle)
{
    PKinesisVideoStream pKinesisVideoStream = FROM_STREAM_HANDLE(streamHandle);

    UINT64 i, index;
    PViewItem pViewItem;

    EXPECT_EQ(STATUS_SUCCESS, contentViewGetHead(pKinesisVideoStream->pView, &pViewItem));
    index = pViewItem->index;

    for (i = 0; i < 10; i++) {
        EXPECT_EQ(STATUS_SUCCESS, contentViewGetItemAt(pKinesisVideoStream->pView, index - i, &pViewItem));
        EXPECT_FALSE(CHECK_ITEM_FRAGMENT_START(pViewItem->flags));
        EXPECT_GE(50000, pViewItem->length);
    }
}

TEST_F(StreamFragmentAggregationTest, fragmentAggregationModeSelection_False)
{
    ReadyStream();

    EXPECT_FALSE(FROM_STREAM_HANDLE(mStreamHandle)->fragmentAggregator.aggregateFragment);
}

TEST_F(StreamFragmentAggregationTest, fragmentAggregationModeSelection_True)
{
    mDeviceInfo.storageInfo.storageType = DEVICE_STORAGE_TYPE_HYBRID_FILE;

    CreateScenarioTestClient(MIN_STORAGE_SIZE_FOR_FRAGMENT_ACCUMULATOR + 1);

    mStreamInfo.streamCaps.bufferDuration = MIN_CONTENT_DURATION_FOR_FRAGMENT_ACCUMULATOR + 1;
    ReadyStream();

    EXPECT_TRUE(FROM_STREAM_HANDLE(mStreamHandle)->fragmentAggregator.aggregateFragment);
}

TEST_F(StreamFragmentAggregationTest, fragmentAggregation_SpillOverToFileStorage)
{
    UINT64 i, frameCount, fileSize;
    BOOL exist;
    CHAR filePath[MAX_PATH_LEN];

    mSubmitServiceCallResultMode = STOP_AT_PUT_STREAM;
    mDeviceInfo.storageInfo.storageType = DEVICE_STORAGE_TYPE_HYBRID_FILE;

    CreateScenarioTestClient(MIN_STORAGE_SIZE_FOR_FRAGMENT_ACCUMULATOR + 1);

    mStreamInfo.streamCaps.bufferDuration = 72 * HUNDREDS_OF_NANOS_IN_AN_HOUR;
    EXPECT_EQ(STATUS_SUCCESS, createKinesisVideoStream(mClientHandle, &mStreamInfo, &mStreamHandle));

    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    EXPECT_TRUE(FROM_STREAM_HANDLE(mStreamHandle)->fragmentAggregator.aggregateFragment);

    // Frame size is 50K. Need to calculate how many frames can cause spill over which is 20% in the test
    frameCount = MIN_STORAGE_SIZE_FOR_FRAGMENT_ACCUMULATOR * 0.2 / 50000;

    for (i = 0; i < frameCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE, TEST_VIDEO_TRACK_ID)) << "frameCount " << i;
    }

    STRCPY(filePath, "1.hfh");
    EXPECT_EQ(STATUS_SUCCESS, fileExists(filePath, &exist));
    EXPECT_TRUE(exist);
    EXPECT_EQ(STATUS_SUCCESS, getFileLength(filePath, &fileSize));

    // Accounting for the MKV packaging and allocation book keeping
    EXPECT_EQ(50029, fileSize);
}

TEST_F(StreamFragmentAggregationTest, fragmentAggregation_SimpleAggregationNoSpillOver)
{
    UINT64 i, frameCount;
    BOOL exist;
    CHAR filePath[MAX_PATH_LEN];
    BYTE buffer[10000];
    UINT32 readSize;

    mSubmitServiceCallResultMode = STOP_AT_PUT_STREAM;
    mDeviceInfo.storageInfo.storageType = DEVICE_STORAGE_TYPE_HYBRID_FILE;
    mDeviceInfo.storageInfo.spillRatio = 80;
    mStreamInfo.streamCaps.storePressurePolicy = CONTENT_STORE_PRESSURE_POLICY_DROP_TAIL_ITEM;

    CreateScenarioTestClient(MIN_STORAGE_SIZE_FOR_FRAGMENT_ACCUMULATOR + 1);

    mStreamInfo.streamCaps.bufferDuration = 72 * HUNDREDS_OF_NANOS_IN_AN_HOUR;
    EXPECT_EQ(STATUS_SUCCESS, createKinesisVideoStream(mClientHandle, &mStreamInfo, &mStreamHandle));

    mMockProducerConfig.mFrameSizeByte = 4500;
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    EXPECT_TRUE(FROM_STREAM_HANDLE(mStreamHandle)->fragmentAggregator.aggregateFragment);

    // Frame duration is 50ms by default test config.
    // Need to calculate how many frames can cause the fragmentation to start after
    // MIN_CONTENT_DURATION_FOR_FRAGMENT_ACCUMULATOR buffer accumulation
    frameCount = MIN_CONTENT_DURATION_FOR_FRAGMENT_ACCUMULATOR / (50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    for (i = 0; i < frameCount + 3 * mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE, TEST_VIDEO_TRACK_ID)) << "frameCount " << i;
    }

    STRCPY(filePath, "1.hfh");
    EXPECT_EQ(STATUS_SUCCESS, fileExists(filePath, &exist));
    EXPECT_FALSE(exist);

    checkContentViewAggregated(mStreamHandle);

    // Read up data in order to check the recovery
    while (STATUS_SUCCEEDED(getKinesisVideoStreamData(mStreamHandle, 0, buffer, SIZEOF(buffer), &readSize)));

    for (i = 0; i < 10 * mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE, TEST_VIDEO_TRACK_ID)) << "frameCount " << i;
    }

    checkContentViewNotAggregated(mStreamHandle);
}

TEST_F(StreamFragmentAggregationTest, fragmentAggregation_SimpleAggregationWithSpillOver)
{
    UINT64 i, frameCount, fileSize;
    BOOL exist;
    CHAR filePath[MAX_PATH_LEN];
    BYTE buffer[10000];
    UINT32 readSize;

    mSubmitServiceCallResultMode = STOP_AT_PUT_STREAM;
    mDeviceInfo.storageInfo.storageType = DEVICE_STORAGE_TYPE_HYBRID_FILE;
    mStreamInfo.streamCaps.storePressurePolicy = CONTENT_STORE_PRESSURE_POLICY_DROP_TAIL_ITEM;

    CreateScenarioTestClient(MIN_STORAGE_SIZE_FOR_FRAGMENT_ACCUMULATOR + 1);

    mStreamInfo.streamCaps.bufferDuration = 72 * HUNDREDS_OF_NANOS_IN_AN_HOUR;
    EXPECT_EQ(STATUS_SUCCESS, createKinesisVideoStream(mClientHandle, &mStreamInfo, &mStreamHandle));

    mMockProducerConfig.mFrameSizeByte = 4500;
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    EXPECT_TRUE(FROM_STREAM_HANDLE(mStreamHandle)->fragmentAggregator.aggregateFragment);

    // Frame duration is 50ms by default test config.
    // Need to calculate how many frames can cause the fragmentation to start after
    // MIN_CONTENT_DURATION_FOR_FRAGMENT_ACCUMULATOR buffer accumulation
    frameCount = MIN_CONTENT_DURATION_FOR_FRAGMENT_ACCUMULATOR / (50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    for (i = 0; i < frameCount + 3 * mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE, TEST_VIDEO_TRACK_ID)) << "frameCount " << i;
    }

    STRCPY(filePath, "253.hfh");
    EXPECT_EQ(STATUS_SUCCESS, fileExists(filePath, &exist));
    EXPECT_TRUE(exist);
    EXPECT_EQ(STATUS_SUCCESS, getFileLength(filePath, &fileSize));

    // Accounting for aggregation, MKV packaging and allocation book keeping
    EXPECT_EQ(90636, fileSize);

    checkContentViewAggregated(mStreamHandle);

    // Read up data in order to check the recovery
    while (STATUS_SUCCEEDED(getKinesisVideoStreamData(mStreamHandle, 0, buffer, SIZEOF(buffer), &readSize)));

    for (i = 0; i < 10 * mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE, TEST_VIDEO_TRACK_ID)) << "frameCount " << i;
    }

    checkContentViewNotAggregated(mStreamHandle);
}
