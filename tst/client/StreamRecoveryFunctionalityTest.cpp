#include "ClientTestFixture.h"

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;
using ::testing::WithParamInterface;

class StreamRecoveryFunctionalityTest : public ClientTestBase, public WithParamInterface< ::std::tuple<STREAMING_TYPE, uint64_t, bool, uint64_t> > {
  protected:
    void SetUp()
    {
        ClientTestBase::SetUp();

        STREAMING_TYPE streamingType;
        bool enableAck;
        uint64_t retention, replayDuration;
        std::tie(streamingType, retention, enableAck, replayDuration) = GetParam();
        mStreamInfo.retention = (UINT64) retention;
        mStreamInfo.streamCaps.streamingType = streamingType;
        mStreamInfo.streamCaps.fragmentAcks = enableAck;
        mStreamInfo.streamCaps.replayDuration = (UINT64) replayDuration;
    }

    // Drives the stream until a handle has transmitted, then drops that handle so a rollback and MKV
    // stream-start fix-up are owed to whichever session transmits next.
    void transmitThenDropFirstHandle(MockProducer& mockProducer)
    {
        std::vector<UPLOAD_HANDLE> uploadHandles;
        MockConsumer* pMockConsumer = NULL;
        BOOL gotStreamData = FALSE;
        UINT64 currentTime;

        for (UINT32 i = 0; i < 2 * mMockProducerConfig.mKeyFrameInterval; i++) {
            EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
        }

        mStreamingSession.getActiveUploadHandles(uploadHandles);
        ASSERT_FALSE(uploadHandles.empty());
        pMockConsumer = mStreamingSession.getConsumer(uploadHandles[0]);
        ASSERT_TRUE(pMockConsumer != NULL);

        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);
        EXPECT_EQ(STATUS_SUCCESS, pMockConsumer->timedGetStreamData(currentTime, &gotStreamData));
        ASSERT_TRUE(gotStreamData) << "first handle did not transmit, so no rollback would be owed";

        EXPECT_EQ(STATUS_SUCCESS, pMockConsumer->submitConnectionError(SERVICE_CALL_RESULT_OK));
    }

    // Returns the upload handle PIC is currently serving, or INVALID_UPLOAD_HANDLE_VALUE if there is none. With two
    // live handles, as after a token rotation, PIC serves the older one, so the highest-numbered handle is not
    // necessarily the one receiving data.
    UPLOAD_HANDLE activeUploadHandle()
    {
        PUploadHandleInfo pUploadHandleInfo = getStreamUploadInfoWithState(FROM_STREAM_HANDLE(mStreamHandle), UPLOAD_HANDLE_STATE_ACTIVE);

        return pUploadHandleInfo == NULL ? INVALID_UPLOAD_HANDLE_VALUE : pUploadHandleInfo->handle;
    }

    // TRUE if the buffer begins with the EBML header magic, which is what makes a PutMedia body parseable.
    static BOOL beginsWithEbmlHeader(PBYTE pData, UINT32 size)
    {
        return (BOOL) (size >= 4 && pData[0] == 0x1A && pData[1] == 0x45 && pData[2] == 0xDF && pData[3] == 0xA3);
    }

    //
    // Streams for the given number of seconds of mock time, putting one second of video and reading once per
    // second, and reports through pFirstBodyHadHeader whether the first body began with an EBML header. With the
    // small frames these tests use, one read drains what was put, so the session keeps up with the camera.
    // Returns the handle that was transmitting when the period ended.
    //
    UPLOAD_HANDLE streamForSeconds(MockProducer& mockProducer, UINT64 seconds, PBOOL pFirstBodyHadHeader)
    {
        MockConsumer* pMockConsumer = NULL;
        UPLOAD_HANDLE uploadHandle;
        UINT64 currentTime;
        UINT32 retrievedSize = 0;
        BOOL gotStreamData = FALSE, submittedAck = FALSE, inspectedFirstBody = FALSE;

        *pFirstBodyHadHeader = FALSE;

        for (UINT64 second = 0; second < seconds; second++) {
            for (UINT32 i = 0; i < mMockProducerConfig.mFps; i++) {
                EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
            }

            uploadHandle = activeUploadHandle();
            if (!IS_VALID_UPLOAD_HANDLE(uploadHandle)) {
                break;
            }

            pMockConsumer = mStreamingSession.getConsumer(uploadHandle);
            if (pMockConsumer == NULL) {
                break;
            }

            currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);
            pMockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedSize);
            EXPECT_EQ(STATUS_SUCCESS, pMockConsumer->timedSubmitNormalAck(currentTime, &submittedAck));

            if (gotStreamData && !inspectedFirstBody && retrievedSize >= 4) {
                inspectedFirstBody = TRUE;
                *pFirstBodyHadHeader = beginsWithEbmlHeader(pMockConsumer->mDataBuffer, retrievedSize);
            }

            incrementTestTimeVal(HUNDREDS_OF_NANOS_IN_A_SECOND);
        }

        EXPECT_TRUE(inspectedFirstBody) << "the stream never transmitted before the outage";

        return activeUploadHandle();
    }

    //
    // Holds the link down for the given number of seconds, spread across count reconnect attempts. Frames keep
    // arriving at the camera's rate throughout, because a camera does not stop when the network does. Each attempt
    // is never asked for data and dies with the transport timeout an outage produces.
    //
    void holdOutage(MockProducer& mockProducer, UINT32 count, UINT64 seconds, PKinesisVideoStream pKinesisVideoStream, BOOL assertPreserved)
    {
        MockConsumer* pMockConsumer = NULL;
        UPLOAD_HANDLE idleHandle;
        UINT64 gapPerAttempt = (seconds * HUNDREDS_OF_NANOS_IN_A_SECOND) / count;
        UINT64 framesPerAttempt = gapPerAttempt * mMockProducerConfig.mFps / HUNDREDS_OF_NANOS_IN_A_SECOND;

        for (UINT32 i = 0; i < count; i++) {
            // Frames before the lookup: with an empty view, PIC only starts the next attempt once there is
            // something to send.
            for (UINT64 frame = 0; frame < framesPerAttempt; frame++) {
                EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
            }

            incrementTestTimeVal(gapPerAttempt);

            idleHandle = activeUploadHandle();
            ASSERT_TRUE(IS_VALID_UPLOAD_HANDLE(idleHandle)) << "no reconnect attempt was spawned during the outage, iteration " << i;

            pMockConsumer = mStreamingSession.getConsumer(idleHandle);
            ASSERT_TRUE(pMockConsumer != NULL);

            // Deliberately no getStreamData call: this attempt never transmits a byte.
            EXPECT_EQ(STATUS_SUCCESS, pMockConsumer->submitConnectionError(SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT));

            if (assertPreserved) {
                EXPECT_EQ(UPLOAD_CONNECTION_STATE_IN_USE, pKinesisVideoStream->connectionState)
                    << "reconnect attempt " << i << " (handle " << idleHandle << ") transmitted nothing, so it must not discard the "
                    << "header fix-up owed by the session that died at the start of the outage";
            }
        }
    }

    //
    // Steps for driving the outage, shared by the outage test and its control. The session keeps up with the camera
    // for longer than the replay window, the link is cut while video is still queued, and five reconnect attempts
    // die across the outage without sending a byte. Returns the session that transmitted before the outage.
    //
    void driveOutage(MockProducer& mockProducer, PKinesisVideoStream pKinesisVideoStream, BOOL assertPreserved, PUPLOAD_HANDLE pPreOutageHandle)
    {
        MockConsumer* pMockConsumer = NULL;
        BOOL firstBodyHadHeader = FALSE;

        // 1. Steady state, run past the replay window so the rollback lands on a mid-stream key frame, rather than on the stream's first item.
        *pPreOutageHandle =
            streamForSeconds(mockProducer, mStreamInfo.streamCaps.replayDuration / HUNDREDS_OF_NANOS_IN_A_SECOND + 2, &firstBodyHadHeader);
        ASSERT_TRUE(IS_VALID_UPLOAD_HANDLE(*pPreOutageHandle)) << "no session transmitted before the outage";
        EXPECT_TRUE(firstBodyHadHeader) << "the first body of the stream must begin with an EBML header";
        ASSERT_GT(pKinesisVideoStream->curViewItem.viewItem.ackTimestamp, mStreamInfo.streamCaps.replayDuration)
            << "the session did not get past the replay window, so the recovery would replay from the stream's first item";

        // The camera keeps producing as the link fails, so the session dies with video it never sent. Had the view
        // been empty, the timeout would make the next key frame start a new stream with its own header, hiding
        // whether the fix-up supplied one (condition under test).
        for (UINT32 i = 0; i < mMockProducerConfig.mFps; i++) {
            EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
        }

        incrementTestTimeVal(HUNDREDS_OF_NANOS_IN_A_SECOND);

        // 2. The link is cut on the transmitting session, so a rollback and a header fix-up are now owed to
        //    whichever session transmits next.
        pMockConsumer = mStreamingSession.getConsumer(*pPreOutageHandle);
        ASSERT_TRUE(pMockConsumer != NULL);
        EXPECT_EQ(STATUS_SUCCESS, pMockConsumer->submitConnectionError(SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT));
        ASSERT_EQ(UPLOAD_CONNECTION_STATE_IN_USE, pKinesisVideoStream->connectionState)
            << "a session that transmitted and then died must leave a rollback and header fix-up pending";

        // 3. The outage, five seconds across five attempts. Attempts are spawned and die having sent nothing, and
        //    none may discard what the pre-outage session left owed.
        holdOutage(mockProducer, 5, 5, pKinesisVideoStream, assertPreserved);
    }

    //
    // Step 4: the link returns. Puts one more second of video and reads the first body of the session PIC now
    // serves, which must not be the one that died before the outage. The read goes straight to PIC rather than
    // through the mock consumer, whose ACK bookkeeping expects every session to begin at a stream start, which
    // the control deliberately breaks.
    //
    void readRecoveryBody(MockProducer& mockProducer, UPLOAD_HANDLE preOutageHandle, MockConsumer** ppMockConsumer, PUINT32 pRetrievedSize)
    {
        UPLOAD_HANDLE recoveryHandle;
        STATUS retStatus;

        for (UINT32 i = 0; i < mMockProducerConfig.mFps; i++) {
            EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
        }

        recoveryHandle = activeUploadHandle();
        ASSERT_TRUE(IS_VALID_UPLOAD_HANDLE(recoveryHandle)) << "no session available once the link returned";
        ASSERT_NE(preOutageHandle, recoveryHandle) << "the outage did not replace the session";

        *ppMockConsumer = mStreamingSession.getConsumer(recoveryHandle);
        ASSERT_TRUE(*ppMockConsumer != NULL);

        // A session that reaches a stream start, such as one a token rotation adds, ends there and still returns
        // what it read up to it.
        *pRetrievedSize = 0;
        retStatus = getKinesisVideoStreamData(mStreamHandle, recoveryHandle, (*ppMockConsumer)->mDataBuffer, (*ppMockConsumer)->mDataBufferSize,
                                              pRetrievedSize);
        ASSERT_TRUE(retStatus == STATUS_SUCCESS || retStatus == STATUS_NO_MORE_DATA_AVAILABLE || retStatus == STATUS_AWAITING_PERSISTED_ACK ||
                    retStatus == STATUS_END_OF_STREAM)
            << "reading from the session after the outage failed with 0x" << std::hex << retStatus;
        ASSERT_GE(*pRetrievedSize, 4u) << "the session after the outage sent nothing";
    }

    // Terminates count successive handles, none of which ever calls getStreamData.
    void dropHandlesWithoutTransmitting(UINT32 count, PKinesisVideoStream pKinesisVideoStream, BOOL assertPreserved)
    {
        std::vector<UPLOAD_HANDLE> uploadHandles;
        MockConsumer* pMockConsumer = NULL;

        for (UINT32 i = 0; i < count; i++) {
            mStreamingSession.getActiveUploadHandles(uploadHandles);
            ASSERT_FALSE(uploadHandles.empty()) << "no successor handle was spawned after termination " << i;
            pMockConsumer = mStreamingSession.getConsumer(uploadHandles.back());
            ASSERT_TRUE(pMockConsumer != NULL);

            EXPECT_EQ(STATUS_SUCCESS, pMockConsumer->submitConnectionError(SERVICE_CALL_RESULT_OK));

            if (assertPreserved) {
                EXPECT_EQ(UPLOAD_CONNECTION_STATE_IN_USE, pKinesisVideoStream->connectionState)
                    << "handle " << i << " never transmitted but cleared the pending rollback";
            }
        }
    }
};
#ifdef ALIGNED_MEMORY_MODEL
TEST_P(StreamRecoveryFunctionalityTest, CreateStreamThenStreamResetConnectionEnsureRecovery)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer;
    BOOL didPutFrame, gotStreamData, submittedAck;
    UINT64 currentTime, streamStopTime, resetConnectionTime;

    CreateScenarioTestClient();
    PASS_TEST_FOR_ZERO_RETENTION_AND_OFFLINE();

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    streamStopTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 50 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    resetConnectionTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 30 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        EXPECT_EQ(STATUS_SUCCESS, mockProducer.timedPutFrame(currentTime, &didPutFrame));
        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (int i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            STATUS retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
            EXPECT_EQ(STATUS_SUCCESS, mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck));
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
        }

        if (IS_VALID_TIMESTAMP(resetConnectionTime) && currentTime >= resetConnectionTime) {
            // reset connection
            EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamTerminated(mStreamHandle, INVALID_UPLOAD_HANDLE_VALUE, SERVICE_CALL_RESULT_OK));
            resetConnectionTime = INVALID_TIMESTAMP_VALUE; // reset connection only once.
        }

    } while (currentTime < streamStopTime);

    VerifyStopStreamSyncAndFree();
}

