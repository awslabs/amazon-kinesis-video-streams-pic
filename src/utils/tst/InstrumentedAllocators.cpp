#include "UtilTestFixture.h"

class InstrumentedAllocatorsTest : public UtilTestBase {
protected:
    InstrumentedAllocatorsTest() : UtilTestBase(FALSE) {
        SRAND(12345);
    }

    static const SIZE_T TestAllocSize = 1000;
    static const UINT32 TestIterationCount = 10000;
};

TEST_F(InstrumentedAllocatorsTest, set_reset_basic_test)
{
    memAlloc storedMemAlloc = globalMemAlloc;
    memAlignAlloc storedMemAlignAlloc = globalMemAlignAlloc;
    memCalloc storedMemCalloc = globalMemCalloc;
    memFree storedMemFree = globalMemFree;
    memRealloc storedMemRealloc = globalMemRealloc;
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();

#ifndef INSTRUMENTED_ALLOCATORS
    SET_INSTRUMENTED_ALLOCATORS();

    // Check that we have not set anything actually
    EXPECT_EQ(storedMemAlloc, globalMemAlloc);
    EXPECT_EQ(storedMemAlignAlloc, globalMemAlignAlloc);
    EXPECT_EQ(storedMemCalloc, globalMemCalloc);
    EXPECT_EQ(storedMemFree, globalMemFree);
    EXPECT_EQ(storedMemRealloc, globalMemRealloc);

    RESET_INSTRUMENTED_ALLOCATORS();

    EXPECT_EQ(storedMemAlloc, globalMemAlloc);
    EXPECT_EQ(storedMemAlignAlloc, globalMemAlignAlloc);
    EXPECT_EQ(storedMemCalloc, globalMemCalloc);
    EXPECT_EQ(storedMemFree, globalMemFree);
    EXPECT_EQ(storedMemRealloc, globalMemRealloc);
#else
    SET_INSTRUMENTED_ALLOCATORS();

    // Check that the allocators have changed
    EXPECT_NE(storedMemAlloc, globalMemAlloc);
    EXPECT_NE(storedMemAlignAlloc, globalMemAlignAlloc);
    EXPECT_NE(storedMemCalloc, globalMemCalloc);
    EXPECT_NE(storedMemFree, globalMemFree);
    EXPECT_NE(storedMemRealloc, globalMemRealloc);

    RESET_INSTRUMENTED_ALLOCATORS();

    // Reset back to the stored ones
    EXPECT_EQ(storedMemAlloc, globalMemAlloc);
    EXPECT_EQ(storedMemAlignAlloc, globalMemAlignAlloc);
    EXPECT_EQ(storedMemCalloc, globalMemCalloc);
    EXPECT_EQ(storedMemFree, globalMemFree);
    EXPECT_EQ(storedMemRealloc, globalMemRealloc);
#endif

    // Force setting/resetting couple of times

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());

        // Check that the allocators have changed
        EXPECT_NE(storedMemAlloc, globalMemAlloc);
        EXPECT_NE(storedMemAlignAlloc, globalMemAlignAlloc);
        EXPECT_NE(storedMemCalloc, globalMemCalloc);
        EXPECT_NE(storedMemFree, globalMemFree);
        EXPECT_NE(storedMemRealloc, globalMemRealloc);

        EXPECT_EQ(STATUS_SUCCESS, resetInstrumentedAllocators());

        // Reset back to the stored ones
        EXPECT_EQ(storedMemAlloc, globalMemAlloc);
        EXPECT_EQ(storedMemAlignAlloc, globalMemAlignAlloc);
        EXPECT_EQ(storedMemCalloc, globalMemCalloc);
        EXPECT_EQ(storedMemFree, globalMemFree);
        EXPECT_EQ(storedMemRealloc, globalMemRealloc);
    }

    EXPECT_EQ(totalAllocSize, getInstrumentedTotalAllocationSize());
}

