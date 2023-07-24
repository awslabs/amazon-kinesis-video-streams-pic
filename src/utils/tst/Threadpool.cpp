#include "UtilTestFixture.h"
#include <thread>
#include <chrono>

class ThreadpoolFunctionalityTest : public UtilTestBase {
};

PVOID randomishTask(PVOID customData) {
    THREAD_SLEEP((rand()%100 + 1) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    return 0;
}

PVOID exitOnTeardownTask(PVOID customData) {
    BOOL * pTerminate = (BOOL*)customData;
    while(!*pTerminate) {
        THREAD_SLEEP(20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    return 0;
}

TEST_F(ThreadpoolFunctionalityTest, CreateDestroyTest)
{
    PThreadpool pThreadpool = NULL;
    EXPECT_EQ(STATUS_SUCCESS, threadpoolCreate(&pThreadpool, 1, 1));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolFree(pThreadpool));

    //wait for threads to exit before test ends
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_EQ(STATUS_SUCCESS, threadpoolCreate(&pThreadpool, 1, 2));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolFree(pThreadpool));

    //wait for threads to exit before test ends
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_EQ(STATUS_SUCCESS, threadpoolCreate(&pThreadpool, 1, 1));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolFree(pThreadpool));

    //wait for threads to exit before test ends
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(ThreadpoolFunctionalityTest, BasicTryAddTest)
{
    PThreadpool pThreadpool = NULL;
    BOOL terminate = FALSE;
    srand(GETTIME());
    EXPECT_EQ(STATUS_SUCCESS, threadpoolCreate(&pThreadpool, 1, 1));

    //sleep for a little. Create asynchronously detaches threads, so using TryAdd too soon can
    //fail if the threads are not yet ready.
    THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_EQ(STATUS_SUCCESS, threadpoolTryAdd(pThreadpool, exitOnTeardownTask, &terminate));
    EXPECT_EQ(STATUS_THREADPOOL_MAX_COUNT, threadpoolTryAdd(pThreadpool, exitOnTeardownTask, &terminate));
    terminate = TRUE;
    EXPECT_EQ(STATUS_SUCCESS, threadpoolFree(pThreadpool));

    //wait for threads to exit before test ends
    THREAD_SLEEP(300 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(ThreadpoolFunctionalityTest, BasicPushTest)
{
    PThreadpool pThreadpool = NULL;
    BOOL terminate = FALSE;
    UINT32 count = 0;
    srand(GETTIME());
    EXPECT_EQ(STATUS_SUCCESS, threadpoolCreate(&pThreadpool, 1, 2));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolPush(pThreadpool, exitOnTeardownTask, &terminate));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolPush(pThreadpool, exitOnTeardownTask, &terminate));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolPush(pThreadpool, exitOnTeardownTask, &terminate));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolTotalThreadCount(pThreadpool, &count));
    EXPECT_EQ(count, 2);
    terminate = TRUE;
    EXPECT_EQ(STATUS_SUCCESS, threadpoolFree(pThreadpool));

    //wait for threads to exit before test ends
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(ThreadpoolFunctionalityTest, GetThreadCountTest)
{
    PThreadpool pThreadpool = NULL;
    UINT32 count = 0;
    BOOL terminate = FALSE;
    srand(GETTIME());
    const UINT32 max = 10;
    //accepted race condition where min is 1, threadpoolPush can create a new thread
    //before the first thread is ready to accept tasks
    UINT32 min = rand()%(max/2) + 2;
    EXPECT_EQ(STATUS_SUCCESS, threadpoolCreate(&pThreadpool, min, max));
    //let threads get ready so new threads aren't created for this task
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    EXPECT_EQ(STATUS_SUCCESS, threadpoolTotalThreadCount(pThreadpool, &count));
    EXPECT_EQ(count, min);

    for(UINT32 i = 0; i < min; i++) {
        EXPECT_EQ(STATUS_SUCCESS, threadpoolPush(pThreadpool, exitOnTeardownTask, &terminate));
    }
    //no new threads created
    EXPECT_EQ(STATUS_SUCCESS, threadpoolTotalThreadCount(pThreadpool, &count));
    EXPECT_EQ(count, min);

    //another thread needed
    EXPECT_EQ(STATUS_SUCCESS, threadpoolPush(pThreadpool, exitOnTeardownTask, &terminate));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolTotalThreadCount(pThreadpool, &count));
    EXPECT_EQ(count, min+1);

    terminate = TRUE;
    EXPECT_EQ(STATUS_SUCCESS, threadpoolFree(pThreadpool));

    //wait for threads to exit before test ends
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(ThreadpoolFunctionalityTest, ThreadsExitGracefullyAfterThreadpoolFreeTest)
{
    PThreadpool pThreadpool = NULL;
    UINT32 count = 0;
    BOOL terminate = FALSE;
    srand(GETTIME());
    const UINT32 max = 10;
    UINT32 min = rand()%(max/2) + 2;
    EXPECT_EQ(STATUS_SUCCESS, threadpoolCreate(&pThreadpool, min, max));
    for(UINT32 i = 0; i < min; i++) {
        EXPECT_EQ(STATUS_SUCCESS, threadpoolPush(pThreadpool, exitOnTeardownTask, &terminate));
    }
    for(UINT32 i = min; i < max; i++) {
        EXPECT_EQ(STATUS_SUCCESS, threadpoolPush(pThreadpool, randomishTask, NULL));
    }
    EXPECT_EQ(STATUS_SUCCESS, threadpoolFree(pThreadpool));
    //now threads will exit after threadpoolFree
    terminate = TRUE;

    //wait for threads to exit before test ends
    THREAD_SLEEP(150 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

typedef struct ThreadpoolUser {
    PThreadpool pThreadpool;
    volatile ATOMIC_BOOL usable;
};

PVOID createTasks(PVOID customData) {
    ThreadpoolUser * user = (ThreadpoolUser*)customData;
    PThreadpool pThreadpool = user->pThreadpool;
    auto iterations = rand()%20;

    for(auto i = 0; i < iterations; i++) {
        THREAD_SLEEP((rand()%5 + 1) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        //allowed to fail as we may delete the threadpool early
        if (ATOMIC_LOAD_BOOL(&user->usable)) {
            if (threadpoolPush(pThreadpool, randomishTask, NULL) != STATUS_SUCCESS) {
                break;
            }
        }
        else {
            break;
        }
    }
    return 0;
}

TEST_F(ThreadpoolFunctionalityTest, MultithreadUseTest)
{
    PThreadpool pThreadpool = NULL;
    ThreadpoolUser user;
    UINT32 count = 0;
    BOOL terminate = FALSE;
    srand(GETTIME());
    const UINT32 max = 10;
    UINT32 min = rand()%(max/2) + 2;
    TID thread1, thread2;
    EXPECT_EQ(STATUS_SUCCESS, threadpoolCreate(&pThreadpool, min, max));
    user.pThreadpool = pThreadpool;
    ATOMIC_STORE_BOOL(&user.usable, TRUE);
    THREAD_CREATE(&thread1, createTasks, &user);
    THREAD_CREATE(&thread2, createTasks, &user);

    THREAD_SLEEP((rand()%50 + 50) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    ATOMIC_STORE_BOOL(&user.usable, FALSE);
    EXPECT_EQ(STATUS_SUCCESS, threadpoolFree(pThreadpool));

    //wait for threads to exit before test ends to avoid false memory leak alarm
    THREAD_SLEEP(250 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}
