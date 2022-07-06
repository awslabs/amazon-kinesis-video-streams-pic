/**
 * Implementation of a stream states callbacks
 */

#define LOG_CLASS "StreamState"
#include "Include_i.h"

/**
 * Static definitions of the states
 */
StateMachineState STREAM_STATE_MACHINE_STATES[] = {
    {
        STREAM_STATE_NEW,
        STREAM_STATE_NONE | STREAM_STATE_NEW | STREAM_STATE_STOPPED,
        fromNewStreamState,
        executeNewStreamState,
        defaultStreamStateTransitionHook,
        INFINITE_RETRY_COUNT_SENTINEL,
        STATUS_INVALID_STREAM_READY_STATE
    },
    {
        STREAM_STATE_DESCRIBE,
        STREAM_STATE_NEW | STREAM_STATE_STOPPED | STREAM_STATE_DESCRIBE | STREAM_STATE_PUT_STREAM,
        fromDescribeStreamState,
        executeDescribeStreamState,
        defaultStreamStateTransitionHook,
        SERVICE_CALL_MAX_RETRY_COUNT,
        STATUS_DESCRIBE_STREAM_CALL_FAILED
    },
    {
        STREAM_STATE_CREATE,
        STREAM_STATE_DESCRIBE | STREAM_STATE_CREATE,
        fromCreateStreamState,
        executeCreateStreamState,
        defaultStreamStateTransitionHook,
        SERVICE_CALL_MAX_RETRY_COUNT,
        STATUS_CREATE_STREAM_CALL_FAILED
    },
    {
        STREAM_STATE_TAG_STREAM,
        STREAM_STATE_DESCRIBE | STREAM_STATE_CREATE | STREAM_STATE_TAG_STREAM,
        fromTagStreamState,
        executeTagStreamState,
        defaultStreamStateTransitionHook,
        SERVICE_CALL_MAX_RETRY_COUNT,
        STATUS_TAG_STREAM_CALL_FAILED
    },
    {
        STREAM_STATE_GET_ENDPOINT,
        STREAM_STATE_STOPPED | STREAM_STATE_DESCRIBE | STREAM_STATE_CREATE | STREAM_STATE_GET_ENDPOINT | STREAM_STATE_TAG_STREAM,
        fromGetEndpointStreamState,
        executeGetEndpointStreamState,
        defaultStreamStateTransitionHook,
        SERVICE_CALL_MAX_RETRY_COUNT,
        STATUS_GET_STREAMING_ENDPOINT_CALL_FAILED
    },
    {
        STREAM_STATE_GET_TOKEN,
        STREAM_STATE_STOPPED | STREAM_STATE_GET_ENDPOINT | STREAM_STATE_GET_TOKEN | STREAM_STATE_PUT_STREAM,
        fromGetTokenStreamState,
        executeGetTokenStreamState,
        defaultStreamStateTransitionHook,
        SERVICE_CALL_MAX_RETRY_COUNT,
        STATUS_GET_STREAMING_TOKEN_CALL_FAILED
    },
    {
        STREAM_STATE_READY,
        STREAM_STATE_STOPPED | STREAM_STATE_GET_TOKEN | STREAM_STATE_READY | STREAM_STATE_PUT_STREAM | STREAM_STATE_STREAMING,
        fromReadyStreamState,
        executeReadyStreamState,
        defaultStreamStateTransitionHook,
        SERVICE_CALL_MAX_RETRY_COUNT,
        STATUS_STREAM_READY_CALLBACK_FAILED
    },
    {
        STREAM_STATE_PUT_STREAM,
        STREAM_STATE_READY | STREAM_STATE_PUT_STREAM,
        fromPutStreamState,
        executePutStreamState,
        defaultStreamStateTransitionHook,
        INFINITE_RETRY_COUNT_SENTINEL,
        STATUS_PUT_STREAM_CALL_FAILED
    },
    {
        STREAM_STATE_STREAMING,
        STREAM_STATE_PUT_STREAM | STREAM_STATE_STREAMING,
        fromStreamingStreamState,
        executeStreamingStreamState,
        defaultStreamStateTransitionHook,
        INFINITE_RETRY_COUNT_SENTINEL,
        STATUS_PUT_STREAM_CALL_FAILED
    },
    {
        STREAM_STATE_STOPPED,
        STREAM_STATE_STOPPED | STREAM_STATE_CREATE | STREAM_STATE_DESCRIBE | STREAM_STATE_TAG_STREAM | STREAM_STATE_GET_ENDPOINT |
        STREAM_STATE_GET_TOKEN | STREAM_STATE_READY | STREAM_STATE_PUT_STREAM | STREAM_STATE_STREAMING | STREAM_STATE_NEW,
        fromStoppedStreamState,
        executeStoppedStreamState,
        defaultStreamStateTransitionHook,
        INFINITE_RETRY_COUNT_SENTINEL,
        STATUS_PUT_STREAM_CALL_FAILED
    }
};