TEST_F(InstrumentedAllocatorsTest, memory_leak_check_malloc)
{
    EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();
    PVOID pAlloc = MEMALLOC(TestAllocSize);
    MEMSET(pAlloc, 0xFF, TestAllocSize);
    MEMCHK(pAlloc, 0xff, TestAllocSize);
    EXPECT_EQ(STATUS_MEMORY_NOT_FREED, resetInstrumentedAllocators());
    EXPECT_EQ(totalAllocSize + TestAllocSize, getInstrumentedTotalAllocationSize());

    // Free the allocation as we are tripping ASAN
    MEMFREE((PSIZE_T) pAlloc - 1);
}

TEST_F(InstrumentedAllocatorsTest, memory_leak_check_alignalloc)
{
    EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();
    PVOID pAlloc = MEMALIGNALLOC(TestAllocSize, 16);
    MEMSET(pAlloc, 0xFF, TestAllocSize);
    MEMCHK(pAlloc, 0xff, TestAllocSize);
    EXPECT_EQ(STATUS_MEMORY_NOT_FREED, resetInstrumentedAllocators());
    EXPECT_EQ(totalAllocSize + TestAllocSize, getInstrumentedTotalAllocationSize());

    MEMFREE((PSIZE_T) pAlloc - 1);
}

TEST_F(InstrumentedAllocatorsTest, memory_leak_check_calloc)
{
    EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();
    PVOID pAlloc = MEMCALLOC(10, TestAllocSize);
    MEMCHK(pAlloc, 0x00, 10 * TestAllocSize);
    MEMSET(pAlloc, 0xFF, 10 * TestAllocSize);
    MEMCHK(pAlloc, 0xff, 10 * TestAllocSize);
    EXPECT_EQ(STATUS_MEMORY_NOT_FREED, resetInstrumentedAllocators());
    EXPECT_EQ(totalAllocSize + 10 * TestAllocSize, getInstrumentedTotalAllocationSize());

    MEMFREE((PSIZE_T) pAlloc - 1);
}

TEST_F(InstrumentedAllocatorsTest, memory_leak_check_realloc_larger_small_diff)
{
    EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();
    PVOID pAlloc = MEMALLOC(TestAllocSize);
    MEMSET(pAlloc, 0xFF, TestAllocSize);
    MEMCHK(pAlloc, 0xff, TestAllocSize);
    PVOID pRealloc = MEMREALLOC(pAlloc, TestAllocSize + 1);
    MEMSET(pRealloc, 0xFF, TestAllocSize + 1);
    MEMCHK(pRealloc, 0xff, TestAllocSize + 1);

    EXPECT_EQ(STATUS_MEMORY_NOT_FREED, resetInstrumentedAllocators());
    EXPECT_EQ(totalAllocSize + TestAllocSize + 1, getInstrumentedTotalAllocationSize());

    MEMFREE((PSIZE_T) pRealloc - 1);
}

TEST_F(InstrumentedAllocatorsTest, memory_leak_check_realloc_larger_large_diff)
{
    EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();
    PVOID pAlloc = MEMALLOC(TestAllocSize);
    MEMSET(pAlloc, 0xFF, TestAllocSize);
    MEMCHK(pAlloc, 0xff, TestAllocSize);
    PVOID pRealloc = MEMREALLOC(pAlloc, 10 * TestAllocSize);
    MEMSET(pRealloc, 0xFF, 10 * TestAllocSize);
    MEMCHK(pRealloc, 0xff, 10 * TestAllocSize);

    EXPECT_EQ(STATUS_MEMORY_NOT_FREED, resetInstrumentedAllocators());
    EXPECT_EQ(totalAllocSize + 10 * TestAllocSize, getInstrumentedTotalAllocationSize());

    MEMFREE((PSIZE_T) pRealloc - 1);
}

TEST_F(InstrumentedAllocatorsTest, memory_leak_check_realloc_smaller_small_diff)
{
    EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();
    PVOID pAlloc = MEMALLOC(TestAllocSize);
    MEMSET(pAlloc, 0xFF, TestAllocSize);
    MEMCHK(pAlloc, 0xff, TestAllocSize);
    PVOID pRealloc = MEMREALLOC(pAlloc, TestAllocSize - 1);
    MEMSET(pRealloc, 0xFF, TestAllocSize - 1);
    MEMCHK(pRealloc, 0xff, TestAllocSize - 1);

    EXPECT_EQ(STATUS_MEMORY_NOT_FREED, resetInstrumentedAllocators());
    EXPECT_EQ(totalAllocSize + TestAllocSize - 1, getInstrumentedTotalAllocationSize());

    MEMFREE((PSIZE_T) pRealloc - 1);
}