TEST_P(StreamRecoveryFunctionalityTest, CreateStreamThenStreamResetConnectionAfterTokenRotationEnsureRecovery)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer;
    BOOL didPutFrame, gotStreamData, submittedAck;
    UINT64 currentTime, streamStopTime, resetConnectionTime;

    mDeviceInfo.clientInfo.stopStreamTimeout = STREAM_CLOSED_TIMEOUT_DURATION_IN_SECONDS * HUNDREDS_OF_NANOS_IN_A_SECOND;
    CreateScenarioTestClient();
    PASS_TEST_FOR_ZERO_RETENTION_AND_OFFLINE();

    mMockConsumerConfig.mUploadSpeedBytesPerSecond = 100;
    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);
    streamStopTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 50 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    resetConnectionTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 30 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        EXPECT_EQ(STATUS_SUCCESS, mockProducer.timedPutFrame(currentTime, &didPutFrame));
        if (!IS_VALID_TIMESTAMP(resetConnectionTime)) {
            mStreamingSession.getActiveUploadHandles(currentUploadHandles);
            for (int i = 0; i < currentUploadHandles.size(); i++) {
                UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
                mockConsumer = mStreamingSession.getConsumer(uploadHandle);
                STATUS retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
                EXPECT_EQ(STATUS_SUCCESS, mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck));
                VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            }
        }

        if (IS_VALID_TIMESTAMP(resetConnectionTime) && currentTime >= resetConnectionTime) {
            // reset connection
            EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamTerminated(mStreamHandle, INVALID_UPLOAD_HANDLE_VALUE, SERVICE_CALL_RESULT_OK));
            // restore slow speed after reset so all subsequent uploads are at normal speed
            mMockConsumerConfig.mUploadSpeedBytesPerSecond = 1000000;
            // restore speed for current active upload handles
            mStreamingSession.getActiveUploadHandles(currentUploadHandles);
            for (int i = 0; i < currentUploadHandles.size(); i++) {
                UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
                mockConsumer = mStreamingSession.getConsumer(uploadHandle);
                mockConsumer->mUploadSpeed = 1000000;
            }
            resetConnectionTime = INVALID_TIMESTAMP_VALUE; // reset connection only once.
        }

    } while (currentTime < streamStopTime);

    VerifyStopStreamSyncAndFree();
}

