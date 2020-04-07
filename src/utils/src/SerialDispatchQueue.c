#include "Include_i.h"

/**
 * Create a serial dispatch queue object
 */
STATUS serialDispatchQueueCreate(PSERIAL_DISPATCH_QUEUE_HANDLE pHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSerialDispatchQueue pSerialDispatchQueue = NULL;

    CHK(pHandle != NULL, STATUS_NULL_ARG);

    CHK_STATUS(serialDispatchQueueCreateInternal(&pSerialDispatchQueue));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        pSerialDispatchQueue = NULL;
    }

    if (pHandle != NULL) {
        *pHandle = TO_SERIAL_DISPATCH_QUEUE_HANDLE(pSerialDispatchQueue);
    }

    LEAVES();
    return retStatus;
}

/**
 * Frees and de-allocates the serial dispatch queue
 */
STATUS serialDispatchQueueFree(PSERIAL_DISPATCH_QUEUE_HANDLE pHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSerialDispatchQueue pSerialDispatchQueue = NULL;

    CHK(pHandle != NULL, STATUS_NULL_ARG);

    pSerialDispatchQueue = FROM_SERIAL_DISPATCH_QUEUE_HANDLE(*pHandle);

    serialDispatchQueueFreeInternal(&pSerialDispatchQueue);

    // Set the handle pointer to invalid
    *pHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS serialDispatchQueueShutdown(SERIAL_DISPATCH_QUEUE_HANDLE handle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSerialDispatchQueue pSerialDispatchQueue = NULL;

    CHK(IS_VALID_SERIAL_DISPATCH_QUEUE_HANDLE_VALUE(handle), STATUS_INVALID_ARG);

    pSerialDispatchQueue = FROM_SERIAL_DISPATCH_QUEUE_HANDLE(handle);

    ATOMIC_STORE_BOOL(&pSerialDispatchQueue->shutdown, TRUE);
    CVAR_SIGNAL(pSerialDispatchQueue->executorCvar);
    THREAD_JOIN(pSerialDispatchQueue->executorTid, NULL);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS serialDispatchQueueDispatchTask(SERIAL_DISPATCH_QUEUE_HANDLE handle,
                                       DISPATCH_QUEUE_TASK_PRIORITY priority,
                                       DispatchQueueTaskFunc dispatchQueueTaskFn,
                                       UINT64 customData)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSerialDispatchQueue pSerialDispatchQueue = NULL;
    BOOL locked = FALSE;
    PDispatchQueueTask pDispatchQueueTask = NULL;

    CHK(dispatchQueueTaskFn != NULL, STATUS_NULL_ARG);
    CHK(IS_VALID_SERIAL_DISPATCH_QUEUE_HANDLE_VALUE(handle), STATUS_INVALID_ARG);

    pSerialDispatchQueue = FROM_SERIAL_DISPATCH_QUEUE_HANDLE(handle);

    CHK_ERR(!ATOMIC_LOAD_BOOL(&pSerialDispatchQueue->shutdown),
            STATUS_INVALID_OPERATION,
            "SerialDispatchQueue is shutting down");

    MUTEX_LOCK(pSerialDispatchQueue->lock);
    locked = TRUE;

    pDispatchQueueTask = (PDispatchQueueTask) MEMALLOC(SIZEOF(DispatchQueueTask));
    CHK(pDispatchQueueTask != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pDispatchQueueTask->customData = customData;
    pDispatchQueueTask->dispatchQueueTaskFn = dispatchQueueTaskFn;

    if (priority == DISPATCH_QUEUE_TASK_PRIORITY_HIGH) {
        CHK_STATUS(stackQueueEnqueue(pSerialDispatchQueue->taskqueuePriHigh, (UINT64) pDispatchQueueTask));
    } else {
        CHK_STATUS(stackQueueEnqueue(pSerialDispatchQueue->taskqueuePriDefault, (UINT64) pDispatchQueueTask));
    }
    pDispatchQueueTask = NULL;

    MUTEX_UNLOCK(pSerialDispatchQueue->lock);
    locked = FALSE;

    CVAR_SIGNAL(pSerialDispatchQueue->executorCvar);

CleanUp:

    if (pDispatchQueueTask != NULL) {
        MEMFREE(pDispatchQueueTask);
    }

    if (locked) {
        MUTEX_UNLOCK(pSerialDispatchQueue->lock);
    }

    LEAVES();
    return retStatus;
}

/////////////////////////////////////////////////////////////////////////////////
// Internal operations
/////////////////////////////////////////////////////////////////////////////////

STATUS serialDispatchQueueCreateInternal(PSerialDispatchQueue* ppSerialDispatchQueue)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSerialDispatchQueue pSerialDispatchQueue = NULL;

    CHK(ppSerialDispatchQueue != NULL, STATUS_NULL_ARG);

    CHK(NULL != (pSerialDispatchQueue = (PSerialDispatchQueue) MEMCALLOC(1, SIZEOF(SerialDispatchQueue))),
        STATUS_NOT_ENOUGH_MEMORY);
    pSerialDispatchQueue->lock = MUTEX_CREATE(TRUE);
    CHK(IS_VALID_MUTEX_VALUE(pSerialDispatchQueue->lock), STATUS_INTERNAL_ERROR);
    pSerialDispatchQueue->executorCvar = CVAR_CREATE();
    CHK(IS_VALID_CVAR_VALUE(pSerialDispatchQueue->executorCvar), STATUS_INTERNAL_ERROR);
    pSerialDispatchQueue->shutdown = FALSE;
    CHK_STATUS(stackQueueCreate(&pSerialDispatchQueue->taskqueuePriDefault));
    CHK_STATUS(stackQueueCreate(&pSerialDispatchQueue->taskqueuePriHigh));
    THREAD_CREATE(&pSerialDispatchQueue->executorTid, serialDispatchQueueExecutor, (PVOID) pSerialDispatchQueue);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        serialDispatchQueueFreeInternal(&pSerialDispatchQueue);
        pSerialDispatchQueue = NULL;
    }

    if (ppSerialDispatchQueue != NULL) {
        *ppSerialDispatchQueue = pSerialDispatchQueue;
    }

    LEAVES();
    return retStatus;
}


