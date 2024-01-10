#include "UtilTestFixture.h"

class ThreadFunctionalityTest : public UtilTestBase {
};

#define TEST_THREAD_COUNT       500

MUTEX gThreadMutex;
UINT64 gThreadCount;

struct sleep_times {
    BOOL threadSleepTime;
    BOOL threadVisited;
    BOOL threadCleared;
};

PVOID testThreadRoutine(PVOID arg)
{
    MUTEX_LOCK(gThreadMutex);
    gThreadCount++;
    MUTEX_UNLOCK(gThreadMutex);
    struct sleep_times* st = (struct sleep_times*) arg;

    // Mark as visited
    st->threadVisited = TRUE;

    UINT64 sleepTime = st->threadSleepTime;
    // Just sleep for some time
    THREAD_SLEEP(sleepTime);

    // Mark as cleared
    st->threadCleared = TRUE;

    MUTEX_LOCK(gThreadMutex);
    gThreadCount--;
    MUTEX_UNLOCK(gThreadMutex);
    return NULL;
}

TEST_F(ThreadFunctionalityTest, ThreadCreateAndReleaseSimpleCheck)
{
    UINT64 index;
    TID threads[TEST_THREAD_COUNT];
    gThreadMutex = MUTEX_CREATE(FALSE);
    struct sleep_times st[TEST_THREAD_COUNT];

    // Create the threads
    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        st[index].threadVisited = FALSE;
        st[index].threadCleared = FALSE;
        st[index].threadSleepTime = index * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threads[index], testThreadRoutine, (PVOID)&st[index]));
    }

    // Await for the threads to finish
    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        EXPECT_EQ(STATUS_SUCCESS, THREAD_JOIN(threads[index], NULL));
    }

    MUTEX_LOCK(gThreadMutex);
    EXPECT_EQ(0, gThreadCount);
    MUTEX_UNLOCK(gThreadMutex);

    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        EXPECT_TRUE(st[index].threadVisited) << "Thread didn't visit index " << index;
        EXPECT_TRUE(st[index].threadCleared) << "Thread didn't clear index " << index;
    }

    MUTEX_FREE(gThreadMutex);
}

TEST_F(ThreadFunctionalityTest, ThreadCreateAndCancel)
{
    UINT64 index;
    TID threads[TEST_THREAD_COUNT];
    gThreadMutex = MUTEX_CREATE(FALSE);
    struct sleep_times st[TEST_THREAD_COUNT];

    // Create the threads
    for (index = 0; index < TEST_THREAD_COUNT; index++) {

        st[index].threadVisited = FALSE;
        st[index].threadCleared = FALSE;
        // Long sleep
        st[index].threadSleepTime = 20 * HUNDREDS_OF_NANOS_IN_A_SECOND;

        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threads[index], testThreadRoutine, (PVOID)&st[index]));
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
    MUTEX_LOCK(gThreadMutex);
    EXPECT_EQ(TEST_THREAD_COUNT, gThreadCount);
    MUTEX_UNLOCK(gThreadMutex);

    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        EXPECT_TRUE(st[index].threadVisited) << "Thread didn't visit index " << index;
        EXPECT_FALSE(st[index].threadCleared) << "Thread shouldn't have cleared index " << index;
    }

    MUTEX_FREE(gThreadMutex);
}