// Create stream, stream, last persisted ACK is before the rollback duration, the received ACK is
// after the rollback duration, inject a fault indicating not-dead host and terminate session.
// Make sure the rollback is to the received ACK + next fragment
TEST_P(StreamRecoveryFunctionalityTest, CreateStreamThenStreamRollbackToLastReceivedAckEnsureRecovery)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    UPLOAD_HANDLE errorHandle = INVALID_UPLOAD_HANDLE_VALUE;
    MockConsumer* mockConsumer;
    BOOL didPutFrame, gotStreamData = FALSE, submittedAck;
    UINT64 currentTime, streamStopTime, rollbackTime, lastReceivedAckTime = 0, lastPersistedAckTime = 0;
    STATUS retStatus;

    PASS_TEST_FOR_OFFLINE_OR_ZERO_RETENTION();

    mDeviceInfo.clientInfo.stopStreamTimeout = STREAM_CLOSED_TIMEOUT_DURATION_IN_SECONDS * HUNDREDS_OF_NANOS_IN_A_SECOND;
    CreateScenarioTestClient();

    DLOGD("mStreamInfo.streamCaps.streamingType %d mStreamInfo.retention %ld", mStreamInfo.streamCaps.streamingType, mStreamInfo.retention);

    mStreamInfo.streamCaps.replayDuration = 2 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    mMockConsumerConfig.mPersistAckDelayMs = 5000;
    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);
    streamStopTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 20 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    rollbackTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 9 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        EXPECT_EQ(STATUS_SUCCESS, mockProducer.timedPutFrame(currentTime, &didPutFrame));

        if (!IS_VALID_TIMESTAMP(rollbackTime)) {
            mStreamingSession.getActiveUploadHandles(currentUploadHandles);
            for (int i = 0; i < currentUploadHandles.size(); i++) {
                UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
                mockConsumer = mStreamingSession.getConsumer(uploadHandle);

                // First getStreamData should trigger rollback
                if (IS_VALID_TIMESTAMP(lastReceivedAckTime) && uploadHandle != errorHandle) {
                    UINT32 actualDataSize;
                    retStatus = getKinesisVideoStreamData(mStreamHandle, mockConsumer->mUploadHandle, mockConsumer->mDataBuffer, 1, &actualDataSize);
                    if (mStreamInfo.streamCaps.streamingType == STREAMING_TYPE_REALTIME) {
                        // Rollback to last received ack in realtime mode
                        EXPECT_EQ(lastReceivedAckTime * HUNDREDS_OF_NANOS_IN_A_MILLISECOND,
                                  FROM_STREAM_HANDLE(mStreamHandle)->curViewItem.viewItem.ackTimestamp);
                    } else {
                        // Rollback to the tail in offline mode
                        EXPECT_EQ(lastPersistedAckTime * HUNDREDS_OF_NANOS_IN_A_MILLISECOND +
                                      1 * HUNDREDS_OF_NANOS_IN_A_SECOND * mMockProducerConfig.mKeyFrameInterval / mMockProducerConfig.mFps,
                                  FROM_STREAM_HANDLE(mStreamHandle)->curViewItem.viewItem.ackTimestamp);
                    }
                    lastReceivedAckTime = INVALID_TIMESTAMP_VALUE; // only check when rollback happens
                } else {
                    retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
                }

                EXPECT_EQ(STATUS_SUCCESS, mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck));
                VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            }
        } else {
            mStreamingSession.getActiveUploadHandles(currentUploadHandles);
            for (int i = 0; i < currentUploadHandles.size(); i++) {
                UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
                mockConsumer = mStreamingSession.getConsumer(uploadHandle);
                retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
                VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
                if (mockConsumer != NULL) {
                    mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck);
                    if (submittedAck) {
                        if (mFragmentAck.ackType == FRAGMENT_ACK_TYPE_RECEIVED) {
                            lastReceivedAckTime = mFragmentAck.timestamp;
                        } else if (mFragmentAck.ackType == FRAGMENT_ACK_TYPE_PERSISTED) {
                            lastPersistedAckTime = mFragmentAck.timestamp;
                        }
                    }
                }
            }
        }

        if (IS_VALID_TIMESTAMP(rollbackTime) && currentTime >= rollbackTime) {
            // send error ack to trigger rollback
            submittedAck = FALSE;
            mStreamingSession.getActiveUploadHandles(currentUploadHandles);
            for (int i = 0; i < currentUploadHandles.size(); i++) {
                UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
                mockConsumer = mStreamingSession.getConsumer(uploadHandle);
                EXPECT_EQ(STATUS_SUCCESS,
                          mockConsumer->submitErrorAck(SERVICE_CALL_RESULT_FRAGMENT_ARCHIVAL_ERROR, lastReceivedAckTime, &submittedAck));
                if (submittedAck) {
                    rollbackTime = INVALID_TIMESTAMP_VALUE; // rollback only once.
                    errorHandle = uploadHandle;
                }
            }
        }
    } while (currentTime < streamStopTime);

    VerifyStopStreamSyncAndFree();
}

/*
 * contentView: Frag1 | Frag2 | Frag3 ...
 * Send a non recoverable error ack to Frag1. Make sure that Frag1 is not streamed in the new connection
 */
TEST_P(StreamRecoveryFunctionalityTest, CreateStreamThenStreamFatalErrorThrowAwayBadFragmentNearTail)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer = nullptr;
    BOOL gotStreamData = FALSE, submittedAck;
    UINT64 currentTime, stopTime;
    TID thread;
    STATUS retStatus;
    UINT32 i, totalFragmentPut = 10, sizeOfThreeFragments, totalByteSent = 0, retrievedDataSize = 0, ackReceived = 0;
    UPLOAD_HANDLE errorHandle;

    // need ack to count number of fragments streamed.
    PASS_TEST_FOR_OFFLINE_OR_ZERO_RETENTION();

    // reduce the frame size so that it generates fragments faster
    mMockProducerConfig.mFrameSizeByte = 500;
    CreateScenarioTestClient();
    sizeOfThreeFragments = mMockProducerConfig.mFrameSizeByte * 3 * mMockProducerConfig.mKeyFrameInterval;

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    // need ack to count number of fragments streamed. No ack case should also work if with ack works.
    for (i = 0; i < mMockProducerConfig.mKeyFrameInterval * totalFragmentPut; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // stream 3 fragments without submitting any acks
    while (totalByteSent < sizeOfThreeFragments) {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (gotStreamData) {
                totalByteSent += retrievedDataSize;
            }
        }
    }

    // submit a fatal error ack to current upload handle
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_EQ(STATUS_SUCCESS, mockConsumer->submitErrorAck(SERVICE_CALL_RESULT_FRAGMENT_DURATION_REACHED, &submittedAck));
    EXPECT_EQ(TRUE, submittedAck);
    errorHandle = mockConsumer->mUploadHandle;

    // loop until the error handle get end-of-stream
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
        VerifyGetStreamDataResult(retStatus, gotStreamData, mockConsumer->mUploadHandle, &currentTime, &mockConsumer);
    } while (!gotStreamData);

    // check that a new upload handle is created and the error handle is gone.
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_TRUE(currentUploadHandles[0] != errorHandle);
    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&thread, stopStreamSyncRoutine, (PVOID) this));
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // stream out whats left in the buffer
    stopTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 120 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (int i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (mockConsumer != NULL) {
                mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck);
                if (submittedAck) {
                    ackReceived++;
                }
            }
        }
    } while (currentTime < stopTime && !currentUploadHandles.empty());

    THREAD_JOIN(thread, NULL);

    EXPECT_TRUE(STATUS_SUCCESS == mThreadReturnStatus);
    EXPECT_TRUE(mStreamingSession.mConsumerList.empty());
    EXPECT_EQ(TRUE, ATOMIC_LOAD_BOOL(&mStreamClosed));

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));

    // Verify that we didnt get any ack for the bad fragment
    EXPECT_EQ(ackReceived, (totalFragmentPut - 1) * 3);
}

/*
 * contentView: Frag1 | Frag2 | Frag3 ...
 * Send a non recoverable error ack to Frag2 with a timestamp. Make sure that ONLY Frag2 is not streamed in the new connection
 */
