#include "Include_i.h"

// per thread in threadpool
typedef struct __ThreadData {
    volatile ATOMIC_BOOL terminate;
    PThreadpool pThreadpool;
    MUTEX dataMutex;
} ThreadData, *PThreadData;

typedef struct TaskData {
    startRoutine function;
    PVOID customData;
} TaskData, *PTaskData;

PVOID threadpoolActor(PVOID data)
{
    PThreadData pThreadData = (PThreadData) data;
    PThreadpool pThreadpool = NULL;
    PSafeBlockingQueue pQueue = NULL;
    PTaskData pTask = NULL;
    UINT32 count = 0;
    BOOL taskQueueEmpty = FALSE;
    UINT64 item = 0;
    BOOL finished = FALSE;

    if (pThreadData == NULL) {
        DLOGE("Threadpool actor unable to start, threaddata is NULL");
        return 0;
    }

    // attempt to acquire thread mutex, if we cannot it means the threadpool has already been
    // destroyed. Quickly exit
    if (MUTEX_TRYLOCK(pThreadData->dataMutex)) {
        pThreadpool = pThreadData->pThreadpool;

        if (pThreadpool == NULL) {
            DLOGE("Threadpool actor unable to start, threadpool is NULL");
            return 0;
        }

        pQueue = pThreadpool->taskQueue;
        MUTEX_UNLOCK(pThreadData->dataMutex);
    } else {
        finished = TRUE;
    }

    // This actor will now wait for a task to be added to the queue, and then execute that task
    // when the task is complete it will check if the we're beyond our min threshhold of threads
    // to determine whether it should exit or wait for another task.
    while (!finished) {
        // ThreadData is allocated separately from the Threadpool.
        // The Threadpool will set terminate to false before the threadpool is free.
        // This way the thread actors can avoid accessing the Threadpool after termination.
        if (!ATOMIC_LOAD_BOOL(&pThreadData->terminate)) {
            ATOMIC_INCREMENT(&pThreadData->pThreadpool->availableThreads);
            if (safeBlockingQueueDequeue(pQueue, &item) == STATUS_SUCCESS) {
                pTask = (PTaskData) item;
                ATOMIC_DECREMENT(&pThreadData->pThreadpool->availableThreads);
                if (pTask != NULL) {
                    pTask->function(pTask->customData);
                    SAFE_MEMFREE(pTask);
                }
                // got an error, but not terminating, so fixup available thread count
            } else if (!ATOMIC_LOAD_BOOL(&pThreadData->terminate)) {
                ATOMIC_DECREMENT(&pThreadData->pThreadpool->availableThreads);
            }
        }

        if (MUTEX_TRYLOCK(pThreadData->dataMutex)) {
            // Threadpool is active - lock its mutex
            MUTEX_LOCK(pThreadpool->listMutex);

            // Check that there aren't any pending tasks.
            if (safeBlockingQueueIsEmpty(pQueue, &taskQueueEmpty) == STATUS_SUCCESS) {
                if (taskQueueEmpty) {
                    // Check if this thread is needed to maintain minimum thread count
                    // otherwise exit loop and remove it.
                    if (stackQueueGetCount(pThreadpool->threadList, &count) == STATUS_SUCCESS) {
                        if (count > pThreadpool->minThreads) {
                            finished = TRUE;
                            if (stackQueueRemoveItem(pThreadpool->threadList, pThreadData) != STATUS_SUCCESS) {
                                DLOGE("Failed to remove thread data from threadpool");
                            }
                        }
                    }
                }
            }
            // this is a redundant safety check. To get here the threadpool must be being
            // actively being destroyed, but it was unable to acquire our lock so entered a
            // sleep spin check to allow this thread to finish. However somehow the task queue
            // was not empty, so we ended up here. This check forces us to still exit gracefully
            // in the event somehow the queue is not empty.
            else if (ATOMIC_LOAD_BOOL(&pThreadData->terminate)) {
                finished = TRUE;
                if (stackQueueRemoveItem(pThreadpool->threadList, pThreadData) != STATUS_SUCCESS) {
                    DLOGE("Failed to remove thread data from threadpool");
                }
            }
            MUTEX_UNLOCK(pThreadpool->listMutex);
            MUTEX_UNLOCK(pThreadData->dataMutex);
        } else {
            // couldn't lock our mutex, which means Threadpool locked this mutex to indicate
            // Threadpool has been deleted.
            //
            // Unlock mutex and free ThreadData
            MUTEX_UNLOCK(pThreadData->dataMutex);
            finished = TRUE;
        }
    }
    // we assume we've already been removed from the threadList
    MUTEX_FREE(pThreadData->dataMutex);
    SAFE_MEMFREE(pThreadData);
    return 0;
}

