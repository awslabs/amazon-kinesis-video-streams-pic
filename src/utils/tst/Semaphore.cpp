#include "UtilTestFixture.h"

class SemaphoreFunctionalityTest : public UtilTestBase {
public:
    SemaphoreFunctionalityTest() :
            handle(INVALID_SEMAPHORE_HANDLE_VALUE) {
        ATOMIC_STORE_BOOL(&acquired, FALSE);
        ATOMIC_STORE_BOOL(&shutdown, FALSE);
        ATOMIC_STORE(&threadCount, 0);
        ATOMIC_STORE(&drainThreadCount, 0);
    }

    volatile ATOMIC_BOOL acquired;
    volatile SIZE_T threadCount;
    volatile SIZE_T drainThreadCount;
    volatile ATOMIC_BOOL shutdown;
    SEMAPHORE_HANDLE handle;

    static PVOID acquireThreadRoutine(PVOID arg)
    {
        STATUS retStatus = STATUS_SUCCESS;
        SemaphoreFunctionalityTest* pThis = (SemaphoreFunctionalityTest*) arg;
        ATOMIC_INCREMENT(&pThis->threadCount);
        retStatus = semaphoreAcquire(pThis->handle, INFINITE_TIME_VALUE);
        if(ATOMIC_LOAD_BOOL(&pThis->shutdown)) {
            EXPECT_EQ(STATUS_SEMAPHORE_OPERATION_AFTER_SHUTDOWN, retStatus);
        }
        else {
            EXPECT_EQ(STATUS_SUCCESS, retStatus);
        }
        ATOMIC_DECREMENT(&pThis->threadCount);
        return NULL;
    }

    static PVOID drainThreadRoutine(PVOID arg)
    {
        SemaphoreFunctionalityTest* pThis = (SemaphoreFunctionalityTest*) arg;
        ATOMIC_INCREMENT(&pThis->drainThreadCount);
        EXPECT_EQ(STATUS_SUCCESS, semaphoreWaitUntilClear(pThis->handle, INFINITE_TIME_VALUE));
        ATOMIC_DECREMENT(&pThis->drainThreadCount);
        return NULL;
    }
};

TEST_F(SemaphoreFunctionalityTest, apiInputTest)
{
    PSemaphore pSemaphore;

    EXPECT_NE(STATUS_SUCCESS, semaphoreCreate(0, &handle));
    EXPECT_NE(STATUS_SUCCESS, semaphoreCreate(10, NULL));
    EXPECT_NE(STATUS_SUCCESS, semaphoreCreate(0, NULL));
    EXPECT_NE(STATUS_SUCCESS, semaphoreCreateInternal(0, &pSemaphore, FALSE));
    EXPECT_NE(STATUS_SUCCESS, semaphoreCreateInternal(10, NULL, FALSE));
    EXPECT_NE(STATUS_SUCCESS, semaphoreCreateInternal(0, NULL, FALSE));
    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_NE(STATUS_SUCCESS, semaphoreFree(NULL));

    for (UINT32 i = 1; i < 1000; i++) {
        EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(i, &handle));
        ATOMIC_STORE_BOOL(&shutdown, TRUE);
        EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));
    }

    // Call again - idempotent
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));

    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(1, &handle));
    EXPECT_NE(STATUS_SUCCESS, semaphoreAcquire(INVALID_SEMAPHORE_HANDLE_VALUE, 1));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 1));
    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));

    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(1, &handle));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 1));
    EXPECT_NE(STATUS_SUCCESS, semaphoreRelease(INVALID_SEMAPHORE_HANDLE_VALUE));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreRelease(handle));
    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));

    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(1, &handle));
    EXPECT_NE(STATUS_SUCCESS, semaphoreLock(INVALID_SEMAPHORE_HANDLE_VALUE));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreLock(handle));
    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));

    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(1, &handle));
    EXPECT_NE(STATUS_SUCCESS, semaphoreUnlock(INVALID_SEMAPHORE_HANDLE_VALUE));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreUnlock(handle));
    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));

    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(1, &handle));
    EXPECT_NE(STATUS_SUCCESS, semaphoreWaitUntilClear(INVALID_SEMAPHORE_HANDLE_VALUE, 1));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreWaitUntilClear(handle, 1));
    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));
}

TEST_F(SemaphoreFunctionalityTest, acquireBasicTest)
{
    // Basic ops
    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(1, &handle));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 0));
    EXPECT_EQ(STATUS_OPERATION_TIMED_OUT, semaphoreAcquire(handle, 0));
    EXPECT_EQ(STATUS_OPERATION_TIMED_OUT, semaphoreAcquire(handle, 1));
    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));

    // Failure after locked
    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(1, &handle));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreLock(handle));
    EXPECT_EQ(STATUS_SEMAPHORE_ACQUIRE_WHEN_LOCKED, semaphoreAcquire(handle, 0));
    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));
}