UINT32 STREAM_STATE_MACHINE_STATE_COUNT = SIZEOF(STREAM_STATE_MACHINE_STATES) / SIZEOF(StateMachineState);

///////////////////////////////////////////////////////////////////////////
// State machine callback functions
///////////////////////////////////////////////////////////////////////////


STATUS defaultStreamStateTransitionHook(
        UINT64 customData /* customData should be PKinesisVideoClient */,
        PUINT64 stateTransitionWaitTime) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    STATUS countStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = NULL;
    PKinesisVideoClient pKinesisVideoClient = NULL;
    PKvsRetryStrategy pKvsRetryStrategy = NULL;
    PKvsRetryStrategyCallbacks pKvsRetryStrategyCallbacks = NULL;
    UINT64 retryWaitTime = 0;

    pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    CHK(pKinesisVideoStream != NULL && stateTransitionWaitTime != NULL, STATUS_NULL_ARG);

    pKinesisVideoClient = pKinesisVideoStream->pKinesisVideoClient;
    CHK(pKinesisVideoClient != NULL, STATUS_NULL_ARG);

    pKvsRetryStrategy = &(pKinesisVideoClient->deviceInfo.clientInfo.kvsRetryStrategy);
    pKvsRetryStrategyCallbacks = &(pKinesisVideoClient->deviceInfo.clientInfo.kvsRetryStrategyCallbacks);

    // result > SERVICE_CALL_RESULT_OK covers case for -
    // result != SERVICE_CALL_RESULT_NOT_SET and != SERVICE_CALL_RESULT_OK
    // If we support any other 2xx service call results, the condition
    // should change to (pKinesisVideoStream->base.result > 299 && ...)
    CHK(pKinesisVideoStream->base.result > SERVICE_CALL_RESULT_OK &&
            pKvsRetryStrategy != NULL &&
            pKvsRetryStrategy->pRetryStrategy != NULL &&
            pKvsRetryStrategyCallbacks->executeRetryStrategyFn != NULL, STATUS_SUCCESS);


    if(pKvsRetryStrategyCallbacks->getCurrentRetryAttemptNumberFn != NULL) {
        if((countStatus = pKvsRetryStrategyCallbacks->getCurrentRetryAttemptNumberFn(pKvsRetryStrategy, &pKinesisVideoStream->diagnostics.streamApiCallRetryCount)) != STATUS_SUCCESS) {
            DLOGW("Failed to get retry count. Error code: %08x", countStatus);
        }
        else {
            DLOGD("Stream state machine retry count: %lu", pKinesisVideoStream->diagnostics.streamApiCallRetryCount);
        }
    }

    DLOGD("\n KinesisVideoStream base result is [%u]. Executing KVS retry handler of retry strategy type [%u]",
          pKinesisVideoStream->base.result, pKvsRetryStrategy->retryStrategyType);

    pKvsRetryStrategyCallbacks->executeRetryStrategyFn(pKvsRetryStrategy, &retryWaitTime);
    *stateTransitionWaitTime = retryWaitTime;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromNewStreamState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    UINT64 state = STREAM_STATE_NEW;

    CHK(pKinesisVideoStream != NULL && pState != NULL, STATUS_NULL_ARG);

    // Transition to describe state if not stopped
    if (pKinesisVideoStream->streamState == STREAM_STATE_STOPPED) {
        state = STREAM_STATE_STOPPED;
    } else {
        state = STREAM_STATE_DESCRIBE;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeNewStreamState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);

    CHK(pKinesisVideoStream != NULL, STATUS_NULL_ARG);

    // Step the state machine to automatically invoke the Describe API
    CHK_STATUS(stepStateMachine(pKinesisVideoStream->base.pStateMachine));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeDescribeStreamState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    PKinesisVideoClient pKinesisVideoClient = NULL;

    CHK(pKinesisVideoStream != NULL, STATUS_NULL_ARG);

    pKinesisVideoClient = pKinesisVideoStream->pKinesisVideoClient;

    // Step the client state machine first
    CHK_STATUS(stepClientStateMachine(pKinesisVideoClient));

    pKinesisVideoStream->base.serviceCallContext.pAuthInfo = &pKinesisVideoClient->tokenAuthInfo;
    pKinesisVideoStream->base.serviceCallContext.version = SERVICE_CALL_CONTEXT_CURRENT_VERSION;
    pKinesisVideoStream->base.serviceCallContext.customData = TO_STREAM_HANDLE(pKinesisVideoStream);
    pKinesisVideoStream->base.serviceCallContext.timeout = SERVICE_CALL_DEFAULT_TIMEOUT;
    pKinesisVideoStream->base.serviceCallContext.callAfter = time;

    // Reset the call result
    pKinesisVideoStream->base.result = SERVICE_CALL_RESULT_NOT_SET;

    // Call the describe
    CHK_STATUS(pKinesisVideoClient->clientCallbacks.describeStreamFn(
        pKinesisVideoClient->clientCallbacks.customData, pKinesisVideoStream->streamInfo.name, &pKinesisVideoStream->base.serviceCallContext));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromDescribeStreamState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    UINT64 state = STREAM_STATE_DESCRIBE;

    CHK(pKinesisVideoStream != NULL && pState != NULL, STATUS_NULL_ARG);

    // Transition to states if not stopped
    if (pKinesisVideoStream->streamState == STREAM_STATE_STOPPED) {
        state = STREAM_STATE_STOPPED;
    } else {
        // Check the previous results
        switch (pKinesisVideoStream->base.result) {
            case SERVICE_CALL_RESULT_OK:
                // Check that the stream status is not Deleting as it's an error
                CHK(pKinesisVideoStream->streamStatus != STREAM_STATUS_DELETING, STATUS_STREAM_IS_BEING_DELETED_ERROR);

                // If the call is OK and the stream status is active then move to the next state
                if (pKinesisVideoStream->streamStatus == STREAM_STATUS_ACTIVE) {
                    // If we have tags then we should attempt tagging the stream first
                    if (pKinesisVideoStream->streamInfo.tagCount != 0) {
                        state = STREAM_STATE_TAG_STREAM;
                    } else {
                        state = STREAM_STATE_GET_ENDPOINT;
                    }
                }

                break;

            case SERVICE_CALL_RESOURCE_NOT_FOUND:
                // Move to the create state as no streams have been found
                state = STREAM_STATE_CREATE;
                break;

            default:
                // Re-run the state
                break;
        }
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromCreateStreamState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    UINT64 state = STREAM_STATE_CREATE;

    CHK(pKinesisVideoStream != NULL && pState != NULL, STATUS_NULL_ARG);

    // Transition to states if not stopped
    if (pKinesisVideoStream->streamState == STREAM_STATE_STOPPED) {
        state = STREAM_STATE_STOPPED;
    } else if (pKinesisVideoStream->base.result == SERVICE_CALL_RESULT_OK) {
        if (pKinesisVideoStream->streamInfo.tagCount != 0) {
            state = STREAM_STATE_TAG_STREAM;
        } else {
            state = STREAM_STATE_GET_ENDPOINT;
        }
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromTagStreamState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    UINT64 state = STREAM_STATE_TAG_STREAM;

    CHK(pKinesisVideoStream != NULL && pState != NULL, STATUS_NULL_ARG);

    // Transition to states if not stopped
    if (pKinesisVideoStream->streamState == STREAM_STATE_STOPPED) {
        state = STREAM_STATE_STOPPED;
    } else if (pKinesisVideoStream->base.result == SERVICE_CALL_RESULT_OK) {
        state = STREAM_STATE_GET_ENDPOINT;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromGetEndpointStreamState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    UINT64 state = STREAM_STATE_GET_ENDPOINT;

    CHK(pKinesisVideoStream != NULL && pState != NULL, STATUS_NULL_ARG);

    // Transition to states if not stopped
    if (pKinesisVideoStream->streamState == STREAM_STATE_STOPPED) {
        state = STREAM_STATE_STOPPED;
    } else if (pKinesisVideoStream->base.result == SERVICE_CALL_RESULT_OK) {
        state = STREAM_STATE_GET_TOKEN;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromGetTokenStreamState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    UINT64 state = STREAM_STATE_GET_TOKEN;

    CHK(pKinesisVideoStream != NULL && pState != NULL, STATUS_NULL_ARG);

    // Transition to states if not stopped
    if (pKinesisVideoStream->streamState == STREAM_STATE_STOPPED) {
        state = STREAM_STATE_STOPPED;
    } else if (pKinesisVideoStream->base.result == SERVICE_CALL_RESULT_OK) {
        state = STREAM_STATE_READY;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromPutStreamState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    UINT64 state = STREAM_STATE_PUT_STREAM;
    UINT64 duration, viewByteSize;
    PUploadHandleInfo pUploadHandleInfo, pOngoingUploadHandle;

    CHK(pKinesisVideoStream != NULL && pState != NULL, STATUS_NULL_ARG);

    // Transition to states if not stopped
    if (pKinesisVideoStream->streamState == STREAM_STATE_STOPPED) {
        state = STREAM_STATE_STOPPED;
    } else {
        switch (pKinesisVideoStream->base.result) {
            case SERVICE_CALL_RESULT_OK:
                // find the handle in the new state and set it to ready
                pUploadHandleInfo = getStreamUploadInfoWithState(pKinesisVideoStream, UPLOAD_HANDLE_STATE_NEW);

                if (NULL != pUploadHandleInfo) {
                    pUploadHandleInfo->state = UPLOAD_HANDLE_STATE_READY;

                    // NOTE: On successful streaming state transition we need to notify the data notification callback
                    // to handle the case with the intermittent producer that's already finished putting frames and
                    // is awaiting for the existing buffer to be streamed out.

                    // When a new handle is created, there is a potential that there is no more putFrame to
                    // to drive the first data available call to the new handle. Therefore, do a data available call here.

                    // If there is still any active upload handles, then when they reach end-of-stream they will probe the
                    // next ready handle, so there is no need to do data available call in this case.
                    pOngoingUploadHandle = getStreamUploadInfoWithState(pKinesisVideoStream,
                                                                        UPLOAD_HANDLE_STATE_STREAMING | UPLOAD_HANDLE_STATE_AWAITING_ACK |
                                                                            UPLOAD_HANDLE_STATE_TERMINATING | UPLOAD_HANDLE_STATE_ACK_RECEIVED);

                    if (pOngoingUploadHandle == NULL) {
                        // ping the first ready upload handle. (There can be more than one ready upload handle due to
                        // token rotation)
                        pUploadHandleInfo = getStreamUploadInfoWithState(pKinesisVideoStream, UPLOAD_HANDLE_STATE_READY);

                        // Get the duration and the size
                        CHK_STATUS(getAvailableViewSize(pKinesisVideoStream, &duration, &viewByteSize));

                        // Call the notification callback
                        CHK_STATUS(pKinesisVideoStream->pKinesisVideoClient->clientCallbacks.streamDataAvailableFn(
                            pKinesisVideoStream->pKinesisVideoClient->clientCallbacks.customData, TO_STREAM_HANDLE(pKinesisVideoStream),
                            pKinesisVideoStream->streamInfo.name, pUploadHandleInfo->handle, duration, viewByteSize));
                    }
                }

                state = STREAM_STATE_STREAMING;
                break;

            case SERVICE_CALL_REQUEST_TIMEOUT:
            case SERVICE_CALL_GATEWAY_TIMEOUT:
            case SERVICE_CALL_NETWORK_READ_TIMEOUT:
            case SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT:

                state = STREAM_STATE_GET_ENDPOINT;
                break;

            case SERVICE_CALL_NOT_AUTHORIZED:
            case SERVICE_CALL_FORBIDDEN:

                state = STREAM_STATE_GET_TOKEN;
                break;

            default:
                state = STREAM_STATE_DESCRIBE;
        }
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromStreamingStreamState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    UINT64 state = STREAM_STATE_STREAMING;

    CHK(pKinesisVideoStream != NULL && pState != NULL, STATUS_NULL_ARG);

    // Check the previous results
    if (pKinesisVideoStream->streamState == STREAM_STATE_STOPPED) {
        state = STREAM_STATE_STOPPED;
    }

    // Assign the nest state
    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromStoppedStreamState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    UINT64 state = STREAM_STATE_NEW;

    CHK(pKinesisVideoStream != NULL && pState != NULL, STATUS_NULL_ARG);

    // Indicate that we are not stopped and we are not coming from a new state
    pKinesisVideoStream->streamState = STREAM_STATE_READY;

    retStatus = serviceCallResultCheck(pKinesisVideoStream->base.result);

    // Terminate in case of no-recovery
    CHK(pKinesisVideoStream->streamInfo.streamCaps.recoverOnError, retStatus);

    // Check the result
    switch (retStatus) {
        case STATUS_SUCCESS:
            // Stream has been terminated. Return to get endpoint
            state = STREAM_STATE_GET_ENDPOINT;
            break;

        case STATUS_SERVICE_CALL_DEVICE_LIMIT_ERROR:
        case STATUS_SERVICE_CALL_STREAM_LIMIT_ERROR:
            // Stream/device limit is reached. Return to get endpoint
            state = STREAM_STATE_GET_ENDPOINT;
            retStatus = STATUS_SUCCESS;
            break;

        case STATUS_SERVICE_CALL_NOT_AUTHORIZED_ERROR:
            // Not authorized - return to get token
            state = STREAM_STATE_GET_TOKEN;
            retStatus = STATUS_SUCCESS;
            break;

        case STATUS_SERVICE_CALL_RESOURCE_IN_USE_ERROR:
        case STATUS_SERVICE_CALL_UNKOWN_ERROR:
            // resource in use, unknown - return to describe
            state = STREAM_STATE_DESCRIBE;
            retStatus = STATUS_SUCCESS;
            break;

        case STATUS_ACK_ERR_ACK_INTERNAL_ERROR:
            // Service internal error - return to describe
            state = STREAM_STATE_DESCRIBE;
            retStatus = STATUS_SUCCESS;
            break;

        case STATUS_SERVICE_CALL_RESOURCE_NOT_FOUND_ERROR:
        case STATUS_SERVICE_CALL_RESOURCE_DELETED_ERROR:
            // resource not found, deleted - return to describe
            state = STREAM_STATE_DESCRIBE;
            retStatus = STATUS_SUCCESS;
            break;

        case STATUS_SERVICE_CALL_TIMEOUT_ERROR:
            state = STREAM_STATE_READY;
            retStatus = STATUS_SUCCESS;
            break;

        default:
            // Default state in any other case
            retStatus = STATUS_SUCCESS;
            break;
    }

CleanUp:

    if (pState != NULL) {
        *pState = state;
    }

    LEAVES();
    return retStatus;
}

STATUS fromReadyStreamState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    UINT64 state = STREAM_STATE_READY;

    CHK(pKinesisVideoStream != NULL && pState != NULL, STATUS_NULL_ARG);

    // Transition to put stream state if not stopped
    if (pKinesisVideoStream->streamState == STREAM_STATE_STOPPED) {
        state = STREAM_STATE_STOPPED;
    } else {
        state = STREAM_STATE_PUT_STREAM;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeGetEndpointStreamState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    PKinesisVideoClient pKinesisVideoClient = NULL;
    PStateMachineState pState = NULL;

    CHK(pKinesisVideoStream != NULL, STATUS_NULL_ARG);

    pKinesisVideoClient = pKinesisVideoStream->pKinesisVideoClient;

    // Step the client state machine first
    if (STATUS_FAILED(retStatus = stepClientStateMachine(pKinesisVideoClient))) {
        if (retStatus == STATUS_CLIENT_AUTH_CALL_FAILED) {
            // reset client state machine to READY state

            // Get the accepted state
            CHK_STATUS(getStateMachineState(pKinesisVideoClient->base.pStateMachine, CLIENT_STATE_READY, &pState));

            // Check if we are in the correct state
            CHK_STATUS(acceptStateMachineState(pKinesisVideoClient->base.pStateMachine, pState->acceptStates));

            // Step the machine
            CHK_STATUS(stepStateMachine(pKinesisVideoClient->base.pStateMachine));
        } else {
            CHK(FALSE, retStatus);
        }
    }

    pKinesisVideoStream->base.serviceCallContext.pAuthInfo = &pKinesisVideoClient->tokenAuthInfo;
    pKinesisVideoStream->base.serviceCallContext.version = SERVICE_CALL_CONTEXT_CURRENT_VERSION;
    pKinesisVideoStream->base.serviceCallContext.customData = TO_STREAM_HANDLE(pKinesisVideoStream);
    pKinesisVideoStream->base.serviceCallContext.timeout = SERVICE_CALL_DEFAULT_TIMEOUT;
    pKinesisVideoStream->base.serviceCallContext.callAfter = time;

    // Reset the call result
    pKinesisVideoStream->base.result = SERVICE_CALL_RESULT_NOT_SET;

    // Call API
    CHK_STATUS(pKinesisVideoClient->clientCallbacks.getStreamingEndpointFn(
        pKinesisVideoClient->clientCallbacks.customData, pKinesisVideoStream->streamInfo.name, (PCHAR) GET_DATA_ENDPOINT_REAL_TIME_PUT_API_NAME,
        &pKinesisVideoStream->base.serviceCallContext));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeGetTokenStreamState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    PKinesisVideoClient pKinesisVideoClient = NULL;

    CHK(pKinesisVideoStream != NULL, STATUS_NULL_ARG);

    pKinesisVideoClient = pKinesisVideoStream->pKinesisVideoClient;

    // Step the client state machine first
    CHK_STATUS(stepClientStateMachine(pKinesisVideoClient));

    pKinesisVideoStream->base.serviceCallContext.pAuthInfo = &pKinesisVideoClient->tokenAuthInfo;
    pKinesisVideoStream->base.serviceCallContext.version = SERVICE_CALL_CONTEXT_CURRENT_VERSION;
    pKinesisVideoStream->base.serviceCallContext.customData = TO_STREAM_HANDLE(pKinesisVideoStream);
    pKinesisVideoStream->base.serviceCallContext.timeout = SERVICE_CALL_DEFAULT_TIMEOUT;
    pKinesisVideoStream->base.serviceCallContext.callAfter = time;

    // Reset the call result
    pKinesisVideoStream->base.result = SERVICE_CALL_RESULT_NOT_SET;

    // Call API
    CHK_STATUS(pKinesisVideoClient->clientCallbacks.getStreamingTokenFn(pKinesisVideoClient->clientCallbacks.customData,
                                                                        pKinesisVideoStream->streamInfo.name, STREAM_ACCESS_MODE_READ,
                                                                        &pKinesisVideoStream->base.serviceCallContext));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeCreateStreamState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    PKinesisVideoClient pKinesisVideoClient = NULL;

    CHK(pKinesisVideoStream != NULL, STATUS_NULL_ARG);

    pKinesisVideoClient = pKinesisVideoStream->pKinesisVideoClient;

    // Step the client state machine first
    CHK_STATUS(stepClientStateMachine(pKinesisVideoClient));

    pKinesisVideoStream->base.serviceCallContext.pAuthInfo = &pKinesisVideoClient->tokenAuthInfo;
    pKinesisVideoStream->base.serviceCallContext.version = SERVICE_CALL_CONTEXT_CURRENT_VERSION;
    pKinesisVideoStream->base.serviceCallContext.customData = TO_STREAM_HANDLE(pKinesisVideoStream);
    pKinesisVideoStream->base.serviceCallContext.timeout = SERVICE_CALL_DEFAULT_TIMEOUT;
    pKinesisVideoStream->base.serviceCallContext.callAfter = time;

    // Reset the call result
    pKinesisVideoStream->base.result = SERVICE_CALL_RESULT_NOT_SET;

    // Call API
    CHK_STATUS(pKinesisVideoClient->clientCallbacks.createStreamFn(
        pKinesisVideoClient->clientCallbacks.customData, pKinesisVideoClient->deviceInfo.name, pKinesisVideoStream->streamInfo.name,
        pKinesisVideoStream->streamInfo.streamCaps.contentType, pKinesisVideoStream->streamInfo.kmsKeyId, pKinesisVideoStream->streamInfo.retention,
        &pKinesisVideoStream->base.serviceCallContext));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeTagStreamState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    PKinesisVideoClient pKinesisVideoClient = NULL;

    CHK(pKinesisVideoStream != NULL, STATUS_NULL_ARG);
    CHK(pKinesisVideoStream->streamInfo.tagCount != 0 && pKinesisVideoStream->streamInfo.tags != NULL, STATUS_INVALID_ARG);

    pKinesisVideoClient = pKinesisVideoStream->pKinesisVideoClient;

    // Step the client state machine first
    CHK_STATUS(stepClientStateMachine(pKinesisVideoClient));

    pKinesisVideoStream->base.serviceCallContext.pAuthInfo = &pKinesisVideoClient->tokenAuthInfo;
    pKinesisVideoStream->base.serviceCallContext.version = SERVICE_CALL_CONTEXT_CURRENT_VERSION;
    pKinesisVideoStream->base.serviceCallContext.customData = TO_STREAM_HANDLE(pKinesisVideoStream);
    pKinesisVideoStream->base.serviceCallContext.timeout = SERVICE_CALL_DEFAULT_TIMEOUT;
    pKinesisVideoStream->base.serviceCallContext.callAfter = time;

    // Reset the call result
    pKinesisVideoStream->base.result = SERVICE_CALL_RESULT_NOT_SET;

    // Call API
    CHK_STATUS(pKinesisVideoClient->clientCallbacks.tagResourceFn(pKinesisVideoClient->clientCallbacks.customData, pKinesisVideoStream->base.arn,
                                                                  pKinesisVideoStream->streamInfo.tagCount, pKinesisVideoStream->streamInfo.tags,
                                                                  &pKinesisVideoStream->base.serviceCallContext));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeReadyStreamState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    PKinesisVideoClient pKinesisVideoClient = NULL;
    UINT64 duration, viewByteSize;

    CHK(pKinesisVideoStream != NULL, STATUS_NULL_ARG);

    pKinesisVideoClient = pKinesisVideoStream->pKinesisVideoClient;

    pKinesisVideoStream->streamReady = TRUE;

    // Pulse the ready condition variable
    CHK_STATUS(pKinesisVideoClient->clientCallbacks.broadcastConditionVariableFn(pKinesisVideoClient->clientCallbacks.customData,
                                                                                 pKinesisVideoStream->base.ready));

    // Call the ready callback
    CHK_STATUS(
        pKinesisVideoClient->clientCallbacks.streamReadyFn(pKinesisVideoClient->clientCallbacks.customData, TO_STREAM_HANDLE(pKinesisVideoStream)));

    // Get the duration and the size. If there is stuff to send then also trigger PutStream
    CHK_STATUS(getAvailableViewSize(pKinesisVideoStream, &duration, &viewByteSize));

    // Check if we need to also call put stream API
    if (pKinesisVideoStream->streamState == STREAM_STATE_READY || pKinesisVideoStream->streamState == STREAM_STATE_STOPPED || viewByteSize != 0) {
        // Step the state machine to automatically invoke the PutStream API
        CHK_STATUS(stepStateMachine(pKinesisVideoStream->base.pStateMachine));
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executePutStreamState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);
    PKinesisVideoClient pKinesisVideoClient = NULL;

    CHK(pKinesisVideoStream != NULL, STATUS_NULL_ARG);

    pKinesisVideoClient = pKinesisVideoStream->pKinesisVideoClient;

    // Step the client state machine first
    CHK_STATUS(stepClientStateMachine(pKinesisVideoClient));

    pKinesisVideoStream->base.serviceCallContext.pAuthInfo = &pKinesisVideoStream->streamingAuthInfo;
    pKinesisVideoStream->base.serviceCallContext.version = SERVICE_CALL_CONTEXT_CURRENT_VERSION;
    pKinesisVideoStream->base.serviceCallContext.customData = TO_STREAM_HANDLE(pKinesisVideoStream);
    // Infinite wait for streaming
    pKinesisVideoStream->base.serviceCallContext.timeout = SERVICE_CALL_INFINITE_TIMEOUT;
    pKinesisVideoStream->base.serviceCallContext.callAfter = time;

    // We need to call the put stream API the first time
    if (pKinesisVideoStream->streamState != STREAM_STATE_PUT_STREAM) {
        // Reset the call result
        pKinesisVideoStream->base.result = SERVICE_CALL_RESULT_NOT_SET;

        // Call API
        CHK_STATUS(pKinesisVideoClient->clientCallbacks.putStreamFn(
            pKinesisVideoClient->clientCallbacks.customData, pKinesisVideoStream->streamInfo.name, (PCHAR) MKV_CONTAINER_TYPE_STRING,
            pKinesisVideoClient->clientCallbacks.getCurrentTimeFn(pKinesisVideoClient->clientCallbacks.customData),
            pKinesisVideoStream->streamInfo.streamCaps.absoluteFragmentTimes, pKinesisVideoStream->streamInfo.streamCaps.fragmentAcks,
            pKinesisVideoStream->streamingEndpoint, &pKinesisVideoStream->base.serviceCallContext));

        pKinesisVideoStream->streamState = STREAM_STATE_PUT_STREAM;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeStreamingStreamState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);

    CHK(pKinesisVideoStream != NULL, STATUS_NULL_ARG);

    // Set the indicator state to streaming
    pKinesisVideoStream->streamState = STREAM_STATE_STREAMING;

    // Nothing to do

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeStoppedStreamState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 duration, viewByteSize;
    PKinesisVideoStream pKinesisVideoStream = STREAM_FROM_CUSTOM_DATA(customData);

    CHK(pKinesisVideoStream != NULL, STATUS_NULL_ARG);

    // Also store the connection stopping result
    pKinesisVideoStream->connectionDroppedResult = pKinesisVideoStream->base.result;

    // Check if we want to prime the state machine based on whether we have any more content to send
    // currently and if the error is a timeout.
    if (SERVICE_CALL_RESULT_IS_A_TIMEOUT(pKinesisVideoStream->connectionDroppedResult)) {
        CHK_STATUS(getAvailableViewSize(pKinesisVideoStream, &duration, &viewByteSize));
        if (viewByteSize == 0) {
            // Next time putFrame is called we will self-prime.
            // The idea is to drop frames till new key frame which will
            // become a stream start.
            pKinesisVideoStream->resetGeneratorOnKeyFrame = TRUE;
            pKinesisVideoStream->skipNonKeyFrames = TRUE;

            // Set an indicator to step the state machine on new frame
            pKinesisVideoStream->streamState = STREAM_STATE_NEW;

            // Early exit
            CHK(FALSE, retStatus);
        }
    }

    // Auto-prime the state machine
    CHK_STATUS(stepStateMachine(pKinesisVideoStream->base.pStateMachine));

CleanUp:

    LEAVES();
    return retStatus;
}