TEST_P(StreamRecoveryFunctionalityTest, CreateStreamThenStreamFatalErrorWithTimestampThrowAwayBadFragmentMiddle)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer = nullptr;
    BOOL gotStreamData = FALSE, submittedAck;
    UINT64 currentTime, stopTime, ackTime = 0, duration;
    TID thread;
    STATUS retStatus;
    UINT32 i, totalFragmentPut = 10, sizeOfThreeFragments, totalByteSent = 0, retrievedDataSize = 0, ackReceived = 0;
    UPLOAD_HANDLE errorHandle;

    // need ack to count number of fragments streamed.
    PASS_TEST_FOR_OFFLINE_OR_ZERO_RETENTION();
    PASS_TEST_FOR_OFFLINE_ZERO_REPLAY_DURATION();

    // reduce the frame size so that it generates fragments faster
    mMockProducerConfig.mFrameSizeByte = 500;
    CreateScenarioTestClient();
    sizeOfThreeFragments = mMockProducerConfig.mFrameSizeByte * 3 * mMockProducerConfig.mKeyFrameInterval;

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    duration = (UINT64) 1000 / mMockProducerConfig.mFps * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    // need ack to count number of fragments streamed. No ack case should also work if with ack works.
    for (i = 0; i < mMockProducerConfig.mKeyFrameInterval * totalFragmentPut; i++) {
        if (i < mMockProducerConfig.mKeyFrameInterval) {
            ackTime += duration;
        }

        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // stream 3 fragments without submitting any acks
    while (totalByteSent < sizeOfThreeFragments) {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (gotStreamData) {
                totalByteSent += retrievedDataSize;
            }
        }
    }

    // submit a fatal error ack to current upload handle at timestamp corresponding to the second fragment
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_EQ(
        STATUS_SUCCESS,
        mockConsumer->submitErrorAck(SERVICE_CALL_RESULT_FRAGMENT_DURATION_REACHED, ackTime / HUNDREDS_OF_NANOS_IN_A_MILLISECOND, &submittedAck));
    EXPECT_EQ(TRUE, submittedAck);
    errorHandle = mockConsumer->mUploadHandle;

    // loop until the error handle get end-of-stream
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
        VerifyGetStreamDataResult(retStatus, gotStreamData, mockConsumer->mUploadHandle, &currentTime, &mockConsumer);
    } while (!gotStreamData);

    // check that a new upload handle is created and the error handle is gone.
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_TRUE(currentUploadHandles[0] != errorHandle);
    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&thread, stopStreamSyncRoutine, (PVOID) this));
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // stream out whats left in the buffer
    stopTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 120 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (mockConsumer != NULL) {
                mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck);
                if (submittedAck) {
                    ackReceived++;
                }
            }
        }
    } while (currentTime < stopTime && !currentUploadHandles.empty());

    THREAD_JOIN(thread, NULL);

    EXPECT_TRUE(STATUS_SUCCESS == mThreadReturnStatus);
    EXPECT_TRUE(mStreamingSession.mConsumerList.empty());
    EXPECT_EQ(TRUE, ATOMIC_LOAD_BOOL(&mStreamClosed));

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));

    // Verify that we didnt get any ack for the bad fragment
    EXPECT_EQ(ackReceived, (totalFragmentPut - 1) * 3);
}

/*
 * contentView: Frag1 | Frag2 | Frag3 ...
 * Send a non recoverable error ack on the 4th fragment ingestion without a timestamp.
 * Make sure that the earlier fragments are not streamed in the new connection.
 */
TEST_P(StreamRecoveryFunctionalityTest, CreateStreamThenStreamFatalErrorWithoutTimestampThrowAwayBadFragmentMiddle)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer = nullptr;
    BOOL gotStreamData = FALSE, submittedAck;
    UINT64 currentTime, stopTime;
    TID thread;
    STATUS retStatus;
    UINT32 i, totalFragmentPut = 10, sizeOfThreeFragments, totalByteSent = 0, retrievedDataSize = 0, ackReceived = 0;
    UPLOAD_HANDLE errorHandle;

    // need ack to count number of fragments streamed.
    PASS_TEST_FOR_OFFLINE_OR_ZERO_RETENTION();
    PASS_TEST_FOR_OFFLINE_ZERO_REPLAY_DURATION();

    // reduce the frame size so that it generates fragments faster
    mMockProducerConfig.mFrameSizeByte = 500;
    CreateScenarioTestClient();
    sizeOfThreeFragments = mMockProducerConfig.mFrameSizeByte * 3 * mMockProducerConfig.mKeyFrameInterval;

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    // need ack to count number of fragments streamed. No ack case should also work if with ack works.
    for (i = 0; i < mMockProducerConfig.mKeyFrameInterval * totalFragmentPut; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // stream 3 fragments without submitting any acks
    while (totalByteSent < sizeOfThreeFragments) {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (gotStreamData) {
                totalByteSent += retrievedDataSize;
            }
        }
    }

    // submit a fatal error ack to current upload handle at timestamp corresponding to the second fragment
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_EQ(STATUS_SUCCESS, mockConsumer->submitErrorAck(SERVICE_CALL_RESULT_FRAGMENT_DURATION_REACHED, INVALID_TIMESTAMP_VALUE, &submittedAck));
    EXPECT_EQ(TRUE, submittedAck);
    errorHandle = mockConsumer->mUploadHandle;

    // loop until the error handle get end-of-stream
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
        VerifyGetStreamDataResult(retStatus, gotStreamData, mockConsumer->mUploadHandle, &currentTime, &mockConsumer);
    } while (!gotStreamData);

    // check that a new upload handle is created and the error handle is gone.
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_TRUE(currentUploadHandles[0] != errorHandle);
    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&thread, stopStreamSyncRoutine, (PVOID) this));
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // stream out whats left in the buffer
    stopTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 120 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (mockConsumer != NULL) {
                mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck);
                if (submittedAck) {
                    ackReceived++;
                }
            }
        }
    } while (currentTime < stopTime && !currentUploadHandles.empty());

    THREAD_JOIN(thread, NULL);

    EXPECT_TRUE(STATUS_SUCCESS == mThreadReturnStatus);
    EXPECT_TRUE(mStreamingSession.mConsumerList.empty());
    EXPECT_EQ(TRUE, ATOMIC_LOAD_BOOL(&mStreamClosed));

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));

    // Verify that we didnt get any ack for the entire duration of the streamed upload handle
    EXPECT_EQ(ackReceived, (totalFragmentPut - 4) * 3);
}

/*
 * contentView: Frag1 | Frag2 | Frag3 ...
 * Send the first 3 fragments and issue a persistent ACKs for the first one. Send a non recoverable error ack
 * without a timestamp. Make sure that the first fragment is rolled back to and streamed whioe
 * the fragments from the persistent ACK till current are skipped in the new connection
 */
TEST_P(StreamRecoveryFunctionalityTest, CreateStreamThenStreamFatalErrorWithoutTimestampThrowAwayBadFromPersist)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer = nullptr;
    BOOL gotStreamData = FALSE, submittedAck;
    UINT64 currentTime, stopTime;
    TID thread;
    STATUS retStatus;
    UINT32 i, totalFragmentPut = 10, sizeOfThreeFragments, totalByteSent = 0, retrievedDataSize = 0, ackReceived = 0;
    UPLOAD_HANDLE errorHandle;

    // need ack to count number of fragments streamed.
    PASS_TEST_FOR_OFFLINE_OR_ZERO_RETENTION();
    PASS_TEST_FOR_OFFLINE_ZERO_REPLAY_DURATION();

    // reduce the frame size so that it generates fragments faster
    mMockProducerConfig.mFrameSizeByte = 500;
    CreateScenarioTestClient();
    sizeOfThreeFragments = mMockProducerConfig.mFrameSizeByte * 3 * mMockProducerConfig.mKeyFrameInterval;

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    // need ack to count number of fragments streamed. No ack case should also work if with ack works.
    for (i = 0; i < mMockProducerConfig.mKeyFrameInterval * totalFragmentPut; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // stream 3 fragments with submitting acks only for the first one
    while (totalByteSent < sizeOfThreeFragments) {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (gotStreamData) {
                totalByteSent += retrievedDataSize;
            }
        }
    }

    // Only ACK the first fragment
    if (mockConsumer != NULL) {
        mockConsumer->submitNormalAck(SERVICE_CALL_RESULT_OK, FRAGMENT_ACK_TYPE_BUFFERING, 0, &submittedAck);
        if (submittedAck) {
            ackReceived++;
        }
        mockConsumer->submitNormalAck(SERVICE_CALL_RESULT_OK, FRAGMENT_ACK_TYPE_RECEIVED, 0, &submittedAck);
        if (submittedAck) {
            ackReceived++;
        }
        mockConsumer->submitNormalAck(SERVICE_CALL_RESULT_OK, FRAGMENT_ACK_TYPE_PERSISTED, 0, &submittedAck);
        if (submittedAck) {
            ackReceived++;
        }
    }

    // submit a fatal error ack to current upload handle at timestamp corresponding to the second fragment
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_EQ(STATUS_SUCCESS, mockConsumer->submitErrorAck(SERVICE_CALL_RESULT_FRAGMENT_DURATION_REACHED, INVALID_TIMESTAMP_VALUE, &submittedAck));
    EXPECT_EQ(TRUE, submittedAck);
    errorHandle = mockConsumer->mUploadHandle;

    // loop until the error handle get end-of-stream
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
        VerifyGetStreamDataResult(retStatus, gotStreamData, mockConsumer->mUploadHandle, &currentTime, &mockConsumer);
    } while (!gotStreamData);

    // check that a new upload handle is created and the error handle is gone.
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_TRUE(currentUploadHandles[0] != errorHandle);
    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&thread, stopStreamSyncRoutine, (PVOID) this));
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // stream out whats left in the buffer
    stopTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 120 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedDataSize);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (mockConsumer != NULL) {
                mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck);
                if (submittedAck) {
                    ackReceived++;
                }
            }
        }
    } while (currentTime < stopTime && !currentUploadHandles.empty());

    THREAD_JOIN(thread, NULL);

    EXPECT_TRUE(STATUS_SUCCESS == mThreadReturnStatus);
    EXPECT_TRUE(mStreamingSession.mConsumerList.empty());
    EXPECT_EQ(TRUE, ATOMIC_LOAD_BOOL(&mStreamClosed));

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));

    // Verify that we didnt get any ack for the entire duration of the streamed upload handle back until the lsat
    // persisted ack which would be included in the rollback.
    EXPECT_EQ(ackReceived, (totalFragmentPut - 3) * 3);
}

