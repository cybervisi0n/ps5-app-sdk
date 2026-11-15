#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <pthread.h>

#include <sys/types.h>

#include "ps5_heap.h"

#ifndef SCE_KERNEL_PROT_CPU_READ
#define SCE_KERNEL_PROT_CPU_READ  0x01
#define SCE_KERNEL_PROT_CPU_WRITE 0x02
#endif

#ifndef PS5_DIRECT_MEMORY_SIZE
#define PS5_DIRECT_MEMORY_SIZE (256ull * 1024 * 1024)
#endif

#ifndef PS5_DIRECT_MEMORY_TYPE
#define PS5_DIRECT_MEMORY_TYPE 0
#endif

extern "C" int sceKernelAllocateDirectMemory(int64_t search_start, int64_t search_end, size_t len, size_t alignment, int memory_type, int64_t* phys_addr_out);
extern "C" int sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags, int64_t direct_memory_start, size_t alignment);
extern "C" int sceKernelReleaseDirectMemory(int64_t start, size_t len);
extern "C" int sceKernelMunmap(void * vaddr, size_t len);

namespace PS5Heap
{
    constexpr size_t ALIGNMENT = 16;
    constexpr size_t DMEM_ALIGNMENT = 0x4000;
    constexpr uint64_t BLOCK_MAGIC = 0x5053354845415042ull;

    constexpr size_t alignUp(size_t n, size_t alignment)
    {
        return (n + alignment - 1) & ~(alignment - 1);
    }

    struct alignas(16) Block
    {
        uint64_t magic;
        size_t size;
        bool free;

        Block* prev;
        Block* next;
    };

    constexpr size_t HEADER_SIZE =
        alignUp(sizeof(Block), ALIGNMENT);

    class DirectHeap
    {
    public:
        DirectHeap()
            : base(nullptr),
              physicalAddress(0),
              heapSize(0),
              first(nullptr),
              initialized(false)
        {
            pthread_mutex_init(&mutex, nullptr);
        }

        ~DirectHeap()
        {
            shutdown();
            pthread_mutex_destroy(&mutex);
        }

        DirectHeap(const DirectHeap&) = delete;
        DirectHeap& operator=(const DirectHeap&) = delete;

        bool initialize(
            size_t requestedSize,
            off_t searchEnd,
            int memoryType)
        {
            pthread_mutex_lock(&mutex);

            if (initialized)
            {
                pthread_mutex_unlock(&mutex);
                return false;
            }

            if (requestedSize == 0 ||
                requestedSize > SIZE_MAX - DMEM_ALIGNMENT)
            {
                pthread_mutex_unlock(&mutex);
                return false;
            }

            heapSize = alignUp(
                requestedSize,
                DMEM_ALIGNMENT
            );

            off_t physical = 0;

            int result = sceKernelAllocateDirectMemory(
                0,
                searchEnd,
                heapSize,
                DMEM_ALIGNMENT,
                memoryType,
                &physical
            );

            if (result != 0)
            {
                heapSize = 0;
                pthread_mutex_unlock(&mutex);
                return false;
            }

            void* virtualAddress = nullptr;

            result = sceKernelMapDirectMemory(
                &virtualAddress,
                heapSize,
                SCE_KERNEL_PROT_CPU_READ |
                SCE_KERNEL_PROT_CPU_WRITE,
                0,
                physical,
                DMEM_ALIGNMENT
            );

            if (result != 0 || virtualAddress == nullptr)
            {
                sceKernelReleaseDirectMemory(
                    physical,
                    heapSize
                );

                heapSize = 0;

                pthread_mutex_unlock(&mutex);
                return false;
            }

            base = static_cast<uint8_t*>(virtualAddress);
            physicalAddress = physical;

            first = reinterpret_cast<Block*>(base);

            first->magic = BLOCK_MAGIC;
            first->size = heapSize;
            first->free = true;
            first->prev = nullptr;
            first->next = nullptr;

            initialized = true;

            pthread_mutex_unlock(&mutex);
            return true;
        }

        void shutdown()
        {
            pthread_mutex_lock(&mutex);

            if (initialized)
            {
                sceKernelMunmap(base, heapSize);

                sceKernelReleaseDirectMemory(
                    physicalAddress,
                    heapSize
                );

                base = nullptr;
                physicalAddress = 0;
                heapSize = 0;
                first = nullptr;
                initialized = false;
            }

            pthread_mutex_unlock(&mutex);
        }

        void* allocate(size_t size)
        {
            if (size == 0)
                return nullptr;

            if (size > SIZE_MAX - HEADER_SIZE - ALIGNMENT)
                return nullptr;

            size = alignUp(size, ALIGNMENT);

            pthread_mutex_lock(&mutex);

            if (!initialized)
            {
                pthread_mutex_unlock(&mutex);
                return nullptr;
            }

            Block* block = first;

            while (block)
            {
                if (block->magic != BLOCK_MAGIC)
                {
                    pthread_mutex_unlock(&mutex);
                    return nullptr;
                }

                if (block->free &&
                    block->size >= HEADER_SIZE + size)
                {
                    split(block, HEADER_SIZE + size);

                    block->free = false;

                    void* result =
                        reinterpret_cast<uint8_t*>(block)
                        + HEADER_SIZE;

                    pthread_mutex_unlock(&mutex);
                    return result;
                }

                block = block->next;
            }

            pthread_mutex_unlock(&mutex);
            return nullptr;
        }

