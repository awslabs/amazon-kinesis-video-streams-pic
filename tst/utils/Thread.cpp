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
        DLOGE("Arguments passed to this shouldn't be null!");
        return NULL;
    }

    MUTEX_LOCK(gThreadMutex);
    gThreadCount++;
    struct sleep_times* st = (struct sleep_times*) arg;

    // Mark as visited
    st->threadVisited = TRUE;
    UINT64 sleepTime = st->threadSleepTime;

    MUTEX_UNLOCK(gThreadMutex);

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
    ThreadParams thread_params[TEST_THREAD_COUNT];

    gThreadCount = 0;

    // Create the threads
    for (index = 0; index < TEST_THREAD_COUNT; index++) {
        st[index].threadVisited = FALSE;
        st[index].threadCleared = FALSE;
        st[index].threadSleepTime = index * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        thread_params[index].version = 0;
        thread_params[index].stackSize = threadStack;
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE_WITH_PARAMS(&threads[index], &thread_params[index], testThreadRoutine, (PVOID) &st[index]));
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
    ThreadParams threadParams;
    threadParams.version = 0;
    threadParams.stackSize = 0;

    // 0 passed into the size parameter means to use the defaults.
    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE_WITH_PARAMS(&threadId, &threadParams, emptyRoutine, NULL));
    EXPECT_NE(0, threadId);
    EXPECT_EQ(STATUS_SUCCESS, THREAD_JOIN(threadId, NULL));
}

TEST_F(ThreadFunctionalityTest, ThreadCreateWithParamsNegativeTest)
{
    TID threadId = 0;
    SIZE_T threadStack = 512 * 1024; // 0.5 MiB
    ThreadParams threadParams;
    threadParams.version = 0;
    threadParams.stackSize = threadStack;
    STATUS threadCreateStatus = STATUS_SUCCESS;

    // No out value case
    EXPECT_EQ(STATUS_NULL_ARG, THREAD_CREATE_WITH_PARAMS(NULL, &threadParams, emptyRoutine, NULL));

    // Request too large stack size case, behavior differs on platforms
    // On Mac, returns STATUS_THREAD_ATTR_SET_STACK_SIZE_FAILED
    // On Ubuntu Linux, returns STATUS_THREAD_INVALID_ARG
    // On Windows, returns STATUS_CREATE_THREAD_FAILED
    threadParams.stackSize = SIZE_MAX;
    threadCreateStatus = THREAD_CREATE_WITH_PARAMS(&threadId, &threadParams, emptyRoutine, NULL);
    EXPECT_TRUE(threadCreateStatus == STATUS_THREAD_ATTR_SET_STACK_SIZE_FAILED || threadCreateStatus == STATUS_THREAD_INVALID_ARG ||
                threadCreateStatus == STATUS_CREATE_THREAD_FAILED);
    EXPECT_EQ(0, threadId);

    // No out value case
    EXPECT_EQ(STATUS_NULL_ARG, THREAD_CREATE(NULL, emptyRoutine, NULL));

    // Invalid version
    threadParams.version = THREAD_PARAMS_CURRENT_VERSION + 100;
    EXPECT_EQ(STATUS_INVALID_THREAD_PARAMS_VERSION, THREAD_CREATE_WITH_PARAMS(&threadId, &threadParams, emptyRoutine, NULL));
    EXPECT_EQ(0, threadId);

    // NULL params
    EXPECT_EQ(STATUS_NULL_ARG, THREAD_CREATE_WITH_PARAMS(&threadId, NULL, emptyRoutine, NULL));
    EXPECT_EQ(0, threadId);
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

    if (arg == NULL) {
        DLOGE("fetchStackSizeThreadRoutine requires a TestThreadInfo* as an argument!");
        return NULL;
    }

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
    TID halfMiBThreadId = 0;
    SIZE_T halfMiBThreadStackSize = 512 * 1024; // 0.5 MiB
    TestThreadInfo halfMiBThreadInfo;
    halfMiBThreadInfo.stackSize = 0;
    halfMiBThreadInfo.failure = 0;
    ThreadParams halfMiBThreadParams;
    halfMiBThreadParams.version = 0;
    halfMiBThreadParams.stackSize = halfMiBThreadStackSize;

    TID oneMiBThreadId = 0;
    SIZE_T oneMiBThreadStackSize = 1024 * 1024; // 1 MiB
    TestThreadInfo oneMiBThreadInfo;
    oneMiBThreadInfo.stackSize = 0;
    oneMiBThreadInfo.failure = 0;
    ThreadParams oneMiBThreadParams;
    oneMiBThreadParams.version = 0;
    oneMiBThreadParams.stackSize = oneMiBThreadStackSize;

    // Check case 1: 0.5 MiB
    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE_WITH_PARAMS(&halfMiBThreadId, &halfMiBThreadParams, fetchStackSizeThreadRoutine, &halfMiBThreadInfo));
    EXPECT_NE(0, halfMiBThreadId);
    EXPECT_EQ(STATUS_SUCCESS, THREAD_JOIN(halfMiBThreadId, NULL));
    EXPECT_EQ(0, halfMiBThreadInfo.failure);
    EXPECT_EQ(halfMiBThreadStackSize, halfMiBThreadInfo.stackSize);

    // Check case 2: 1 MiB
    EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE_WITH_PARAMS(&oneMiBThreadId, &oneMiBThreadParams, fetchStackSizeThreadRoutine, &oneMiBThreadInfo));
    EXPECT_NE(0, oneMiBThreadId);
    EXPECT_EQ(STATUS_SUCCESS, THREAD_JOIN(oneMiBThreadId, NULL));
    EXPECT_EQ(0, oneMiBThreadInfo.failure);
    EXPECT_EQ(oneMiBThreadStackSize, oneMiBThreadInfo.stackSize);
}

#endif