/*
 * contentView: Frag1_Frame1 Frag1_Frame2 Frag1_Frame3 ...
 * Send a non recoverable error ack to Frag1 while it is not completed. Make sure that Frag1 is not streamed in the new connection
 */
TEST_P(StreamRecoveryFunctionalityTest, CreateStreamThenStreamFatalErrorThrowAwayBadFragmentAtHeadPartiallyStreamed)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer = nullptr;
    BOOL gotStreamData = FALSE, submittedAck;
    UINT64 currentTime, stopTime;
    TID thread;
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i, totalFragmentPut = 10, ackReceived = 0;
    UPLOAD_HANDLE errorHandle;

    PASS_TEST_FOR_OFFLINE_OR_ZERO_RETENTION();

    // reduce the frame size so that it generates fragments faster
    mMockProducerConfig.mFrameSizeByte = 500;
    CreateScenarioTestClient();

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    // put some frames but not all for the first fragment
    for (i = 0; i < mMockProducerConfig.mKeyFrameInterval - 5; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // stream out everything currently in buffer
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
        }
    } while (retStatus != STATUS_NO_MORE_DATA_AVAILABLE);

    // submit a fatal error ack to current upload handle
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    // Mock consumer hasnt queue up any acks at this point. Manually override error ack submission by specifying the
    // timestamp for the first fragment.
    EXPECT_EQ(STATUS_SUCCESS, mockConsumer->submitErrorAck(SERVICE_CALL_RESULT_FRAGMENT_DURATION_REACHED, 0, &submittedAck));
    EXPECT_EQ(TRUE, submittedAck);
    errorHandle = mockConsumer->mUploadHandle;

    // loop until the error handle get end-of-stream
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
        VerifyGetStreamDataResult(retStatus, gotStreamData, mockConsumer->mUploadHandle, &currentTime, &mockConsumer);
    } while (!gotStreamData);

    // finishing putting all frames for the first fragment.
    for (i = 0; i < 5; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // put in more fragments. At the end we are expecting totalFragmentPut * 3 acks.
    for (i = 0; i < mMockProducerConfig.mKeyFrameInterval * (totalFragmentPut - 1); i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // check that a new upload handle is created and the error handle is gone.
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_TRUE(currentUploadHandles[0] != errorHandle);
    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&thread, stopStreamSyncRoutine, (PVOID) this));
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // stream out whats left in the buffer
    stopTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 120 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (mockConsumer != NULL) {
                mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck);
                if (submittedAck) {
                    ackReceived++;
                }
            }
        }
    } while (currentTime < stopTime && !currentUploadHandles.empty());

    THREAD_JOIN(thread, NULL);

    EXPECT_TRUE(STATUS_SUCCESS == mThreadReturnStatus);
    EXPECT_TRUE(mStreamingSession.mConsumerList.empty());
    EXPECT_EQ(TRUE, ATOMIC_LOAD_BOOL(&mStreamClosed));

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));

    // Verify that we didnt get any ack for the bad fragment
    EXPECT_EQ(ackReceived, (totalFragmentPut - 1) * 3);
}

/*
 * contentView: Frag1_Frame1 Frag1_Frame2 Frag1_Frame3 ...
 * Send a non recoverable error ack to Frag1 while Frag1 is completed (next frame will be the start of Frag2).
 * Make sure that Frag1 is not streamed in the new connection
 */
TEST_P(StreamRecoveryFunctionalityTest, CreateStreamThenStreamFatalErrorThrowAwayBadFragmentAtHeadFullyStreamed)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer = nullptr;
    BOOL gotStreamData = FALSE, submittedAck;
    UINT64 currentTime, stopTime;
    TID thread;
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i, totalFragmentPut = 10, ackReceived = 0;
    UPLOAD_HANDLE errorHandle;

    PASS_TEST_FOR_OFFLINE_OR_ZERO_RETENTION();

    // reduce the frame size so that it generates fragments faster
    mMockProducerConfig.mFrameSizeByte = 500;
    CreateScenarioTestClient();

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    // put all frames for the first fragment
    for (i = 0; i < mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // stream out everything currently in buffer
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
        }
    } while (retStatus != STATUS_NO_MORE_DATA_AVAILABLE);

    // submit a fatal error ack to current upload handle
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    // Mock consumer hasnt queue up any acks at this point. Manually override error ack submission by specifying the
    // timestamp for the first fragment.
    EXPECT_EQ(STATUS_SUCCESS, mockConsumer->submitErrorAck(SERVICE_CALL_RESULT_FRAGMENT_DURATION_REACHED, 0, &submittedAck));
    EXPECT_EQ(TRUE, submittedAck);
    errorHandle = mockConsumer->mUploadHandle;

    // loop until the error handle get end-of-stream
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
        VerifyGetStreamDataResult(retStatus, gotStreamData, mockConsumer->mUploadHandle, &currentTime, &mockConsumer);
    } while (!gotStreamData);

    // put in more fragments. At the end we are expecting totalFragmentPut * 3 acks.
    for (i = 0; i < mMockProducerConfig.mKeyFrameInterval * (totalFragmentPut - 1); i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // check that a new upload handle is created and the error handle is gone.
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_TRUE(currentUploadHandles[0] != errorHandle);
    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&thread, stopStreamSyncRoutine, (PVOID) this));
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // stream out whats left in the buffer
    stopTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 120 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (mockConsumer != NULL) {
                mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck);
                if (submittedAck) {
                    ackReceived++;
                }
            }
        }
    } while (currentTime < stopTime && !currentUploadHandles.empty());

    THREAD_JOIN(thread, NULL);

    EXPECT_TRUE(STATUS_SUCCESS == mThreadReturnStatus);
    EXPECT_TRUE(mStreamingSession.mConsumerList.empty());
    EXPECT_EQ(TRUE, ATOMIC_LOAD_BOOL(&mStreamClosed));

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));

    // Verify that we didnt get any ack for the bad fragment
    EXPECT_EQ(ackReceived, (totalFragmentPut - 1) * 3);
}

/*
 * contentView: Frag1_Frame1 Frag1_Frame2 Frag1_Frame3 ... Frag2_fram1,.... FragN_frameN
 * Timeout the upload handle with current view non-zero. Ensure immediate auto-recovery with rollback
 */
