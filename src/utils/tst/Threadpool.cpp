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
    EXPECT_EQ(STATUS_SUCCESS, threadpoolCreate(&pThreadpool, 0, 1));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolPush(pThreadpool, exitOnTeardownTask, &terminate));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolPush(pThreadpool, exitOnTeardownTask, &terminate));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolPush(pThreadpool, exitOnTeardownTask, &terminate));
    EXPECT_EQ(STATUS_SUCCESS, threadpoolTotalThreadCount(pThreadpool, &count));
    EXPECT_EQ(count, 1);
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
    UINT32 min = rand()%(max/2);
    EXPECT_EQ(STATUS_SUCCESS, threadpoolCreate(&pThreadpool, min, max));
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
    UINT32 min = rand()%(max/2);
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

PVOID createTasks(PVOID customData) {
    PThreadpool pThreadpool = (PThreadpool)customData;
    auto iterations = rand()%20;

    for(auto i = 0; i < iterations; i++) {
        THREAD_SLEEP((rand()%5 + 1) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        //allowed to fail as we may delete the threadpool early
        threadpoolPush(pThreadpool, randomishTask, NULL);
    }
    return 0;
}

TEST_F(ThreadpoolFunctionalityTest, MultithreadUseTest)
{
    PThreadpool pThreadpool = NULL;
    UINT32 count = 0;
    BOOL terminate = FALSE;
    srand(GETTIME());
    const UINT32 max = 10;
    UINT32 min = rand()%(max/2);
    TID thread1, thread2;
    EXPECT_EQ(STATUS_SUCCESS, threadpoolCreate(&pThreadpool, min, max));
    THREAD_CREATE(&thread1, createTasks, pThreadpool);
    THREAD_CREATE(&thread2, createTasks, pThreadpool);

    THREAD_SLEEP((rand()%50 + 50) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    EXPECT_EQ(STATUS_SUCCESS, threadpoolFree(pThreadpool));

    //wait for threads to exit before test ends to avoid false memory leak alarm
    THREAD_SLEEP(250 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}
