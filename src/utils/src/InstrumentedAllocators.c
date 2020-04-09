#include "Include_i.h"

volatile SIZE_T gInstrumentedAllocatorsTotalAllocationSize = 0;

memAlloc gInstrumentedAllocatorsStoredMemAlloc;
memAlignAlloc gInstrumentedAllocatorsStoredMemAlignAlloc;
memCalloc gInstrumentedAllocatorsStoredMemCalloc;
memFree gInstrumentedAllocatorsStoredMemFree;
memRealloc gInstrumentedAllocatorsStoredMemRealloc;

STATUS setInstrumentedAllocators()
{
    // Store the existing function pointers
    gInstrumentedAllocatorsStoredMemAlloc = globalMemAlloc;
    gInstrumentedAllocatorsStoredMemAlignAlloc = globalMemAlignAlloc;
    gInstrumentedAllocatorsStoredMemCalloc = globalMemCalloc;
    gInstrumentedAllocatorsStoredMemFree = globalMemFree;
    gInstrumentedAllocatorsStoredMemRealloc = globalMemRealloc;

    // Overwrite with the instrumented ones
    globalMemAlloc = instrumentedAllocatorsMemAlloc;
    globalMemAlignAlloc = instrumentedAllocatorsMemAlignAlloc;
    globalMemCalloc = instrumentedAllocatorsMemCalloc;
    globalMemFree = instrumentedAllocatorsMemFree;
    globalMemRealloc = instrumentedAllocatorsMemRealloc;

    return STATUS_SUCCESS;
}

STATUS resetInstrumentedAllocators()
{
    STATUS retStatus = STATUS_SUCCESS;
    SIZE_T totalRemainingSize = ATOMIC_LOAD(&gInstrumentedAllocatorsTotalAllocationSize);

    // Reset the global allocators with the stored ones
    globalMemAlloc = gInstrumentedAllocatorsStoredMemAlloc;
    globalMemAlignAlloc = gInstrumentedAllocatorsStoredMemAlignAlloc;
    globalMemCalloc = gInstrumentedAllocatorsStoredMemCalloc;
    globalMemFree = gInstrumentedAllocatorsStoredMemFree;
    globalMemRealloc = gInstrumentedAllocatorsStoredMemRealloc;

    // Check the final total value
    CHK_WARN(totalRemainingSize == 0, STATUS_MEMORY_NOT_FREED,
            "Possible memory leak of size %" PRIu64, totalRemainingSize);

CleanUp:

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

SIZE_T getInstrumentedTotalAllocationSize()
{
    SIZE_T totalRemainingSize = ATOMIC_LOAD(&gInstrumentedAllocatorsTotalAllocationSize);
    return totalRemainingSize;
}

////////////////////////////////////////////////////////////////////////////////
// Internal functionality
////////////////////////////////////////////////////////////////////////////////

PVOID instrumentedAllocatorsMemAlloc(SIZE_T size)
{
    DLOGS("Instrumented mem alloc %" PRIu64 " bytes", (UINT64) size);
    PSIZE_T pAlloc = (PSIZE_T) gInstrumentedAllocatorsStoredMemAlloc(size + SIZEOF(SIZE_T));

    if (pAlloc == NULL) {
        return NULL;
    }

    // Store the allocation size - should be aligned
    *pAlloc = size;

    // Add to the total book keeping
    ATOMIC_ADD(&gInstrumentedAllocatorsTotalAllocationSize, size);

    return pAlloc + 1;
}

PVOID instrumentedAllocatorsMemAlignAlloc(SIZE_T size, SIZE_T alignment)
{
    // Doing the same as alloc
    DLOGS("Instrumented mem alignalloc %" PRIu64 " bytes", (UINT64) size);
    UNUSED_PARAM(alignment);
    return instrumentedAllocatorsMemAlloc(size);
}

PVOID instrumentedAllocatorsMemCalloc(SIZE_T num, SIZE_T size)
{
    SIZE_T overallSize = num * size;
    DLOGS("Instrumented mem calloc %" PRIu64 " bytes", (UINT64) overallSize);

    // Allocate extra size_t to store the size of the allocation
    PSIZE_T pAlloc = (PSIZE_T) gInstrumentedAllocatorsStoredMemCalloc(1, overallSize + SIZEOF(SIZE_T));
    if (pAlloc == NULL) {
        return NULL;
    }

    *pAlloc = overallSize;

    ATOMIC_ADD(&gInstrumentedAllocatorsTotalAllocationSize, overallSize);

    return pAlloc + 1;
}

PVOID instrumentedAllocatorsMemRealloc(PVOID ptr, SIZE_T size)
{
    if (ptr == NULL) {
        return NULL;
    }

    PSIZE_T pAlloc = (PSIZE_T) ptr - 1;
    SIZE_T existingSize = *pAlloc;

    DLOGS("Instrumented mem realloc of %" PRIu64 " bytes to %" PRIu64 " bytes", (UINT64) existingSize, (UINT64) size);

    if (existingSize == size) {
        return ptr;
    }

    PSIZE_T pNewAlloc = (PSIZE_T) gInstrumentedAllocatorsStoredMemRealloc(pAlloc, size + SIZEOF(SIZE_T));
    if (pNewAlloc == NULL) {
        return NULL;
    }

    if (existingSize > size) {
        ATOMIC_SUBTRACT(&gInstrumentedAllocatorsTotalAllocationSize, existingSize - size);
    } else {
        ATOMIC_ADD(&gInstrumentedAllocatorsTotalAllocationSize, size - existingSize);
    }

    return pNewAlloc + 1;
}

VOID instrumentedAllocatorsMemFree(PVOID ptr)
{
    if (ptr == NULL) {
        return;
    }

    PSIZE_T pAlloc = (PSIZE_T) ptr - 1;
    SIZE_T size = *pAlloc;
    DLOGS("Instrumented mem free %" PRIu64 " bytes", (UINT64) size);

    ATOMIC_SUBTRACT(&gInstrumentedAllocatorsTotalAllocationSize, size);

    gInstrumentedAllocatorsStoredMemFree(pAlloc);
}