TEST_P(StreamRecoveryFunctionalityTest, CreateStreamThenStreamTimeoutWithBuffer)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer = nullptr;
    BOOL gotStreamData = FALSE, submittedAck;
    UINT64 currentTime, stopTime;
    TID thread;
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i, totalFragmentPut = 10, ackReceived = 0;
    UPLOAD_HANDLE errorHandle;

    PASS_TEST_FOR_OFFLINE();

    // reduce the frame size so that it generates fragments faster
    CreateScenarioTestClient();

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    // put all frames for the first few fragments
    for (i = 0; i < 5 * mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // stream out a few fragments but still have some in the buffer
    for (i = 0; i < 3 * mMockProducerConfig.mKeyFrameInterval; i++) {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (int j = 0; j < currentUploadHandles.size(); j++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[j];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
        }
    }

    // submit a timeout to current upload handle
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());

    EXPECT_EQ(STATUS_SUCCESS, mockConsumer->submitConnectionError(SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT));
    errorHandle = mockConsumer->mUploadHandle;

    EXPECT_EQ(2, ATOMIC_LOAD(&mPutStreamFuncCount));

    // loop until the error handle get end-of-stream
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
        VerifyGetStreamDataResult(retStatus, gotStreamData, mockConsumer->mUploadHandle, &currentTime, &mockConsumer);
    } while (!gotStreamData);

    // put in more fragments. At the end we are expecting totalFragmentPut * 3 acks.
    for (i = 0; i < mMockProducerConfig.mKeyFrameInterval * (totalFragmentPut - 1); i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // check that a new upload handle is created and the error handle is gone.
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_TRUE(currentUploadHandles[0] != errorHandle);
    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&thread, stopStreamSyncRoutine, (PVOID) this));
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // stream out whats left in the buffer
    stopTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 120 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (mockConsumer != NULL) {
                mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck);
                if (submittedAck) {
                    ackReceived++;
                }
            }
        }
    } while (currentTime < stopTime && !currentUploadHandles.empty());

    THREAD_JOIN(thread, NULL);

    EXPECT_TRUE(STATUS_SUCCESS == mThreadReturnStatus);
    EXPECT_TRUE(mStreamingSession.mConsumerList.empty());
    EXPECT_EQ(TRUE, ATOMIC_LOAD_BOOL(&mStreamClosed));

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));
}

/*
 * contentView: Frag1_Frame1 Frag1_Frame2 Frag1_Frame3 ... Frag2_fram1,.... FragN_frameN
 * Timeout the upload handle with current view zero. Ensure no immediate auto-recovery
 */
TEST_P(StreamRecoveryFunctionalityTest, CreateStreamThenStreamTimeoutWithoutBuffer)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer = nullptr;
    BOOL gotStreamData = FALSE, submittedAck;
    UINT64 currentTime, stopTime;
    TID thread;
    STATUS retStatus = STATUS_SUCCESS;
    BYTE dataBuf[TEST_DEFAULT_PRODUCER_CONFIG_FRAME_SIZE + 1000]; // Should be over the default frame size
    UINT32 i, ackReceived = 0, dataBufSize = SIZEOF(dataBuf), retrievedSize, overhead, numFragments = 5;
    UPLOAD_HANDLE errorHandle;
    PKinesisVideoStream pKinesisVideoStream;
    PStreamMkvGenerator pStreamMkvGenerator;

    PASS_TEST_FOR_OFFLINE();

    // reduce the frame size so that it generates fragments faster
    mMockProducerConfig.mFrameSizeByte = 500;

    // reduce the frame size so that it generates fragments faster
    CreateScenarioTestClient();

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    // put all frames for the first few fragments
    for (i = 0; i < numFragments * mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame());
    }

    // Put one more frame to ensure we are on a non-key frame
    mockProducer.putFrame();

    // stream out everything currently in buffer
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
        }
    } while (retStatus != STATUS_NO_MORE_DATA_AVAILABLE);

    // submit a timeout to current upload handle
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());

    // Expected call counts
    EXPECT_EQ(1, ATOMIC_LOAD(&mPutStreamFuncCount));
    EXPECT_EQ(3, ATOMIC_LOAD(&mDescribeStreamFuncCount)); // NOTE: Describe is made to fail a few times
    EXPECT_EQ(0, ATOMIC_LOAD(&mCreateStreamFuncCount));
    EXPECT_EQ(0, ATOMIC_LOAD(&mTagResourceFuncCount));
    EXPECT_EQ(1, ATOMIC_LOAD(&mGetStreamingEndpointFuncCount));
    EXPECT_EQ(1, ATOMIC_LOAD(&mStreamReadyFuncCount));

    EXPECT_EQ(STATUS_SUCCESS, mockConsumer->submitConnectionError(SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT));
    errorHandle = mockConsumer->mUploadHandle;

    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // No recovery
    EXPECT_EQ(1, ATOMIC_LOAD(&mPutStreamFuncCount));
    EXPECT_EQ(3, ATOMIC_LOAD(&mDescribeStreamFuncCount));
    EXPECT_EQ(0, ATOMIC_LOAD(&mCreateStreamFuncCount));
    EXPECT_EQ(0, ATOMIC_LOAD(&mTagResourceFuncCount));
    EXPECT_EQ(1, ATOMIC_LOAD(&mGetStreamingEndpointFuncCount));
    EXPECT_EQ(1, ATOMIC_LOAD(&mStreamReadyFuncCount));

    // Put some non-key frames until the next key frame - stream should have recovered
    for (i = 0; i < mMockProducerConfig.mKeyFrameInterval - 1; i++) {
        mockProducer.putFrame();
        // Recovery till ready state
        EXPECT_EQ(2, ATOMIC_LOAD(&mPutStreamFuncCount));
        EXPECT_EQ(3, ATOMIC_LOAD(&mDescribeStreamFuncCount)); // remains the same
        EXPECT_EQ(0, ATOMIC_LOAD(&mCreateStreamFuncCount));
        EXPECT_EQ(0, ATOMIC_LOAD(&mTagResourceFuncCount));
        EXPECT_EQ(1, ATOMIC_LOAD(&mGetStreamingEndpointFuncCount)); // remains the same
        EXPECT_EQ(2, ATOMIC_LOAD(&mStreamReadyFuncCount));
    }

    // Next one should be a key frame.
    // In this case it shouldn't make a stream start as we have a rollback
    // and the persisted ACK has not been submitted so the stream start
    // should be a key frame from past.
    mockProducer.putFrame();
    EXPECT_EQ(2, ATOMIC_LOAD(&mPutStreamFuncCount));

    // Put some non-key frames to complete the fragment
    for (i = 0; i < mMockProducerConfig.mKeyFrameInterval - 1; i++) {
        mockProducer.putFrame();
    }

    // check that a new upload handle is created and the error handle is gone.
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_TRUE(currentUploadHandles[0] != errorHandle);

    // Validate that the new upload handle has a rolled back proper start stream
    EXPECT_EQ(STATUS_SUCCESS, getKinesisVideoStreamData(mStreamHandle, currentUploadHandles[0], dataBuf, dataBufSize, &retrievedSize));
    EXPECT_EQ(dataBufSize, retrievedSize);

    pKinesisVideoStream = FROM_STREAM_HANDLE(mStreamHandle);
    pStreamMkvGenerator = (PStreamMkvGenerator) pKinesisVideoStream->pMkvGenerator;
    // Get the overhead size
    overhead = mkvgenGetFrameOverhead(pStreamMkvGenerator, MKV_STATE_START_STREAM);

    // Check the content of the buffer
    // NOTE: We had submitted ACKs for all of the fragments before the termination
    // so the entire buffer is trimmed. We will be skipping over the frames
    // until the next key frame which will become the stream start
    for (i = 0; i < TEST_DEFAULT_PRODUCER_CONFIG_FRAME_SIZE; i++) {
        if (mStreamInfo.streamCaps.replayDuration == 0) {
            EXPECT_EQ((numFragments + 1) * mMockProducerConfig.mKeyFrameInterval, dataBuf[i + overhead]) << "Failed on " << i;
        } else {
            EXPECT_EQ(0, dataBuf[i + overhead]) << "Failed on " << i;
        }
    }

    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&thread, stopStreamSyncRoutine, (PVOID) this));
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // stream out whats left in the buffer
    stopTime = mClientCallbacks.getCurrentTimeFn((UINT64) this) + 120 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        for (i = 0; i < currentUploadHandles.size(); i++) {
            UPLOAD_HANDLE uploadHandle = currentUploadHandles[i];
            mockConsumer = mStreamingSession.getConsumer(uploadHandle);
            retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
            VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
            if (mockConsumer != NULL) {
                mockConsumer->timedSubmitNormalAck(currentTime, &submittedAck);
                if (submittedAck) {
                    ackReceived++;
                }
            }
        }
    } while (currentTime < stopTime && !currentUploadHandles.empty());

    THREAD_JOIN(thread, NULL);

    EXPECT_TRUE(STATUS_SUCCESS == mThreadReturnStatus);
    EXPECT_TRUE(mStreamingSession.mConsumerList.empty());
    EXPECT_EQ(TRUE, ATOMIC_LOAD_BOOL(&mStreamClosed));

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));
}

