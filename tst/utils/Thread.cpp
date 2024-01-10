#include "UtilTestFixture.h"

class ThreadFunctionalityTest : public UtilTestBase {
};

#define TEST_THREAD_COUNT       500

MUTEX gThreadMutex;

struct sleep_times {
    BOOL threadSleepTimes[TEST_THREAD_COUNT];
    BOOL threadVisited[TEST_THREAD_COUNT];
    BOOL threadCleared[TEST_THREAD_COUNT];
    UINT64 index;
    UINT64 threadCount;
};

PVOID testThreadRoutine(PVOID arg)
{
    MUTEX_LOCK(gThreadMutex);
    struct sleep_times* st = (struct sleep_times*) arg;
    st->threadCount++;

    // Mark as visited
    st->threadVisited[st->index] = TRUE;

    // Just sleep for some time
    THREAD_SLEEP(st->threadSleepTimes[st->index]);

    // Mark as cleared
    st->threadCleared[st->index] = TRUE;

    st->threadCount--;
    MUTEX_UNLOCK(gThreadMutex);

    return NULL;
}

TEST_F(ThreadFunctionalityTest, ThreadCreateAndReleaseSimpleCheck)
{
    UINT64 index;
    TID threads[TEST_THREAD_COUNT];
    gThreadMutex = MUTEX_CREATE(FALSE);
    struct sleep_times st;

    // Create the threads
    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        st.threadVisited[index] = FALSE;
        st.threadCleared[index] = FALSE;
        st.threadSleepTimes[index] = index * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threads[index], testThreadRoutine, (PVOID)&st));
    }

    // Await for the threads to finish
    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        EXPECT_EQ(STATUS_SUCCESS, THREAD_JOIN(threads[index], NULL));
    }

    EXPECT_EQ(0, st.threadCount);

    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        EXPECT_TRUE(st.threadVisited[index]) << "Thread didn't visit index " << index;
        EXPECT_TRUE(st.threadCleared[index]) << "Thread didn't clear index " << index;
    }

    MUTEX_FREE(gThreadMutex);
}

TEST_F(ThreadFunctionalityTest, ThreadCreateAndCancel)
{
    UINT64 index;
    TID threads[TEST_THREAD_COUNT];
    gThreadMutex = MUTEX_CREATE(FALSE);
    struct sleep_times st;

    // Create the threads
    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        st.threadVisited[index] = FALSE;
        st.threadCleared[index] = FALSE;
        // Long sleep
        st.threadSleepTimes[index] = 20 * HUNDREDS_OF_NANOS_IN_A_SECOND;
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threads[index], testThreadRoutine, (PVOID)&st));
#if !(defined _WIN32 || defined _WIN64 || defined __CYGWIN__)
        // We should detach thread for non-windows platforms only
        // Windows implementation would cancel the handle and the
        // consequent thread cancel would fail
        EXPECT_EQ(STATUS_SUCCESS, THREAD_DETACH(threads[index]));
#endif
    }

    // wait 2 seconds to make sure all threads are executed
    THREAD_SLEEP(2 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Cancel all the threads
    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CANCEL(threads[index]));
    }

    // Validate that threads have been killed and didn't finish successfully
    EXPECT_EQ(TEST_THREAD_COUNT, st.threadCount);

    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        EXPECT_TRUE(st.threadVisited[index]) << "Thread didn't visit index " << index;
        EXPECT_FALSE(st.threadCleared[index]) << "Thread shouldn't have cleared index " << index;
    }

    MUTEX_FREE(gThreadMutex);
}
