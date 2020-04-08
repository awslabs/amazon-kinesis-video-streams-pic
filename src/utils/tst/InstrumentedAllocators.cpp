#include "UtilTestFixture.h"

class InstrumentedAllocatorsTest : public UtilTestBase {
protected:
    InstrumentedAllocatorsTest() : UtilTestBase(FALSE) {}
    const int TestAllocSize = 1000;
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
}