TEST_P(StreamRecoveryFunctionalityTest, streamStartViewDroppedBeforeFullyConsumedRecoverable)
{
    UINT64 currentTime, testTerminationTime;
    CreateScenarioTestClient();
    BOOL didPutFrame, gotStreamData = FALSE, submittedAck;
    UINT32 tokenRotateCount = 0, i;
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer;
    STATUS status = STATUS_SUCCESS;

    CreateScenarioTestClient();
    /* data buffer need to be less than frame size so a single frame needs two getStreamData calls to be consumed */
    mMockProducerConfig.mFrameSizeByte = 50000;
    mMockConsumerConfig.mDataBufferSizeByte = 30000;

    PASS_TEST_FOR_ZERO_RETENTION_AND_OFFLINE();
    PASS_TEST_FOR_OFFLINE()

    /* content view can contain only 40 frames */
    mStreamInfo.streamCaps.frameRate = 20;
    mStreamInfo.streamCaps.bufferDuration = 2 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    mStreamInfo.streamCaps.viewOverflowPolicy = CONTENT_VIEW_OVERFLOW_POLICY_DROP_TAIL_VIEW_ITEM;

    CreateStreamSync();
    /* default fps is 20 */
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    /* fill the content view */
    for (i = 0; i < 40; ++i) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    /* should only be 1 because we didnt hit token rotation */
    EXPECT_EQ(1, currentUploadHandles.size());

    UPLOAD_HANDLE uploadHandle = currentUploadHandles[0];
    mockConsumer = mStreamingSession.getConsumer(uploadHandle);

    /* do ONE successful getStreamData */
    while (!gotStreamData) {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);
        status = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
        VerifyGetStreamDataResult(status, gotStreamData, uploadHandle, &currentTime, &mockConsumer);
    }

    /* put 40 frames again. The first 40 frames are now all dropped */
    for (i = 0; i < 40; ++i) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    status = STATUS_SUCCESS;
    gotStreamData = FALSE;

    /* do another getStreamData */
    while (!gotStreamData) {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);
        status = mockConsumer->timedGetStreamData(currentTime, &gotStreamData);
    }

    EXPECT_EQ(STATUS_SUCCESS, status);

    VerifyStopStreamSyncAndFree();
}

TEST_P(StreamRecoveryFunctionalityTest, FragmentMetadataStartStreamFailRecovery)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer = nullptr;
    BOOL gotStreamData = FALSE, submittedAck, firstChunk = TRUE;
    UINT64 currentTime, stopTime;
    TID thread;
    STATUS retStatus = STATUS_SUCCESS;
    BYTE dataBuf[TEST_DEFAULT_PRODUCER_CONFIG_FRAME_SIZE + 1000];
    BYTE storedDataBuf[SIZEOF(dataBuf)];
    UINT32 i, ackReceived = 0, dataBufSize = SIZEOF(dataBuf), retrievedSize, storedRetrievedSize, overhead, numFragments = 1;
    UPLOAD_HANDLE errorHandle;
    PKinesisVideoStream pKinesisVideoStream;
    PStreamMkvGenerator pStreamMkvGenerator;

    PASS_TEST_FOR_OFFLINE_ZERO_REPLAY_DURATION();

    // reduce the frame size so that it generates fragments faster
    mMockProducerConfig.mFrameSizeByte = 500;

    // reduce the frame size so that it generates fragments faster
    CreateScenarioTestClient();

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    // Start with some metadata - important that it's before frames
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFragmentMetadata(mStreamHandle, (PCHAR) "TestName", (PCHAR) "TestValue", TRUE));

    // put all frames for the first few fragments
    for (i = 0; i < numFragments * mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame());
    }

    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());

    // stream out everything currently in buffer
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        UPLOAD_HANDLE uploadHandle = currentUploadHandles[0];
        mockConsumer = mStreamingSession.getConsumer(uploadHandle);
        retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedSize);
        VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);

        // Store the first chunk for later comparison
        if (firstChunk) {
            storedRetrievedSize = MIN(dataBufSize, retrievedSize);
            MEMCPY(storedDataBuf, mockConsumer->mDataBuffer, storedRetrievedSize);
            firstChunk = FALSE;
        }
    } while (retStatus != STATUS_NO_MORE_DATA_AVAILABLE);

    // submit an error ACK and ensure we roll back
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    mockConsumer = mStreamingSession.getConsumer(currentUploadHandles[0]);
    EXPECT_EQ(STATUS_SUCCESS, mockConsumer->submitConnectionError(SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT));
    errorHandle = mockConsumer->mUploadHandle;

    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    mockProducer.putFrame();
    EXPECT_EQ(2, ATOMIC_LOAD(&mPutStreamFuncCount));

    // Should have two
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_NE(errorHandle, currentUploadHandles[0]);

    // Get the data with the new handle
    EXPECT_EQ(STATUS_SUCCESS, getKinesisVideoStreamData(mStreamHandle, currentUploadHandles[0], dataBuf, dataBufSize, &retrievedSize));
    EXPECT_EQ(storedRetrievedSize, retrievedSize);

    EXPECT_EQ(0, MEMCMP(dataBuf, storedDataBuf, storedRetrievedSize));

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));
}

TEST_P(StreamRecoveryFunctionalityTest, EventMetadataStartStreamFailRecovery)
{
    std::vector<UPLOAD_HANDLE> currentUploadHandles;
    MockConsumer* mockConsumer = nullptr;
    BOOL gotStreamData = FALSE, submittedAck, firstChunk = TRUE;
    UINT64 currentTime, stopTime;
    TID thread;
    STATUS retStatus = STATUS_SUCCESS;
    BYTE dataBuf[TEST_DEFAULT_PRODUCER_CONFIG_FRAME_SIZE + 1000];
    BYTE storedDataBuf[SIZEOF(dataBuf)];
    UINT32 i, ackReceived = 0, dataBufSize = SIZEOF(dataBuf), retrievedSize, storedRetrievedSize, overhead, numFragments = 1;
    UPLOAD_HANDLE errorHandle;
    PKinesisVideoStream pKinesisVideoStream;
    PStreamMkvGenerator pStreamMkvGenerator;

    PASS_TEST_FOR_OFFLINE_ZERO_REPLAY_DURATION();

    // reduce the frame size so that it generates fragments faster
    mMockProducerConfig.mFrameSizeByte = 500;

    // reduce the frame size so that it generates fragments faster
    CreateScenarioTestClient();

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    // Start with some metadata - important that it's before frames
    // We expect this to fail because we do not allow putting this metadata before stream is started
    EXPECT_EQ(STATUS_STREAM_NOT_STARTED, putKinesisVideoEventMetadata(mStreamHandle, STREAM_EVENT_TYPE_NOTIFICATION, NULL));

    // put all frames for the first few fragments
    for (i = 0; i < numFragments * mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame());
    }

    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());

    // stream out everything currently in buffer
    do {
        currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);

        mStreamingSession.getActiveUploadHandles(currentUploadHandles);
        UPLOAD_HANDLE uploadHandle = currentUploadHandles[0];
        mockConsumer = mStreamingSession.getConsumer(uploadHandle);
        retStatus = mockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedSize);
        VerifyGetStreamDataResult(retStatus, gotStreamData, uploadHandle, &currentTime, &mockConsumer);

        // Store the first chunk for later comparison
        if (firstChunk) {
            storedRetrievedSize = MIN(dataBufSize, retrievedSize);
            MEMCPY(storedDataBuf, mockConsumer->mDataBuffer, storedRetrievedSize);
            firstChunk = FALSE;
        }
    } while (retStatus != STATUS_NO_MORE_DATA_AVAILABLE);

    // submit an error ACK and ensure we roll back
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    mockConsumer = mStreamingSession.getConsumer(currentUploadHandles[0]);
    EXPECT_EQ(STATUS_SUCCESS, mockConsumer->submitConnectionError(SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT));
    errorHandle = mockConsumer->mUploadHandle;

    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    mockProducer.putFrame();
    EXPECT_EQ(2, ATOMIC_LOAD(&mPutStreamFuncCount));

    // Should have two
    mStreamingSession.getActiveUploadHandles(currentUploadHandles);
    EXPECT_EQ(1, currentUploadHandles.size());
    EXPECT_NE(errorHandle, currentUploadHandles[0]);

    // Get the data with the new handle
    EXPECT_EQ(STATUS_SUCCESS, getKinesisVideoStreamData(mStreamHandle, currentUploadHandles[0], dataBuf, dataBufSize, &retrievedSize));
    EXPECT_EQ(storedRetrievedSize, retrievedSize);

    EXPECT_EQ(0, MEMCMP(dataBuf, storedDataBuf, storedRetrievedSize));

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));
}