TEST_F(InstrumentedAllocatorsTest, memory_leak_check_realloc_smaller_large_diff)
{
    EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();
    PVOID pAlloc = MEMALLOC(TestAllocSize);
    MEMSET(pAlloc, 0xFF, TestAllocSize);
    MEMCHK(pAlloc, 0xff, TestAllocSize);
    PVOID pRealloc = MEMREALLOC(pAlloc, TestAllocSize / 10);
    MEMSET(pRealloc, 0xFF, TestAllocSize / 10);
    MEMCHK(pRealloc, 0xff, TestAllocSize / 10);

    EXPECT_EQ(STATUS_MEMORY_NOT_FREED, resetInstrumentedAllocators());
    EXPECT_EQ(totalAllocSize + TestAllocSize / 10, getInstrumentedTotalAllocationSize());

    MEMFREE((PSIZE_T) pRealloc - 1);
}

TEST_F(InstrumentedAllocatorsTest, random_alloc_free)
{
    EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();

    for (UINT32 i = 0; i < TestIterationCount; i++) {
        SIZE_T size = RAND() % TestAllocSize + 100;
        PVOID pAlloc = MEMALLOC(size);
        // Touch beginning, mid and end for no no-op
        *(PBYTE) pAlloc = (BYTE) RAND();
        *((PBYTE) pAlloc + size / 2) = (BYTE) RAND();
        *((PBYTE) pAlloc + size - 1) = (BYTE) RAND();

        MEMFREE(pAlloc);
    }

    EXPECT_EQ(totalAllocSize, getInstrumentedTotalAllocationSize());
    resetInstrumentedAllocators();
}

TEST_F(InstrumentedAllocatorsTest, random_alignalloc_free)
{
    EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();

    for (UINT32 i = 0; i < TestIterationCount; i++) {
        SIZE_T size = RAND() % TestAllocSize + 100;
        PVOID pAlloc = MEMALIGNALLOC(size, 16);
        *(PBYTE) pAlloc = (BYTE) RAND();
        *((PBYTE) pAlloc + size / 2) = (BYTE) RAND();
        *((PBYTE) pAlloc + size - 1) = (BYTE) RAND();

        MEMFREE(pAlloc);
    }

    EXPECT_EQ(totalAllocSize, getInstrumentedTotalAllocationSize());
    resetInstrumentedAllocators();
}

TEST_F(InstrumentedAllocatorsTest, random_calloc_free)
{
    EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();

    for (UINT32 i = 0; i < TestIterationCount; i++) {
        SIZE_T size = RAND() % TestAllocSize + 100;
        PVOID pAlloc = MEMCALLOC(3, size);
        *(PBYTE) pAlloc = (BYTE) RAND();
        *((PBYTE) pAlloc + size / 2) = (BYTE) RAND();
        *((PBYTE) pAlloc + size - 1) = (BYTE) RAND();

        MEMFREE(pAlloc);
    }

    EXPECT_EQ(totalAllocSize, getInstrumentedTotalAllocationSize());
    resetInstrumentedAllocators();
}

TEST_F(InstrumentedAllocatorsTest, random_alloc_later_free)
{
    PBYTE allocations[TestIterationCount];
    EXPECT_EQ(STATUS_SUCCESS, setInstrumentedAllocators());
    SIZE_T totalAllocSize = getInstrumentedTotalAllocationSize();

    for (UINT32 i = 0; i < TestIterationCount; i++) {
        SIZE_T size = RAND() % TestAllocSize + 100;
        allocations[i] = (PBYTE) MEMALLOC(size);
        // Touch beginning, mid and end for no no-op
        allocations[i][0] = (BYTE) RAND();
        allocations[i][size / 2] = (BYTE) RAND();
        allocations[i][size - 1] = (BYTE) RAND();
    }

    // Free all allocations
    for (UINT32 i = 0; i < TestIterationCount; i++) {
        MEMFREE(allocations[i]);
    }

    EXPECT_EQ(totalAllocSize, getInstrumentedTotalAllocationSize());
    resetInstrumentedAllocators();
}
