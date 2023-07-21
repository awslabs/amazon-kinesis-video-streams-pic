#include "UtilTestFixture.h"
#include <chrono>
#include <thread>

class ThreadsafeBlockingQueueFunctionalityTest : public UtilTestBase {
};

TEST_F(ThreadsafeBlockingQueueFunctionalityTest, createDestroyTest)
{
    PSafeBlockingQueue pSafeQueue = NULL;
    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueCreate(&pSafeQueue));
    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueFree(pSafeQueue));
}

TEST_F(ThreadsafeBlockingQueueFunctionalityTest, createEnqueueDestroyTest)
{
    PSafeBlockingQueue pSafeQueue = NULL;
    UINT64 totalItems = 0;
    srand(GETTIME());
    totalItems = rand()%(UINT16_MAX) + 1;
    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueCreate(&pSafeQueue));
    for(UINT64 i = 0; i < totalItems; i++) {
        EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueEnqueue(pSafeQueue, i));
    }
    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueFree(pSafeQueue));
}

TEST_F(ThreadsafeBlockingQueueFunctionalityTest, queueIsEmptyTest)
{
    PSafeBlockingQueue pSafeQueue = NULL;
    UINT64 totalItems = 0, totalLoops = 0, item = 0;
    BOOL empty = FALSE;
    srand(GETTIME());
    totalItems = rand()%(UINT16_MAX) + 1;
    totalLoops = rand()%64;
    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueCreate(&pSafeQueue));
    for(UINT64 j = 0; j < totalLoops; j++) {
        EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueIsEmpty(pSafeQueue, &empty));
        EXPECT_TRUE(empty);
        for(UINT64 i = 0; i < totalItems; i++) {
            EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueEnqueue(pSafeQueue, i));
        }
        EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueIsEmpty(pSafeQueue, &empty));
        EXPECT_TRUE(!empty);
        EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueDequeue(pSafeQueue, &item));
        EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueIsEmpty(pSafeQueue, &empty));
        if(totalItems > 1) {
            EXPECT_TRUE(!empty);
        }
        else {
            EXPECT_TRUE(empty);
        }
        EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueClear(pSafeQueue, FALSE));
    }

    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueFree(pSafeQueue));
}

TEST_F(ThreadsafeBlockingQueueFunctionalityTest, queueCountCorrectTest)
{
    PSafeBlockingQueue pSafeQueue = NULL;
    UINT64 totalItems = 0, totalLoops = 0, item = 0, itemsToQueue = 0;;
    UINT32 items = 0;
    BOOL empty = FALSE;
    srand(GETTIME());
    totalLoops = rand()%64;
    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueCreate(&pSafeQueue));
    //queue and dequeue a random number of items in a loop, and check the value
    for(UINT64 j = 0; j < totalLoops; j++) {
        itemsToQueue = rand()%(UINT16_MAX-totalItems) + 1;
        for(UINT64 i = 0; i < itemsToQueue; i++) {
            EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueEnqueue(pSafeQueue, i));
        }

        totalItems += itemsToQueue;
        EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueGetCount(pSafeQueue, &items));
        EXPECT_EQ(totalItems, items);

        itemsToQueue = rand()%(totalItems);
        for(UINT64 i = 0; i < itemsToQueue; i++) {
            EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueDequeue(pSafeQueue, &item));
        }

        totalItems -= itemsToQueue;
        EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueGetCount(pSafeQueue, &items));
        EXPECT_EQ(totalItems, items);
    }
    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueFree(pSafeQueue));
}
#define STATIC_NUMBER_OF_ITEMS 50

typedef struct SafeQueueUser {
    PSafeBlockingQueue pSafeQueue;
    volatile ATOMIC_BOOL usable;
};

void* writingThread(void* ptr) {
    SafeQueueUser * user = (SafeQueueUser*)ptr;
    PSafeBlockingQueue pSafeQueue = user->pSafeQueue;

    for(int i = 0; i < STATIC_NUMBER_OF_ITEMS; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(rand()%50));
        if(ATOMIC_LOAD_BOOL(&user->usable)) {
            safeBlockingQueueEnqueue(pSafeQueue, i);
        }
        else {
            break;
        }
    }
    return 0;
}

void* readingThread(void* ptr) {
    SafeQueueUser * user = (SafeQueueUser*)ptr;
    PSafeBlockingQueue pSafeQueue = user->pSafeQueue;
    UINT64 item = 0;

    for(int i = 0; i < STATIC_NUMBER_OF_ITEMS; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(rand()%50));
        if(ATOMIC_LOAD_BOOL(&user->usable)) {
            if(safeBlockingQueueDequeue(pSafeQueue, &item) != STATUS_SUCCESS) {
                break;
            }
        }
        else {
            break;
        }
    }
    return 0;
}

TEST_F(ThreadsafeBlockingQueueFunctionalityTest, multithreadQueueDequeueTest)
{
    PSafeBlockingQueue pSafeQueue = NULL;
    SafeQueueUser user;
    UINT64 totalThreads = 0, item = 0, threadCount = 0;
    BOOL empty = FALSE;
    TID threads[8] = {0};
    srand(GETTIME());
    totalThreads = rand()%7 + 2;
    //make it even
    totalThreads -= totalThreads%2;
    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueCreate(&pSafeQueue));
    user.pSafeQueue = pSafeQueue;
    ATOMIC_STORE_BOOL(&user.usable, TRUE);
    for(UINT64 i = 0; i < totalThreads/2; i++) {
        THREAD_CREATE(&threads[threadCount++], readingThread, &user);
    }
    for(UINT64 i = 0; i < totalThreads/2; i++) {
        THREAD_CREATE(&threads[threadCount++], writingThread, &user);
    }
    for(UINT64 i = 0; i < totalThreads; i++) {
        THREAD_JOIN(threads[i], NULL);
    }
    ATOMIC_STORE_BOOL(&user.usable, FALSE);
    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueFree(pSafeQueue));
}

TEST_F(ThreadsafeBlockingQueueFunctionalityTest, multithreadTeardownTest)
{
    PSafeBlockingQueue pSafeQueue = NULL;
    SafeQueueUser user;
    UINT64 totalThreads = 0, item = 0, threadCount = 0;
    BOOL empty = FALSE;
    TID threads[8] = {0};
    srand(GETTIME());
    totalThreads = rand()%7 + 2;
    //make it even
    totalThreads -= totalThreads%2;
    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueCreate(&pSafeQueue));
    user.pSafeQueue = pSafeQueue;
    ATOMIC_STORE_BOOL(&user.usable, TRUE);
    for(UINT64 i = 0; i < totalThreads; i++) {
        THREAD_CREATE(&threads[threadCount++], readingThread, &user);
    }
    THREAD_SLEEP(125 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    ATOMIC_STORE_BOOL(&user.usable, FALSE);
    EXPECT_EQ(STATUS_SUCCESS, safeBlockingQueueFree(pSafeQueue));
    for(UINT64 i = 0; i < totalThreads; i++) {
        THREAD_JOIN(threads[i], NULL);
    }
}