//
// A session that transmitted and then lost the link leaves a rollback and header fix-up owed to whoever
// transmits next. Reconnect attempts that die before sending a byte must not discard it.
//
TEST_P(StreamRecoveryFunctionalityTest, ConnectionResetThenIdleHandlesPreservePendingHeaderFixup)
{
    PKinesisVideoStream pKinesisVideoStream;

    CreateScenarioTestClient();
    PASS_TEST_FOR_ZERO_RETENTION_AND_OFFLINE();

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);
    pKinesisVideoStream = FROM_STREAM_HANDLE(mStreamHandle);

    transmitThenDropFirstHandle(mockProducer);
    EXPECT_EQ(UPLOAD_CONNECTION_STATE_IN_USE, pKinesisVideoStream->connectionState)
        << "a handle that transmitted must leave a pending rollback behind";

    // Handles created while the link is still down fail before sending anything. None of them may clear
    // the rollback owed by the handle that transmitted before the outage.
    dropHandlesWithoutTransmitting(5, pKinesisVideoStream, TRUE);

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));
}

//
// The consequence of the above: once the link returns, the first session to transmit must run
// streamStartFixupOnReconnect, so its body must begin with an EBML header rather than mid-Cluster.
//
TEST_P(StreamRecoveryFunctionalityTest, SessionAfterConnectionResetBeginsWithMkvHeader)
{
    std::vector<UPLOAD_HANDLE> uploadHandles;
    MockConsumer* pMockConsumer = NULL;
    PKinesisVideoStream pKinesisVideoStream;
    UINT32 retrievedSize = 0;
    BOOL gotStreamData = FALSE;
    UINT64 currentTime;

    CreateScenarioTestClient();
    PASS_TEST_FOR_ZERO_RETENTION_AND_OFFLINE();

    CreateStreamSync();

    // Realtime streams with no replay window are excluded. The exclusion is limited to realtime because an
    // offline stream rolls back to the tail whatever the replay window and then runs the fix-up, so it owes
    // a header even with no replay window. The companion test above still covers the flag itself for every
    // configuration.
    if (!IS_OFFLINE_STREAMING_MODE(mStreamInfo.streamCaps.streamingType) && mStreamInfo.streamCaps.replayDuration == 0) {
        EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));
        return;
    }

    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);
    pKinesisVideoStream = FROM_STREAM_HANDLE(mStreamHandle);

    transmitThenDropFirstHandle(mockProducer);
    dropHandlesWithoutTransmitting(5, pKinesisVideoStream, FALSE);

    // The link is back. Keep producing so the surviving session has something to send.
    for (UINT32 i = 0; i < mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    mStreamingSession.getActiveUploadHandles(uploadHandles);
    ASSERT_FALSE(uploadHandles.empty()) << "no session available after the reset";
    pMockConsumer = mStreamingSession.getConsumer(uploadHandles.back());
    ASSERT_TRUE(pMockConsumer != NULL);

    currentTime = mClientCallbacks.getCurrentTimeFn((UINT64) this);
    EXPECT_EQ(STATUS_SUCCESS, pMockConsumer->timedGetStreamData(currentTime, &gotStreamData, &retrievedSize));
    ASSERT_TRUE(gotStreamData);
    ASSERT_GE(retrievedSize, 4u) << "first session after the reset sent nothing";

    // EBML header magic. Its absence is the headerless body the service rejects with INVALID_MKV_DATA.
    EXPECT_EQ(0x1A, pMockConsumer->mDataBuffer[0]) << "body does not begin with an EBML header";
    EXPECT_EQ(0x45, pMockConsumer->mDataBuffer[1]) << "body does not begin with an EBML header";
    EXPECT_EQ(0xDF, pMockConsumer->mDataBuffer[2]) << "body does not begin with an EBML header";
    EXPECT_EQ(0xA3, pMockConsumer->mDataBuffer[3]) << "body does not begin with an EBML header";

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));
}

//
// Simulate a whole outage rather than using individual calls: a session transmits, the link is cut, several
// reconnect attempts die before sending a byte, and the link returns. The session that transmits next must
// begin its body with an EBML header, or the service rejects the fragment with INVALID_MKV_DATA.
//
// The session runs past the replay window before the cut, so the recovery replays from a mid-stream key frame,
// rather than from the stream's first item.
//
TEST_P(StreamRecoveryFunctionalityTest, OutageWithIdleReconnectsStillYieldsMkvHeaderOnRecovery)
{
    PKinesisVideoStream pKinesisVideoStream;
    MockConsumer* pMockConsumer = NULL;
    UPLOAD_HANDLE preOutageHandle = INVALID_UPLOAD_HANDLE_VALUE;
    UINT32 retrievedSize = 0;

    CreateScenarioTestClient();
    PASS_TEST_FOR_ZERO_RETENTION_AND_OFFLINE();

    // Small frames, so one read drains a second of video. Set after CreateScenarioTestClient, which resets the
    // producer defaults.
    mMockProducerConfig.mFrameSizeByte = 500;

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);
    pKinesisVideoStream = FROM_STREAM_HANDLE(mStreamHandle);

    ASSERT_NO_FATAL_FAILURE(driveOutage(mockProducer, pKinesisVideoStream, TRUE, &preOutageHandle));

    // The link returns and the next session transmits. A headerless body here is the reported failure.
    ASSERT_NO_FATAL_FAILURE(readRecoveryBody(mockProducer, preOutageHandle, &pMockConsumer, &retrievedSize));

    EXPECT_TRUE(beginsWithEbmlHeader(pMockConsumer->mDataBuffer, retrievedSize))
        << "body after the outage does not begin with an EBML header; first bytes are " << std::hex << (UINT32) pMockConsumer->mDataBuffer[0] << " "
        << (UINT32) pMockConsumer->mDataBuffer[1] << " " << (UINT32) pMockConsumer->mDataBuffer[2] << " " << (UINT32) pMockConsumer->mDataBuffer[3];

    // The fix-up has been consumed by the session that transmitted, so nothing is left owed.
    EXPECT_FALSE(CHECK_UPLOAD_CONNECTION_STATE_IN_USE(pKinesisVideoStream->connectionState))
        << "the header fix-up was not consumed by the session that transmitted";

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));
}

//
// Guards the byte check in the test above against passing vacuously. Same flow, except the pending fix-up is
// cleared by hand before the recovery session transmits, which is what the defect did. The session then resumes
// after the last item it sent and its body carries no header.
//
TEST_P(StreamRecoveryFunctionalityTest, DiscardingThePendingFixupLeavesTheRecoveryBodyHeaderless)
{
    PKinesisVideoStream pKinesisVideoStream;
    MockConsumer* pMockConsumer = NULL;
    UPLOAD_HANDLE preOutageHandle = INVALID_UPLOAD_HANDLE_VALUE;
    UINT32 retrievedSize = 0;

    CreateScenarioTestClient();
    PASS_TEST_FOR_ZERO_RETENTION_AND_OFFLINE();

    mMockProducerConfig.mFrameSizeByte = 500;

    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);
    pKinesisVideoStream = FROM_STREAM_HANDLE(mStreamHandle);

    ASSERT_NO_FATAL_FAILURE(driveOutage(mockProducer, pKinesisVideoStream, FALSE, &preOutageHandle));

    // Emulate the defect: the pending fix-up is thrown away by a handle that never transmitted.
    pKinesisVideoStream->connectionState = UPLOAD_CONNECTION_STATE_NOT_IN_USE;

    ASSERT_NO_FATAL_FAILURE(readRecoveryBody(mockProducer, preOutageHandle, &pMockConsumer, &retrievedSize));

    EXPECT_FALSE(beginsWithEbmlHeader(pMockConsumer->mDataBuffer, retrievedSize))
        << "clearing the pending fix-up should have produced a headerless body; if a header still appears, the check in the "
        << "test above is not exercising the fix-up";

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));
}

INSTANTIATE_TEST_SUITE_P(PermutatedStreamInfo, StreamRecoveryFunctionalityTest,
                         Combine(Values(STREAMING_TYPE_REALTIME, STREAMING_TYPE_OFFLINE), Values(0, 10 * HUNDREDS_OF_NANOS_IN_AN_HOUR), Bool(),
                                 Values(0, TEST_REPLAY_DURATION)));

#endif