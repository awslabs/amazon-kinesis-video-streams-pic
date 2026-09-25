#include "ClientTestFixture.h"

//
// Regression coverage for FRAGMENT_TIMECODE_LESSER_THAN_PREVIOUS (4004).
//
// A genuine timecode base boundary produced by a generator reset (rebase) is written into the content
// view as a view item flagged ITEM_FLAG_STREAM_START together with ITEM_FLAG_TIMECODE_BASE_START, and
// carrying a fresh EBML header. Two SDK guards normally keep exactly one timecode base per PutMedia
// segment (one upload handle): a session that advances onto such an item is terminated at it, and in
// RELATIVE mode a new session cannot start without a stream-start timestamp.
//
// The 4004 defect was that resetCurrentViewItemStreamStart() would strip the header AND the stream-start
// marker off the boundary once any session had advanced past it, because it could not tell a genuine
// generator-reset boundary from an EBML header that the reconnect fix-up itself had added. Once the
// marker was gone, a later rollback landing before the (now unmarked) boundary would replay two timecode
// bases inside a single segment -> a cluster timecode lower than the previous one -> 4004.
//
// The fix marks genuine boundaries with ITEM_FLAG_TIMECODE_BASE_START at generation time and makes
// resetCurrentViewItemStreamStart() refuse to strip them, while still stripping fix-up-added headers
// (which are not timecode boundaries) so no duplicate header is ever emitted on replay.
//
// The marker and the strip are timecode-mode independent, so these tests do not pin absoluteFragmentTimes.
// CreateScenarioTestClient sets it to TRUE via initScenarioTestMembers regardless. RELATIVE mode is what
// turns the stripped boundary into a stalled stream on the wire, which the reproduction harness covers.
//
class StreamStartBoundaryFunctionalityTest : public ClientTestBase {
  protected:
    void SetUp() override
    {
        ClientTestBase::SetUp();

        // REALTIME stream with ACKs and large-enough retention that nothing is trimmed out of the view.
        mStreamInfo.streamCaps.streamingType = STREAMING_TYPE_REALTIME;
        mStreamInfo.streamCaps.fragmentAcks = TRUE;
        mStreamInfo.retention = 10 * HUNDREDS_OF_NANOS_IN_AN_HOUR;
        mStreamInfo.streamCaps.replayDuration = TEST_REPLAY_DURATION;
    }
};

// These tests drive frames through putFrame, so they build only under the aligned memory model, as every
// other frame-driving functionality tests. Without it the AIV heap does not round
// allocations up to 8 bytes and lays allocation headers on unaligned addresses, which the undefined
// behaviour sanitizer reports from inside the heap itself.
#ifdef ALIGNED_MEMORY_MODEL

// Helpers to lock/unlock the stream while inspecting the content view.
#define BOUNDARY_TEST_LOCK(s, c)   (c)->clientCallbacks.lockMutexFn((c)->clientCallbacks.customData, (s)->base.lock)
#define BOUNDARY_TEST_UNLOCK(s, c) (c)->clientCallbacks.unlockMutexFn((c)->clientCallbacks.customData, (s)->base.lock)

