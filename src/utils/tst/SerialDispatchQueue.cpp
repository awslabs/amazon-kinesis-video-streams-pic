#include "UtilTestFixture.h"
#include <thread>

class SerialDispatchQueueFunctionalityTest : public UtilTestBase {
};

TEST_F(SerialDispatchQueueFunctionalityTest, createAndFreeTest)
{
    SERIAL_DISPATCH_QUEUE_HANDLE handle = INVALID_SERIAL_DISPATCH_QUEUE_HANDLE_VALUE;

    EXPECT_NE(STATUS_SUCCESS, serialDispatchQueueFree(NULL));
    EXPECT_NE(STATUS_SUCCESS, serialDispatchQueueCreate(NULL));
    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueFree(&handle));

    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueCreate(&handle));
    EXPECT_TRUE(IS_VALID_SERIAL_DISPATCH_QUEUE_HANDLE_VALUE(handle));
    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueFree(&handle));
    EXPECT_TRUE(!IS_VALID_SERIAL_DISPATCH_QUEUE_HANDLE_VALUE(handle));
}

VOID incrementCountTask(UINT64 args) {
    THREAD_SLEEP(5 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    PSIZE_T pCount = (PSIZE_T) args;
    *pCount += 1;
}

TEST_F(SerialDispatchQueueFunctionalityTest, noContentionOverSharedResourceTest)
{
    SERIAL_DISPATCH_QUEUE_HANDLE handle = INVALID_SERIAL_DISPATCH_QUEUE_HANDLE_VALUE;
    const UINT64 totalThreadCount = 100;
    std::thread threads[totalThreadCount];
    SIZE_T executionCount = 0;

    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueCreate(&handle));

    auto threadRoutine = [](SERIAL_DISPATCH_QUEUE_HANDLE handle, PSIZE_T pCount) -> void {
        EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueDispatchTask(handle,
                                                                  DISPATCH_QUEUE_TASK_PRIORITY_DEFAULT,
                                                                  incrementCountTask,
                                                                  (UINT64) pCount));
    };

    for (int i = 0; i < totalThreadCount; ++i) {
        threads[i] = std::thread(threadRoutine, handle, &executionCount);
    }
    for (int i = 0; i < totalThreadCount; ++i) {
        threads[i].join();
    }

    THREAD_SLEEP(totalThreadCount * 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    EXPECT_EQ(executionCount, totalThreadCount);

    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueFree(&handle));
}

TEST_F(SerialDispatchQueueFunctionalityTest, honorPriorityTest)
{
    SERIAL_DISPATCH_QUEUE_HANDLE handle = INVALID_SERIAL_DISPATCH_QUEUE_HANDLE_VALUE;
    const UINT64 totalThreadCount = 100;
    std::thread defaultPriThreads[totalThreadCount];
    SIZE_T defaultPriorityExecutionCount = 0;
    SIZE_T highPriorityExecutionCount = 0;

    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueCreate(&handle));

    auto threadRoutine = [](SERIAL_DISPATCH_QUEUE_HANDLE handle, PSIZE_T pCount, BOOL highPri) -> void {
        EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueDispatchTask(handle,
                                                                  highPri ? DISPATCH_QUEUE_TASK_PRIORITY_HIGH : DISPATCH_QUEUE_TASK_PRIORITY_DEFAULT,
                                                                  incrementCountTask,
                                                                  (UINT64) pCount));
    };

    for (int i = 0; i < totalThreadCount; ++i) {
        defaultPriThreads[i] = std::thread(threadRoutine, handle, &defaultPriorityExecutionCount, FALSE);
    }
    std::thread highPriThread(threadRoutine, handle, &highPriorityExecutionCount, TRUE);

    THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_TRUE(ATOMIC_LOAD(&defaultPriorityExecutionCount) < totalThreadCount &&
                ATOMIC_LOAD(&defaultPriorityExecutionCount) > 0);
    EXPECT_TRUE(ATOMIC_LOAD(&highPriorityExecutionCount) > 0);

    for (int i = 0; i < totalThreadCount; ++i) {
        defaultPriThreads[i].join();
    }
    highPriThread.join();

    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueFree(&handle));
}

typedef struct {
    PCHAR destStr;
    CHAR appendingChar;
} AppendCharTaskData, *PAppendCharTaskData;