TEST_F(SemaphoreFunctionalityTest, releaseBasicTest)
{
    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(2, &handle));
    EXPECT_EQ(STATUS_INVALID_OPERATION, semaphoreRelease(handle));

    EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 0));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 0));
    EXPECT_EQ(STATUS_OPERATION_TIMED_OUT, semaphoreAcquire(handle, 0));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreRelease(handle));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 0));

    // Validate there is no interference with locked
    EXPECT_EQ(STATUS_SUCCESS, semaphoreLock(handle));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreRelease(handle));
    EXPECT_EQ(STATUS_SEMAPHORE_ACQUIRE_WHEN_LOCKED, semaphoreAcquire(handle, 0));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreUnlock(handle));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 0));

    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));
}

TEST_F(SemaphoreFunctionalityTest, acquireAwaitingTest)
{
    const UINT32 SEMAPHORE_TEST_THREAD_COUNT = 500;
    const UINT32 SEMAPHORE_TEST_DRAIN_THREAD_COUNT = 10;
    UINT32 i;
    TID threadId;

    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(SEMAPHORE_TEST_THREAD_COUNT, &handle));

    for (i = 0; i < SEMAPHORE_TEST_THREAD_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 0));
    }

    // Spin up a threads to await for the semaphore
    for (i = 0; i < SEMAPHORE_TEST_THREAD_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threadId, acquireThreadRoutine, (PVOID) this));
        EXPECT_EQ(STATUS_SUCCESS, THREAD_DETACH(threadId));
    }

    // Wait for a little
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_EQ(SEMAPHORE_TEST_THREAD_COUNT, (UINT32) ATOMIC_LOAD(&threadCount));

    // Wait for a little
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // Spin up the drain threads to await for the semaphore to release all
    for (i = 0; i < SEMAPHORE_TEST_DRAIN_THREAD_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threadId, drainThreadRoutine, (PVOID) this));
        EXPECT_EQ(STATUS_SUCCESS, THREAD_DETACH(threadId));
    }

    // Wait for a little
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // Should have the drain count equal
    EXPECT_EQ(SEMAPHORE_TEST_DRAIN_THREAD_COUNT, (UINT32) ATOMIC_LOAD(&drainThreadCount));

    // Release the existing ones
    for (i = 0; i < SEMAPHORE_TEST_THREAD_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, semaphoreRelease(handle));
    }

    // Wait for a little
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // All the threads should have existed
    EXPECT_EQ(0, ATOMIC_LOAD(&threadCount));
    // Should have the drain count equal
    EXPECT_EQ(SEMAPHORE_TEST_DRAIN_THREAD_COUNT, (UINT32) ATOMIC_LOAD(&drainThreadCount));

    // Release all of the ones the threads acquired
    for (i = 0; i < SEMAPHORE_TEST_THREAD_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, semaphoreRelease(handle));
    }

    // Wait for a little
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // All the threads should have existed
    EXPECT_EQ(0, ATOMIC_LOAD(&drainThreadCount));

    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));
}

TEST_F(SemaphoreFunctionalityTest, drainTimeoutTest)
{
    const UINT32 SEMAPHORE_TEST_THREAD_COUNT = 500;
    UINT32 i;
    TID threadId;

    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(SEMAPHORE_TEST_THREAD_COUNT, &handle));

    for (i = 0; i < SEMAPHORE_TEST_THREAD_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 0));
    }

    // Spin up a threads to await for the semaphore
    for (i = 0; i < SEMAPHORE_TEST_THREAD_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threadId, acquireThreadRoutine, (PVOID) this));
        EXPECT_EQ(STATUS_SUCCESS, THREAD_DETACH(threadId));
    }

    // Wait for a little
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_EQ(SEMAPHORE_TEST_THREAD_COUNT, (UINT32) ATOMIC_LOAD(&threadCount));

    // Wait for a little
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // Release the existing ones
    for (i = 0; i < SEMAPHORE_TEST_THREAD_COUNT - 1; i++) {
        EXPECT_EQ(STATUS_SUCCESS, semaphoreRelease(handle));
    }

    // Wait for a little
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // All the threads should have existed
    EXPECT_EQ(1, ATOMIC_LOAD(&threadCount));

    // Await for drain and timeout with some timeout and 0
    EXPECT_EQ(STATUS_OPERATION_TIMED_OUT, semaphoreWaitUntilClear(handle, 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND));
    EXPECT_EQ(STATUS_OPERATION_TIMED_OUT, semaphoreWaitUntilClear(handle, 0));

    // Release the remaining
    for (i = 0; i < SEMAPHORE_TEST_THREAD_COUNT + 1; i++) {
        EXPECT_EQ(STATUS_SUCCESS, semaphoreRelease(handle));
    }

    // Make sure drain await returns OK
    EXPECT_EQ(STATUS_SUCCESS, semaphoreWaitUntilClear(handle, 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND));

    // Make sure drain await returns OK with 0 wait
    EXPECT_EQ(STATUS_SUCCESS, semaphoreWaitUntilClear(handle, 0));

    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));
}