        void deallocate(void* ptr)
        {
            if (!ptr)
                return;

            pthread_mutex_lock(&mutex);

            if (!initialized)
            {
                pthread_mutex_unlock(&mutex);
                return;
            }

            uintptr_t address =
                reinterpret_cast<uintptr_t>(ptr);

            uintptr_t start =
                reinterpret_cast<uintptr_t>(base);

            uintptr_t end = start + heapSize;

            if (address < start + HEADER_SIZE ||
                address >= end ||
                (address - start - HEADER_SIZE) % ALIGNMENT != 0)
            {
                pthread_mutex_unlock(&mutex);
                return;
            }

            Block* block = reinterpret_cast<Block*>(
                address - HEADER_SIZE
            );

            // Check that this is a known block, not just
            // arbitrary memory containing a matching magic.
            bool found = false;

            for (Block* current = first;
                 current;
                 current = current->next)
            {
                if (current == block)
                {
                    found = true;
                    break;
                }
            }

            if (!found ||
                block->magic != BLOCK_MAGIC ||
                block->free)
            {
                pthread_mutex_unlock(&mutex);
                return;
            }

            block->free = true;

            // Merge with the following free block.
            if (block->next && block->next->free)
            {
                mergeNext(block);
            }

            // Merge with the preceding free block.
            if (block->prev && block->prev->free)
            {
                block = block->prev;
                mergeNext(block);
            }

            pthread_mutex_unlock(&mutex);
        }

        void* calloc(size_t count, size_t size)
        {
            if (count != 0 &&
                size > SIZE_MAX / count)
            {
                return nullptr;
            }

            size_t total = count * size;

            void* ptr = allocate(total);

            if (ptr)
                std::memset(ptr, 0, total);

            return ptr;
        }

        void* reallocate(void* ptr, size_t newSize)
        {
            if (!ptr)
                return allocate(newSize);

            if (newSize == 0)
            {
                deallocate(ptr);
                return nullptr;
            }

            if (newSize > SIZE_MAX - HEADER_SIZE - ALIGNMENT)
                return nullptr;

            pthread_mutex_lock(&mutex);

            if (!initialized)
            {
                pthread_mutex_unlock(&mutex);
                return nullptr;
            }

            Block* block = reinterpret_cast<Block*>(
                static_cast<uint8_t*>(ptr) - HEADER_SIZE
            );

            bool found = false;

            for (Block* current = first;
                 current;
                 current = current->next)
            {
                if (current == block)
                {
                    found = true;
                    break;
                }
            }

            if (!found ||
                block->magic != BLOCK_MAGIC ||
                block->free)
            {
                pthread_mutex_unlock(&mutex);
                return nullptr;
            }

            size_t requested = alignUp(newSize, ALIGNMENT);

            size_t oldCapacity = block->size - HEADER_SIZE;

            if (requested <= oldCapacity)
            {
                split(block, HEADER_SIZE + requested);

                pthread_mutex_unlock(&mutex);
                return ptr;
            }

            // Try extending into the next free block.
            if (block->next &&
                block->next->free &&
                block->size + block->next->size >=
                    HEADER_SIZE + requested)
            {
                mergeNext(block);
                split(block, HEADER_SIZE + requested);

                pthread_mutex_unlock(&mutex);
                return ptr;
            }

            pthread_mutex_unlock(&mutex);

            // Allocate elsewhere and copy the old contents.
            void* newPtr = allocate(newSize);

            if (!newPtr)
                return nullptr;

            std::memcpy(
                newPtr,
                ptr,
                oldCapacity
            );

            deallocate(ptr);

            return newPtr;
        }

        size_t capacity() const
        {
            return heapSize;
        }

    private:

        void split(Block* block, size_t required)
        {
            if (block->size < required)
                return;

            size_t remaining = block->size - required;

            if (remaining < HEADER_SIZE + ALIGNMENT)
                return;

            Block* newBlock = reinterpret_cast<Block*>(
                reinterpret_cast<uint8_t*>(block) + required
            );

            newBlock->magic = BLOCK_MAGIC;
            newBlock->size = remaining;
            newBlock->free = true;

            newBlock->prev = block;
            newBlock->next = block->next;

            if (newBlock->next)
                newBlock->next->prev = newBlock;

            block->next = newBlock;
            block->size = required;
        }

        void mergeNext(Block* block)
        {
            Block* next = block->next;

            if (!next || !next->free)
                return;

            block->size += next->size;
            block->next = next->next;

            if (block->next)
                block->next->prev = block;

            next->magic = 0;
        }

        uint8_t* base;
        off_t physicalAddress;
        size_t heapSize;

        Block* first;

        bool initialized;
        pthread_mutex_t mutex;
    };

    // Global heap instance.
    static DirectHeap g_heap;

}

bool ps5_heap_initialize()
{
    return PS5Heap::g_heap.initialize(
        PS5_DIRECT_MEMORY_SIZE,
        0x100000000,
        PS5_DIRECT_MEMORY_TYPE
    );
}

void ps5_heap_shutdown()
{
    PS5Heap::g_heap.shutdown();
}
void* ps5_malloc(size_t size)
{
    return PS5Heap::g_heap.allocate(size);
}
void ps5_free(void* ptr)
{
    PS5Heap::g_heap.deallocate(ptr);
}
void* ps5_calloc(size_t count, size_t size)
{
    return PS5Heap::g_heap.calloc(count, size);
}
void* ps5_realloc(void* ptr, size_t size)
{
    return PS5Heap::g_heap.reallocate(ptr, size);
}

extern "C" void* malloc(size_t size)
{
    return ps5_malloc(size);
}

extern "C" void free(void* ptr)
{
    ps5_free(ptr);
}

extern "C" void * calloc(size_t count, size_t size)
{
    return ps5_calloc(count, size);
}

extern "C" void * realloc(void* ptr, size_t size)
{
    return ps5_realloc(ptr, size);
}