/**
 * Create a new threadpool
 */
STATUS threadpoolCreate(PThreadpool* ppThreadpool, UINT32 minThreads, UINT32 maxThreads)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i = 0;
    PThreadpool pThreadpool = NULL;
    BOOL poolCreated = FALSE, mutexCreated = FALSE, listCreated = FALSE, queueCreated = FALSE;
    CHK(ppThreadpool != NULL, STATUS_NULL_ARG);
    CHK(minThreads <= maxThreads && minThreads > 0 && maxThreads > 0, STATUS_INVALID_ARG);

    pThreadpool = (PThreadpool) MEMCALLOC(1, SIZEOF(Threadpool));
    CHK(pThreadpool != NULL, STATUS_NOT_ENOUGH_MEMORY);
    poolCreated = TRUE;

    ATOMIC_STORE_BOOL(&pThreadpool->terminate, FALSE);
    ATOMIC_STORE(&pThreadpool->availableThreads, 0);

    pThreadpool->listMutex = MUTEX_CREATE(FALSE);
    mutexCreated = TRUE;

    CHK_STATUS(safeBlockingQueueCreate(&pThreadpool->taskQueue));
    queueCreated = TRUE;

    CHK_STATUS(stackQueueCreate(&pThreadpool->threadList));
    listCreated = TRUE;

    pThreadpool->minThreads = minThreads;
    pThreadpool->maxThreads = maxThreads;
    for (i = 0; i < minThreads; i++) {
        CHK_STATUS(threadpoolInternalCreateThread(pThreadpool));
    }

    *ppThreadpool = pThreadpool;

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        if (mutexCreated) {
            MUTEX_FREE(pThreadpool->listMutex);
        }
        if (listCreated) {
            stackQueueFree(pThreadpool->threadList);
        }
        if (queueCreated) {
            safeBlockingQueueFree(pThreadpool->taskQueue);
        }
        if (poolCreated) {
            SAFE_MEMFREE(pThreadpool);
        }
    }
    return retStatus;
}

/**
 * Creates a thread, thread data, and detaches thread
 * Adds threadpool list
 */
STATUS threadpoolInternalCreateThread(PThreadpool pThreadpool)
{
    STATUS retStatus = STATUS_SUCCESS;
    PThreadData data = NULL;
    BOOL locked = FALSE;
    TID thread;
    CHK(pThreadpool != NULL, STATUS_NULL_ARG);

    CHK(!ATOMIC_LOAD_BOOL(&pThreadpool->terminate), STATUS_INVALID_OPERATION);

    MUTEX_LOCK(pThreadpool->listMutex);
    locked = TRUE;

    data = (PThreadData) MEMCALLOC(1, SIZEOF(ThreadData));
    CHK(data != NULL, STATUS_NOT_ENOUGH_MEMORY);

    data->dataMutex = MUTEX_CREATE(FALSE);
    data->pThreadpool = pThreadpool;

    CHK_STATUS(stackQueueEnqueue(pThreadpool->threadList, (UINT64) data));

    MUTEX_UNLOCK(pThreadpool->listMutex);
    locked = FALSE;

    THREAD_CREATE(&thread, threadpoolActor, data);
    THREAD_DETACH(thread);

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pThreadpool->listMutex);
    }

    return retStatus;
}