//
// A generator rebase produced while the view still holds earlier content on the previous base is a genuine
// timecode base boundary. It carries ITEM_FLAG_TIMECODE_BASE_START, and that marker plus its EBML header must
// survive being advanced past, because a later rollback could otherwise replay across the boundary and land two
// bases in one segment. Before the fix, resetCurrentViewItemStreamStart cleared ITEM_FLAG_STREAM_START and
// shrank the item here, which is exactly what armed the 4004.
//
// The initial stream start is deliberately NOT marked. It is created on an empty view, so there is no earlier
// base for a rollback to collide with, and marking it would force a session split when a graceful-stop drain
// rolls back across it, stalling shutdown until the stop timeout expires.
//
TEST_F(StreamStartBoundaryFunctionalityTest, GenuineRebaseBoundaryIsMarkedAndSurvivesReset)
{
    PViewItem pViewItem = NULL;
    UINT64 tailIndex, boundaryIndex = INVALID_VIEW_INDEX_VALUE, idx;
    UINT32 origLength, origDataOffset;
    PKinesisVideoStream pKinesisVideoStream;
    PKinesisVideoClient pKinesisVideoClient;

    CreateScenarioTestClient();
    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    pKinesisVideoStream = FROM_STREAM_HANDLE(mStreamHandle);
    pKinesisVideoClient = pKinesisVideoStream->pKinesisVideoClient;

    // Put a full fragment. The very first frame carries the initial stream start.
    for (UINT32 i = 0; i < mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // The initial stream start carries a header but, being created on an empty view, is not a base boundary.
    BOUNDARY_TEST_LOCK(pKinesisVideoStream, pKinesisVideoClient);
    EXPECT_EQ(STATUS_SUCCESS, contentViewGetTail(pKinesisVideoStream->pView, &pViewItem));
    tailIndex = pViewItem->index;
    EXPECT_TRUE(CHECK_ITEM_STREAM_START(pViewItem->flags));
    EXPECT_FALSE(CHECK_ITEM_TIMECODE_BASE_START(pViewItem->flags)) << "initial (empty-view) stream start must not be a boundary";
    BOUNDARY_TEST_UNLOCK(pKinesisVideoStream, pKinesisVideoClient);

    // Force a rebase on the next key frame - this is exactly what a token rotation / error-ACK does. The
    // earlier fragment is still in the view, so the rebased stream start is a genuine base boundary.
    pKinesisVideoStream->resetGeneratorOnKeyFrame = TRUE;
    for (UINT32 i = 0; i < mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    // Find the rebased boundary: a stream-start item that is not the initial one.
    BOUNDARY_TEST_LOCK(pKinesisVideoStream, pKinesisVideoClient);
    for (idx = tailIndex + 1; contentViewGetItemAt(pKinesisVideoStream->pView, idx, &pViewItem) == STATUS_SUCCESS; idx++) {
        if (CHECK_ITEM_STREAM_START(pViewItem->flags)) {
            boundaryIndex = idx;
            break;
        }
    }
    ASSERT_NE(INVALID_VIEW_INDEX_VALUE, boundaryIndex) << "rebase did not produce a stream-start boundary";

    EXPECT_EQ(STATUS_SUCCESS, contentViewGetItemAt(pKinesisVideoStream->pView, boundaryIndex, &pViewItem));
    origLength = pViewItem->length;
    origDataOffset = GET_ITEM_DATA_OFFSET(pViewItem->flags);

    // A rebase-produced stream start must carry the base-start marker and a real EBML header (non-zero
    // data offset). Earlier content is still in the view here, so a rollback could replay across it.
    EXPECT_TRUE(CHECK_ITEM_STREAM_START(pViewItem->flags));
    EXPECT_TRUE(CHECK_ITEM_TIMECODE_BASE_START(pViewItem->flags)) << "mid-backlog rebase boundary was not marked";
    EXPECT_GT(origDataOffset, 0u);

    // Simulate a session having consumed the boundary and advanced onto it (offset == length).
    pKinesisVideoStream->curViewItem.viewItem = *pViewItem;
    pKinesisVideoStream->curViewItem.offset = pViewItem->length;
    BOUNDARY_TEST_UNLOCK(pKinesisVideoStream, pKinesisVideoClient);

    EXPECT_EQ(STATUS_SUCCESS, resetCurrentViewItemStreamStart(pKinesisVideoStream));

    // The boundary must be intact: both markers still set, header still present, length unchanged. This is the
    // call that used to arm the 4004, and it must be a no-op for a genuine base boundary.
    BOUNDARY_TEST_LOCK(pKinesisVideoStream, pKinesisVideoClient);
    EXPECT_EQ(STATUS_SUCCESS, contentViewGetItemAt(pKinesisVideoStream->pView, boundaryIndex, &pViewItem));
    EXPECT_TRUE(CHECK_ITEM_STREAM_START(pViewItem->flags)) << "genuine boundary stream-start marker was stripped";
    EXPECT_TRUE(CHECK_ITEM_TIMECODE_BASE_START(pViewItem->flags)) << "boundary marker was cleared";
    EXPECT_EQ(origLength, pViewItem->length) << "boundary header was stripped (item shrank)";
    EXPECT_EQ(origDataOffset, GET_ITEM_DATA_OFFSET(pViewItem->flags)) << "boundary header offset changed";
    BOUNDARY_TEST_UNLOCK(pKinesisVideoStream, pKinesisVideoClient);

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));
}