STATUS serialDispatchQueueFreeInternal(PSerialDispatchQueue* ppSerialDispatchQueue)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSerialDispatchQueue pSerialDispatchQueue = NULL;

    CHK(ppSerialDispatchQueue != NULL, STATUS_NULL_ARG);
    pSerialDispatchQueue = *ppSerialDispatchQueue;
    CHK(pSerialDispatchQueue != NULL, retStatus);

    ATOMIC_STORE_BOOL(&pSerialDispatchQueue->shutdown, TRUE);

    if (IS_VALID_CVAR_VALUE(pSerialDispatchQueue->executorCvar)) {
        CVAR_BROADCAST(pSerialDispatchQueue->executorCvar);
    }

    if (IS_VALID_TID_VALUE(pSerialDispatchQueue->executorTid)) {
        THREAD_JOIN(pSerialDispatchQueue->executorTid, NULL);
    }

    if (pSerialDispatchQueue->taskqueuePriDefault != NULL) {
        stackQueueClear(pSerialDispatchQueue->taskqueuePriDefault, TRUE);
        stackQueueFree(pSerialDispatchQueue->taskqueuePriDefault);
    }

    if (pSerialDispatchQueue->taskqueuePriHigh != NULL) {
        stackQueueClear(pSerialDispatchQueue->taskqueuePriHigh, TRUE);
        stackQueueFree(pSerialDispatchQueue->taskqueuePriHigh);
    }

    if (IS_VALID_CVAR_VALUE(pSerialDispatchQueue->executorCvar)) {
        CVAR_FREE(pSerialDispatchQueue->executorCvar);
    }

    if (IS_VALID_MUTEX_VALUE(pSerialDispatchQueue->lock)) {
        MUTEX_FREE(pSerialDispatchQueue->lock);
    }

    MEMFREE(pSerialDispatchQueue);

    *ppSerialDispatchQueue = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

PVOID serialDispatchQueueExecutor(PVOID args)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSerialDispatchQueue pSerialDispatchQueue = (PSerialDispatchQueue) args;
    PDispatchQueueTask pDispatchQueueTask = NULL;
    UINT32 taskqueuePriDefaultSize = 0, taskqueuePriHighSize = 0;
    UINT64 stackQueueEntry;
    BOOL locked = FALSE;

    CHK(pSerialDispatchQueue != NULL, STATUS_NULL_ARG);

    while (!ATOMIC_LOAD_BOOL(&pSerialDispatchQueue->shutdown)) {
        if (!locked) {
            MUTEX_LOCK(pSerialDispatchQueue->lock);
            locked = TRUE;
        }

        CHK_STATUS(stackQueueGetCount(pSerialDispatchQueue->taskqueuePriDefault, &taskqueuePriDefaultSize));
        CHK_STATUS(stackQueueGetCount(pSerialDispatchQueue->taskqueuePriHigh, &taskqueuePriHighSize));

        if (taskqueuePriDefaultSize == 0 && taskqueuePriHighSize == 0) {
            CVAR_WAIT(pSerialDispatchQueue->executorCvar, pSerialDispatchQueue->lock, INFINITE_TIME_VALUE);

        } else {
            if (taskqueuePriHighSize > 0) {
                CHK_STATUS(stackQueuePop(pSerialDispatchQueue->taskqueuePriHigh, &stackQueueEntry));
            } else if (taskqueuePriDefaultSize > 0) {
                CHK_STATUS(stackQueuePop(pSerialDispatchQueue->taskqueuePriDefault, &stackQueueEntry));
            }

            pDispatchQueueTask = (PDispatchQueueTask) stackQueueEntry;

            MUTEX_UNLOCK(pSerialDispatchQueue->lock);
            locked = FALSE;

            if (pDispatchQueueTask->dispatchQueueTaskFn != NULL) {
                pDispatchQueueTask->dispatchQueueTaskFn(pDispatchQueueTask->customData);
                MEMFREE(pDispatchQueueTask);
            }
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pSerialDispatchQueue->lock);
    }

    LEAVES();
    return (PVOID) (ULONG_PTR) retStatus;
}
