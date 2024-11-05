#include "UtilTestFixture.h"

class ThreadFunctionalityTest : public UtilTestBase {};

#define TEST_THREAD_COUNT 500

MUTEX gThreadMutex;
UINT64 gThreadCount;

struct sleep_times {
    BOOL threadSleepTime;
    BOOL threadVisited;
    BOOL threadCleared;
};

PVOID testThreadRoutine(PVOID arg)
{
    if (arg == NULL) {
        FAIL() << "TestThreadRoutine requires args!";
    }

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

PVOID emptyRoutine(PVOID arg)
{
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
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threads[index], testThreadRoutine, (PVOID) &st[index]));
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

        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threads[index], testThreadRoutine, (PVOID) &st[index]));
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
    SIZE_T threadStack = 64 * 1024;
    struct sleep_times st[TEST_THREAD_COUNT];

    gThreadCount = 0;

    // Create the threads
    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        st[index].threadVisited = FALSE;
        st[index].threadCleared = FALSE;
        st[index].threadSleepTime = index * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE_WITH_PARAMS(&threads[index], testThreadRoutine, threadStack, (PVOID) &st[index]));
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

TEST_F(ThreadFunctionalityTest, ThreadCreateUseDefaultsTest)
{
    TID threadId = 0;

    // 0 passed into the size parameter means to use the defaults.
    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE_WITH_PARAMS(&threadId, emptyRoutine, 0, NULL));
    EXPECT_NE(0, threadId);
    EXPECT_EQ(STATUS_SUCESSS, THREAD_JOIN(threadId, NULL))
}

TEST_F(ThreadFunctionalityTest, NegativeTest)
{
    TID threadId = 0;
    SIZE_T threadStack = 512 * 1024; // 0.5 MiB

    // No out value case
    EXPECT_NE(STATUS_SUCCESS, THREAD_CREATE_WITH_PARAMS(NULL, emptyRoutine, threadStack, NULL));

    // Request too large stack size case
    EXPECT_NE(STATUS_SUCCESS, THREAD_CREATE_WITH_PARAMS(&threadId, emptyRoutine, SIZE_MAX, NULL));
    EXPECT_EQ(0, threadId);

    // No out value case
    EXPECT_NE(STATUS_SUCCESS, THREAD_CREATE(NULL, testThreadRoutine, NULL));
}

// Linux-only test
#if !defined _WIN32 && !defined _WIN64 && !defined __CYGWIN__ && !defined __APPLE__ && !defined __MACH__

// Struct to hold the stack size information for passing back to main
typedef struct {
    SIZE_T stackSize;
    INT32 failure;
} TestThreadInfo;

// Function to retrieve and print the stack size from within the thread
PVOID fetchStackSizeThreadRoutine(PVOID arg)
{
    pthread_attr_t attr;
    SIZE_T stackSize;
    INT32 result = 0;
    TestThreadInfo* pThreadInfo = (TestThreadInfo*) arg;

    // Initialize the thread attributes for the running thread
    result = pthread_getattr_np(pthread_self(), &attr); // Linux-specific function
    if (result != 0) {
        goto CleanUp;
    }

    // Retrieve the stack size from the thread attributes
    result = pthread_attr_getstacksize(&attr, &stackSize);
    if (result != 0) {
        goto CleanUp;
    }

    pThreadInfo->stackSize = stackSize;

CleanUp:
    pThreadInfo->failure = result;
    pthread_attr_destroy(&attr);

    return NULL;
}

// Create a thread with the min stack size + 512 KiB
// Then check that the thread has the requested size.
TEST_F(ThreadFunctionalityTest, VerifyStackSize)
{
    TID threadId;
    SIZE_T threadStack = 512 * 1024; // 0.5 MiB

    TestThreadInfo threadInfo = {.stackSize = 0, .failure = 0};

    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE_WITH_PARAMS(&threadId, fetchStackSizeThreadRoutine, threadStack, &threadInfo));

    EXPECT_EQ(STATUS_SUCCESS, THREAD_JOIN(threadId, NULL));

    EXPECT_EQ(0, threadInfo.failure);
    EXPECT_EQ(threadStack, threadInfo.stackSize);
}

#endif