TEST_F(SemaphoreFunctionalityTest, freeWithoutReleaseAllTest)
{
    const UINT32 SEMAPHORE_TEST_COUNT = 500;
    UINT32 i;

    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(SEMAPHORE_TEST_COUNT, &handle));

    for (i = 0; i < SEMAPHORE_TEST_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 0));
    }

    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));
}

TEST_F(SemaphoreFunctionalityTest, freeWithoutReleaseAllThreadsTest)
{
    const UINT32 SEMAPHORE_TEST_THREAD_COUNT = 500;
    UINT32 i;
    TID threadId;

    EXPECT_EQ(STATUS_SUCCESS, semaphoreCreate(SEMAPHORE_TEST_THREAD_COUNT, &handle));

    for (i = 0; i < SEMAPHORE_TEST_THREAD_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 0));
    }

    // Spin up a threads to await for the semaphore
    for (i = 0; i < SEMAPHORE_TEST_THREAD_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threadId, acquireThreadRoutine, (PVOID) this));
        EXPECT_EQ(STATUS_SUCCESS, THREAD_DETACH(threadId));
    }

    // Wait for a little
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_EQ(SEMAPHORE_TEST_THREAD_COUNT, (UINT32) ATOMIC_LOAD(&threadCount));

    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));

    // Await for all of the threads to finish
    THREAD_SLEEP(300 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_EQ(0, (UINT32) ATOMIC_LOAD(&threadCount));
}

TEST_F(SemaphoreFunctionalityTest, emptySemaphoreAcquireBasicTest)
{
    EXPECT_EQ(STATUS_SUCCESS, semaphoreEmptyCreate(2, &handle));

    EXPECT_EQ(STATUS_OPERATION_TIMED_OUT, semaphoreAcquire(handle, 0));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreRelease(handle));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 0));

    // Validate there is no interference with locked
    EXPECT_EQ(STATUS_SUCCESS, semaphoreLock(handle));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreRelease(handle));
    EXPECT_EQ(STATUS_SEMAPHORE_ACQUIRE_WHEN_LOCKED, semaphoreAcquire(handle, 0));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreUnlock(handle));
    EXPECT_EQ(STATUS_SUCCESS, semaphoreAcquire(handle, 0));

    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));
}

TEST_F(SemaphoreFunctionalityTest, emptySemaphoreAcquireFreeTest)
{
    UINT32 i;
    TID threadId;
    EXPECT_EQ(STATUS_SUCCESS, semaphoreEmptyCreate(10, &handle));

    // Spin up a threads to await for the semaphore
    for (i = 0; i < 10; i++) {
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threadId, acquireThreadRoutine, (PVOID) this));
        EXPECT_EQ(STATUS_SUCCESS, THREAD_DETACH(threadId));
    }
    // wait for all threads to start
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    EXPECT_EQ(10, (UINT32) ATOMIC_LOAD(&threadCount));
    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));

    // wait for all threads to exit to avoid address santizer false alarm
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(SemaphoreFunctionalityTest, avoidLossySignalsTest) {
    UINT32 i;
    TID threadId;
    EXPECT_EQ(STATUS_SUCCESS, semaphoreEmptyCreate(100, &handle));

    // Spin up a threads to await for the semaphore, release immediately
    for (i = 0; i < 100; i++) {
        EXPECT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threadId, acquireThreadRoutine, (PVOID) this));
        EXPECT_EQ(STATUS_SUCCESS, THREAD_DETACH(threadId));
        EXPECT_EQ(STATUS_SUCCESS, semaphoreRelease(handle));
    }
    // wait for all threads to start
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    EXPECT_EQ(0, (UINT32) ATOMIC_LOAD(&threadCount));
    ATOMIC_STORE_BOOL(&shutdown, TRUE);
    EXPECT_EQ(STATUS_SUCCESS, semaphoreFree(&handle));

    // wait for all threads to start
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}