//
// The reconnect fix-up adds an EBML header (and ITEM_FLAG_STREAM_START) to an item that is NOT a timecode
// boundary. That header MUST remain strippable by resetCurrentViewItemStreamStart(), otherwise a replay
// would emit two headers. Confirm the fix-up header carries no boundary marker and is stripped as before,
// so the fix does not over-reach.
//
TEST_F(StreamStartBoundaryFunctionalityTest, FixupHeaderHasNoBoundaryMarkerAndIsStripped)
{
    PViewItem pViewItem = NULL;
    UINT64 tailIndex, fixupIndex = INVALID_VIEW_INDEX_VALUE, idx;
    UINT32 origLength;
    PKinesisVideoStream pKinesisVideoStream;
    PKinesisVideoClient pKinesisVideoClient;

    CreateScenarioTestClient();
    CreateStreamSync();
    MockProducer mockProducer(mMockProducerConfig, mStreamHandle);

    // Put two full fragments so there is a fragment-start item that is NOT the initial stream start.
    for (UINT32 i = 0; i < 2 * mMockProducerConfig.mKeyFrameInterval; i++) {
        EXPECT_EQ(STATUS_SUCCESS, mockProducer.putFrame(FALSE));
    }

    pKinesisVideoStream = FROM_STREAM_HANDLE(mStreamHandle);
    pKinesisVideoClient = pKinesisVideoStream->pKinesisVideoClient;

    pKinesisVideoClient->clientCallbacks.lockMutexFn(pKinesisVideoClient->clientCallbacks.customData, pKinesisVideoStream->base.lock);

    // Find the start of the second fragment: a fragment-start item that is neither a stream start nor a
    // boundary. This models an item that the reconnect fix-up will prepend a header onto.
    EXPECT_EQ(STATUS_SUCCESS, contentViewGetTail(pKinesisVideoStream->pView, &pViewItem));
    tailIndex = pViewItem->index;
    for (idx = tailIndex; contentViewGetItemAt(pKinesisVideoStream->pView, idx, &pViewItem) == STATUS_SUCCESS; idx++) {
        if (CHECK_ITEM_FRAGMENT_START(pViewItem->flags) && !CHECK_ITEM_STREAM_START(pViewItem->flags)) {
            fixupIndex = idx;
            break;
        }
    }
    ASSERT_NE(INVALID_VIEW_INDEX_VALUE, fixupIndex) << "could not find a non-boundary fragment-start item";

    EXPECT_EQ(STATUS_SUCCESS, contentViewGetItemAt(pKinesisVideoStream->pView, fixupIndex, &pViewItem));
    origLength = pViewItem->length;
    EXPECT_FALSE(CHECK_ITEM_STREAM_START(pViewItem->flags));
    EXPECT_FALSE(CHECK_ITEM_TIMECODE_BASE_START(pViewItem->flags));

    // Point the content view at that item so the fix-up operates on it. Clear curViewItem so the fix-up's
    // internal reset is a no-op on entry.
    EXPECT_EQ(STATUS_SUCCESS, contentViewSetCurrentIndex(pKinesisVideoStream->pView, fixupIndex));
    MEMSET(&pKinesisVideoStream->curViewItem, 0x00, SIZEOF(CurrentViewItem));
    pKinesisVideoStream->curViewItem.viewItem.handle = INVALID_ALLOCATION_HANDLE_VALUE;
    pKinesisVideoClient->clientCallbacks.unlockMutexFn(pKinesisVideoClient->clientCallbacks.customData, pKinesisVideoStream->base.lock);

    // Reconnect fix-up prepends an EBML header + ITEM_FLAG_STREAM_START, but NOT the boundary marker.
    EXPECT_EQ(STATUS_SUCCESS, streamStartFixupOnReconnect(pKinesisVideoStream));

    pKinesisVideoClient->clientCallbacks.lockMutexFn(pKinesisVideoClient->clientCallbacks.customData, pKinesisVideoStream->base.lock);
    EXPECT_EQ(STATUS_SUCCESS, contentViewGetItemAt(pKinesisVideoStream->pView, fixupIndex, &pViewItem));
    EXPECT_TRUE(CHECK_ITEM_STREAM_START(pViewItem->flags)) << "fix-up did not add a stream-start header";
    EXPECT_FALSE(CHECK_ITEM_TIMECODE_BASE_START(pViewItem->flags)) << "fix-up header must not be a boundary";
    EXPECT_GT(pViewItem->length, origLength) << "fix-up did not grow the item by a header";
    EXPECT_GT(GET_ITEM_DATA_OFFSET(pViewItem->flags), 0u);

    // Simulate advancing past the fixed-up item, then reset it.
    pKinesisVideoStream->curViewItem.viewItem = *pViewItem;
    pKinesisVideoStream->curViewItem.offset = pViewItem->length;
    pKinesisVideoClient->clientCallbacks.unlockMutexFn(pKinesisVideoClient->clientCallbacks.customData, pKinesisVideoStream->base.lock);

    EXPECT_EQ(STATUS_SUCCESS, resetCurrentViewItemStreamStart(pKinesisVideoStream));

    // The fix-up header must be stripped: marker cleared, item back to its original length.
    pKinesisVideoClient->clientCallbacks.lockMutexFn(pKinesisVideoClient->clientCallbacks.customData, pKinesisVideoStream->base.lock);
    EXPECT_EQ(STATUS_SUCCESS, contentViewGetItemAt(pKinesisVideoStream->pView, fixupIndex, &pViewItem));
    EXPECT_FALSE(CHECK_ITEM_STREAM_START(pViewItem->flags)) << "fix-up header was not stripped";
    EXPECT_EQ(origLength, pViewItem->length) << "fix-up header strip did not restore length";
    EXPECT_EQ(0u, GET_ITEM_DATA_OFFSET(pViewItem->flags));
    pKinesisVideoClient->clientCallbacks.unlockMutexFn(pKinesisVideoClient->clientCallbacks.customData, pKinesisVideoStream->base.lock);

    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&mStreamHandle));
}

#endif