VOID appendCharTask(UINT64 args) {
    PAppendCharTaskData pData = (PAppendCharTaskData) args;
    int i = 0;
    while (pData->destStr[i] != '\0') {
        i++;
    }
    pData->destStr[i] = pData->appendingChar;

    MEMFREE(pData);
}

TEST_F(SerialDispatchQueueFunctionalityTest, respectTaskOrderOfDefaultPriTasks)
{
    CHAR alphabet[27];
    SERIAL_DISPATCH_QUEUE_HANDLE handle = INVALID_SERIAL_DISPATCH_QUEUE_HANDLE_VALUE;
    CHAR aChar = 'a';

    MEMSET(alphabet, 0x00, SIZEOF(alphabet));

    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueCreate(&handle));

    for (int i = 0; i < 26; ++i) {
        PAppendCharTaskData pData = (PAppendCharTaskData) MEMALLOC(SIZEOF(AppendCharTaskData));
        pData->destStr = alphabet;
        pData->appendingChar = aChar;
        EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueDispatchTask(handle,
                                                                  DISPATCH_QUEUE_TASK_PRIORITY_DEFAULT,
                                                                  appendCharTask,
                                                                  (UINT64) pData));
        aChar++;
    }

    THREAD_SLEEP(50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_TRUE(!STRCMP(alphabet, "abcdefghijklmnopqrstuvwxyz"));
    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueFree(&handle));
}

TEST_F(SerialDispatchQueueFunctionalityTest, respectTaskOrderOfHighPriTasks)
{
    CHAR alphabet[27];
    SERIAL_DISPATCH_QUEUE_HANDLE handle = INVALID_SERIAL_DISPATCH_QUEUE_HANDLE_VALUE;
    CHAR aChar = 'a';

    MEMSET(alphabet, 0x00, SIZEOF(alphabet));

    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueCreate(&handle));

    for (int i = 0; i < 26; ++i) {
        PAppendCharTaskData pData = (PAppendCharTaskData) MEMALLOC(SIZEOF(AppendCharTaskData));
        pData->destStr = alphabet;
        pData->appendingChar = aChar;
        EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueDispatchTask(handle,
                                                                  DISPATCH_QUEUE_TASK_PRIORITY_HIGH,
                                                                  appendCharTask,
                                                                  (UINT64) pData));
        aChar++;
    }

    THREAD_SLEEP(50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_TRUE(!STRCMP(alphabet, "abcdefghijklmnopqrstuvwxyz"));
    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueFree(&handle));
}

TEST_F(SerialDispatchQueueFunctionalityTest, respectTaskOrderOfHighAndDefaultPriTasks)
{
    CHAR alphabetPriDefault[27];
    CHAR alphabetPriHigh[27];
    SERIAL_DISPATCH_QUEUE_HANDLE handle = INVALID_SERIAL_DISPATCH_QUEUE_HANDLE_VALUE;

    MEMSET(alphabetPriDefault, 0x00, SIZEOF(alphabetPriDefault));
    MEMSET(alphabetPriHigh, 0x00, SIZEOF(alphabetPriHigh));

    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueCreate(&handle));

    auto dispatchAppendTask = [](SERIAL_DISPATCH_QUEUE_HANDLE handle, PCHAR destStr, BOOL highPri) -> void {
        CHAR aChar = 'a';
        for (int i = 0; i < 26; ++i) {
            PAppendCharTaskData pData = (PAppendCharTaskData) MEMALLOC(SIZEOF(AppendCharTaskData));
            pData->destStr = destStr;
            pData->appendingChar = aChar;
            EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueDispatchTask(handle,
                                                                      highPri ? DISPATCH_QUEUE_TASK_PRIORITY_HIGH : DISPATCH_QUEUE_TASK_PRIORITY_DEFAULT,
                                                                      appendCharTask,
                                                                      (UINT64) pData));
            aChar++;
        }
    };

    std::thread appendAlphabetPriDefault(dispatchAppendTask, handle, alphabetPriDefault, FALSE);
    std::thread appendAlphabetPriHigh(dispatchAppendTask, handle, alphabetPriHigh, TRUE);

    appendAlphabetPriDefault.join();
    appendAlphabetPriHigh.join();

    THREAD_SLEEP(50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_TRUE(!STRCMP(alphabetPriDefault, "abcdefghijklmnopqrstuvwxyz"));
    EXPECT_TRUE(!STRCMP(alphabetPriHigh, "abcdefghijklmnopqrstuvwxyz"));
    EXPECT_EQ(STATUS_SUCCESS, serialDispatchQueueFree(&handle));
}