STATUS threadpoolInternalCreateTask(PThreadpool pThreadpool, startRoutine function, PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PTaskData pTask = NULL;
    BOOL allocated = FALSE;
    CHK(pThreadpool != NULL, STATUS_NULL_ARG);

    pTask = (PTaskData) MEMCALLOC(1, SIZEOF(TaskData));
    CHK(pTask != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pTask->function = function;
    pTask->customData = customData;

    allocated = TRUE;

    CHK_STATUS(safeBlockingQueueEnqueue(pThreadpool->taskQueue, (UINT64) pTask));

CleanUp:
    if (STATUS_FAILED(retStatus) && allocated) {
        SAFE_MEMFREE(pTask);
    }

    return retStatus;
}

STATUS threadpoolInternalCanCreateThread(PThreadpool pThreadpool, PBOOL pSpaceAvailable)
{
    STATUS retStatus = STATUS_SUCCESS;
    PThreadData data = NULL;
    UINT32 count = 0;
    BOOL locked = FALSE;

    CHK(pThreadpool != NULL && pSpaceAvailable != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pThreadpool->terminate), STATUS_INVALID_OPERATION);

    MUTEX_LOCK(pThreadpool->listMutex);
    locked = TRUE;

    CHK_STATUS(stackQueueGetCount(pThreadpool->threadList, &count));

    if (count < pThreadpool->maxThreads) {
        *pSpaceAvailable = TRUE;
    } else {
        *pSpaceAvailable = FALSE;
    }

    MUTEX_UNLOCK(pThreadpool->listMutex);
    locked = FALSE;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pThreadpool->listMutex);
    }

    return retStatus;
}

/**
 * Destroy a threadpool
 */
STATUS threadpoolFree(PThreadpool pThreadpool)
{
    STATUS retStatus = STATUS_SUCCESS;
    StackQueueIterator iterator;
    PThreadData item = NULL;
    BOOL finished = FALSE, taskQueueEmpty = FALSE;
    CHK(pThreadpool != NULL, STATUS_NULL_ARG);

    // Threads are not forced to finish their tasks. If the user has assigned
    // a thread with an infinite loop then this threadpool object cannot safely
    // terminate it

    //------Inform all threads that Threadpool has been terminated----------

    // set terminate flag of pool -- no new threads/items can be added now
    ATOMIC_STORE_BOOL(&pThreadpool->terminate, TRUE);

    CHK_STATUS(safeBlockingQueueIsEmpty(pThreadpool->taskQueue, &taskQueueEmpty));
    if (!taskQueueEmpty) {
        CHK_STATUS(safeBlockingQueueClear(pThreadpool->taskQueue, TRUE));
    }

    while (!finished) {
        // lock list mutex
        MUTEX_LOCK(pThreadpool->listMutex);

        do {
            // iterate on list
            retStatus = stackQueueGetIterator(pThreadpool->threadList, &iterator);
            if (!IS_VALID_ITERATOR(iterator)) {
                finished = TRUE;
                break;
            }
            retStatus = stackQueueIteratorGetItem(iterator, &item);

            // set terminate flag of item
            ATOMIC_STORE_BOOL(&item->terminate, TRUE);

            // attempt to lock mutex of item
            if (MUTEX_TRYLOCK(item->dataMutex)) {
                // when we acquire the lock, remove the item from the list. Its thread will free it.
                if (stackQueueRemoveItem(pThreadpool->threadList, item) != STATUS_SUCCESS) {
                    DLOGE("Failed to remove thread data from threadpool");
                }
            } else {
                // if the mutex is taken, unlock list mutex, sleep 10 ms and 'start over'
                //
                // The reasoning here is that the threadActors only acquire their mutex to check
                // for termination, after acquiring their mutex they need the list mutex to evaluate
                // the current count and determine if they should exit or wait on the taskQueue.
                //
                // Therefore if we currently have the list mutex, but cannot acquire the item mutex they
                // must be blocking on a mutex lock for the list. When we release it and sleep we allow the
                // other thread to finish their check operation and then unlock the mutex. Next they'll see
                // the termination flag we set earlier and will exit.
                //
                // When we unlock and sleep we give them
                break;
            }
        } while (1);

        MUTEX_UNLOCK(pThreadpool->listMutex);
        if (!finished) {
            // the aforementioned sleep
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        }
    }

    // now free all the memory
    MUTEX_FREE(pThreadpool->listMutex);
    stackQueueFree(pThreadpool->threadList);

    // this auto kicks out all blocking calls to it
    safeBlockingQueueFree(pThreadpool->taskQueue);
    SAFE_MEMFREE(pThreadpool);

CleanUp:

    return retStatus;
}

