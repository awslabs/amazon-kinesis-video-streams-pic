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
    struct sleep_times* st = (struct sleep_times*) arg;

    // Mark as visited
    st->threadVisited = TRUE;
    MUTEX_UNLOCK(gThreadMutex);


    UINT64 sleepTime = st->threadSleepTime;
    // Just sleep for some time
    THREAD_SLEEP(sleepTime);

    MUTEX_LOCK(gThreadMutex);

    // Mark as cleared
    st->threadCleared = TRUE;

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

    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        EXPECT_TRUE(st[index].threadVisited) << "Thread didn't visit index " << index;
        EXPECT_FALSE(st[index].threadCleared) << "Thread shouldn't have cleared index " << index;
    }

    MUTEX_UNLOCK(gThreadMutex);

    MUTEX_FREE(gThreadMutex);
}

TEST_F(ThreadFunctionalityTest, ThreadCreateAndReleaseSimpleCheckWithStack)
{
    UINT64 index;
    TID threads[TEST_THREAD_COUNT];
    gThreadMutex = MUTEX_CREATE(FALSE);
    srand(GETTIME());
    SIZE_T threadStack = 16 * 1024 + rand()%(200 * 1024);
    struct sleep_times st[TEST_THREAD_COUNT];

    gThreadCount = 0;

    // Create the threads
    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        st[index].threadVisited = FALSE;
        st[index].threadCleared = FALSE;
        st[index].threadSleepTime = index * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE_WITH_PARAMS(&threads[index], testThreadRoutine, threadStack, (PVOID)&st[index]));
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

TEST_F(ThreadFunctionalityTest, NegativeTest)
{
    UINT64 index;
    TID threads[TEST_THREAD_COUNT];
    gThreadMutex = MUTEX_CREATE(FALSE);
    SIZE_T threadStack = 16 * 1024;
    struct sleep_times st[TEST_THREAD_COUNT];

    gThreadCount = 0;
    EXPECT_NE(STATUS_SUCCESS, THREAD_CREATE_WITH_PARAMS(NULL, testThreadRoutine, threadStack, NULL));
    EXPECT_NE(STATUS_SUCCESS, THREAD_CREATE(NULL, testThreadRoutine, NULL));

    MUTEX_LOCK(gThreadMutex);
    EXPECT_EQ(0, gThreadCount);
    MUTEX_UNLOCK(gThreadMutex);

    MUTEX_FREE(gThreadMutex);
}
