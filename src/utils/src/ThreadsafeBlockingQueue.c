#include "Include_i.h"

/**
 * Create a threadsafe blocking queue
 */
STATUS safeBlockingQueueCreate(PSafeBlockingQueue* ppSafeQueue)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSafeBlockingQueue pSafeQueue = NULL;

    CHK(ppSafeQueue != NULL, STATUS_NULL_ARG);

    //Allocate the main structure
    pSafeQueue = (PSafeBlockingQueue) MEMCALLOC(1, SIZEOF(SafeBlockingQueue));
    CHK(pSafeQueue != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pSafeQueue->mutex = MUTEX_CREATE(FALSE);
    CHK_STATUS(semaphoreCreate(UINT32_MAX, &(pSafeQueue->semaphore)));
    CHK_STATUS(stackQueueCreate(&(pSafeQueue->queue)));

CleanUp:
    if(STATUS_FAILED(retStatus) && pSafeQueue != NULL) {
        SAFE_MEMFREE(pSafeQueue);
    }

    return retStatus;
}

/**
 * Frees and de-allocates the thread safe blocking queue
 */
PUBLIC_API STATUS safeBlockingQueueFree(PSafeBlockingQueue pSafeQueue)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pSafeQueue != NULL, STATUS_NULL_ARG);

    //free semaphore first, this will unblock any threads
    //blocking on the queue.
    CHK_STATUS(semaphoreFree(&(pSafeQueue->semaphore)));
    CHK_STATUS(stackQueueFree(pSafeQueue->queue));
    MUTEX_FREE(pSafeQueue->mutex);

    SAFE_MEMFREE(pSafeQueue);

CleanUp:

    return retStatus;
}

/**
 * Clears and de-allocates all the items
 */
PUBLIC_API STATUS safeBlockingQueueClear(PSafeBlockingQueue pSafeQueue, BOOL freeData)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSafeQueue != NULL, STATUS_NULL_ARG);

    //0 timeout semaphore acquire, eventually getting a timeout, this is done to clear
    //the counting semaphore
    while(semaphoreAcquire(pSafeQueue->semaphore, 0) == STATUS_SUCCESS);

    MUTEX_LOCK(pSafeQueue->mutex);
    locked = TRUE;
    
    CHK_STATUS(stackQueueClear(pSafeQueue->queue, freeData));

    MUTEX_UNLOCK(pSafeQueue->mutex);
    locked = FALSE;

CleanUp:
    if(locked) {
        MUTEX_UNLOCK(pSafeQueue->mutex);
    }

    return retStatus;
}

/**
 * Gets the number of items in the stack/queue
 */
PUBLIC_API STATUS safeBlockingQueueGetCount(PSafeBlockingQueue pSafeQueue, PUINT32 pCount)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSafeQueue != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pSafeQueue->mutex);
    locked = TRUE;
    
    CHK_STATUS(stackQueueGetCount(pSafeQueue->queue, pCount));

    MUTEX_UNLOCK(pSafeQueue->mutex);
    locked = FALSE;

CleanUp:
    if(locked) {
        MUTEX_UNLOCK(pSafeQueue->mutex);
    }

    return retStatus;
}

/**
 * Removes the item at the given item
 */
PUBLIC_API STATUS safeBlockingQueueRemoveItem(PSafeBlockingQueue pSafeQueue, UINT64 item)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSafeQueue != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pSafeQueue->mutex);
    locked = TRUE;
    
    CHK_STATUS(stackQueueRemoveItem(pSafeQueue->queue, item));

    MUTEX_UNLOCK(pSafeQueue->mutex);
    locked = FALSE;

CleanUp:
    if(locked) {
        MUTEX_UNLOCK(pSafeQueue->mutex);
    }

    return retStatus;
}

/**
 * Whether the thread safe blocking queue is empty
 */
PUBLIC_API STATUS safeBlockingQueueIsEmpty(PSafeBlockingQueue pSafeQueue, PBOOL pIsEmpty)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSafeQueue != NULL && pIsEmpty != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pSafeQueue->mutex);
    locked = TRUE;
    
    CHK_STATUS(stackQueueIsEmpty(pSafeQueue->queue, pIsEmpty));

    MUTEX_UNLOCK(pSafeQueue->mutex);
    locked = FALSE;

CleanUp:
    if(locked) {
        MUTEX_UNLOCK(pSafeQueue->mutex);
    }

    return retStatus;
}

/**
 * Enqueues an item in the queue
 */
PUBLIC_API STATUS safeBlockingQueueEnqueue(PSafeBlockingQueue pSafeQueue, UINT64 item)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSafeQueue != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pSafeQueue->mutex);
    locked = TRUE;
    
    CHK_STATUS(stackQueueEnqueue(pSafeQueue->queue, item));

    MUTEX_UNLOCK(pSafeQueue->mutex);
    locked = FALSE;

    CHK_STATUS(semaphoreRelease(pSafeQueue->semaphore));

CleanUp:
    if(locked) {
        MUTEX_UNLOCK(pSafeQueue->mutex);
    }

    return retStatus;
}

/**
 * Dequeues an item from the queue
 */
PUBLIC_API STATUS safeBlockingQueueDequeue(PSafeBlockingQueue pSafeQueue, PUINT64 pItem)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSafeQueue != NULL && pItem != NULL, STATUS_NULL_ARG);

    CHK_STATUS(semaphoreAcquire(pSafeQueue->semaphore, INFINITE_TIME_VALUE));

    MUTEX_LOCK(pSafeQueue->mutex);
    locked = TRUE;
    
    CHK_STATUS(stackQueueDequeue(pSafeQueue->queue, pItem));

    MUTEX_UNLOCK(pSafeQueue->mutex);
    locked = FALSE;

CleanUp:
    if(locked) {
        MUTEX_UNLOCK(pSafeQueue->mutex);
    }

    return retStatus;
}