/**
 * Amount of threads currently tracked by this threadpool
 */
STATUS threadpoolTotalThreadCount(PThreadpool pThreadpool, PUINT32 pCount)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pThreadpool != NULL && pCount != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pThreadpool->terminate), STATUS_INVALID_OPERATION);

    MUTEX_LOCK(pThreadpool->listMutex);
    locked = TRUE;

    CHK_STATUS(stackQueueGetCount(pThreadpool->threadList, pCount));

    MUTEX_UNLOCK(pThreadpool->listMutex);
    locked = FALSE;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pThreadpool->listMutex);
    }
    return retStatus;
}

/**
 * Amount of threads available to accept a new task
 */
STATUS threadpoolInternalInactiveThreadCount(PThreadpool pThreadpool, PSIZE_T pCount)
{
    STATUS retStatus = STATUS_SUCCESS;
    SIZE_T unblockedThreads = 0;
    UINT32 pendingTasks;

    CHK(pThreadpool != NULL && pCount != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pThreadpool->terminate), STATUS_INVALID_OPERATION);

    CHK_STATUS(safeBlockingQueueGetCount(pThreadpool->taskQueue, &pendingTasks));
    unblockedThreads = (SIZE_T) ATOMIC_LOAD(&pThreadpool->availableThreads);
    *pCount = unblockedThreads - (SIZE_T) pendingTasks;

CleanUp:
    return retStatus;
}

/**
 * Create a thread with the given task.
 * returns: STATUS_SUCCESS if a thread was already available
 *          or if a new thread was created.
 *          STATUS_FAILED/ if the threadpool is already at its
 *          predetermined max.
 */
STATUS threadpoolTryAdd(PThreadpool pThreadpool, startRoutine function, PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL spaceAvailable = FALSE;
    SIZE_T count = 0;
    CHK(pThreadpool != NULL, STATUS_NULL_ARG);

    CHK_STATUS(threadpoolInternalCanCreateThread(pThreadpool, &spaceAvailable));
    CHK_STATUS(threadpoolInternalInactiveThreadCount(pThreadpool, &count));
    // fail if there is not an available thread or if we're already maxed out on threads
    CHK(spaceAvailable || count > 0, STATUS_THREADPOOL_MAX_COUNT);

    CHK_STATUS(threadpoolInternalCreateTask(pThreadpool, function, customData));
    // only create a thread if there aren't any inactive threads.
    if (count <= 0) {
        CHK_STATUS(threadpoolInternalCreateThread(pThreadpool));
    }

CleanUp:
    return retStatus;
}

/**
 * Create a thread with the given task.
 * returns: STATUS_SUCCESS if a thread was already available
 *          or if a new thread was created
 *          or if the task was added to the queue for the next thread.
 *          STATUS_FAILED/ if the threadpool queue is full.
 */
STATUS threadpoolPush(PThreadpool pThreadpool, startRoutine function, PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL spaceAvailable = FALSE;
    SIZE_T count = 0;
    CHK(pThreadpool != NULL, STATUS_NULL_ARG);

    CHK_STATUS(threadpoolInternalCanCreateThread(pThreadpool, &spaceAvailable));
    CHK_STATUS(threadpoolInternalInactiveThreadCount(pThreadpool, &count));

    // always queue task
    CHK_STATUS(threadpoolInternalCreateTask(pThreadpool, function, customData));

    // only create a thread if there are no available threads and not maxed
    if (count <= 0 && spaceAvailable) {
        CHK_STATUS(threadpoolInternalCreateThread(pThreadpool));
    }

CleanUp:
    return retStatus;
}
