#include "heap_checker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ALLOCATIONS 10000

typedef struct
{
    void *ptr;        // 할당된 메모리 포인터
    size_t size;      // 할당 크기
    char tag[32];     // 선택적 태그 (디버깅용)
    const char *file; // 호출 소스 파일
    int line;         // 호출 라인 번호
} allocation_info_t;

static allocation_info_t allocations[MAX_ALLOCATIONS];
static size_t allocation_count = 0;
static size_t total_allocated = 0;
static size_t peak_allocated = 0;
static size_t total_allocations = 0;
static size_t total_frees = 0;

void heap_checker_init(void)
{
    memset(allocations, 0, sizeof(allocations));
    allocation_count = 0;
    total_allocated = 0;
    peak_allocated = 0;
    total_allocations = 0;
    total_frees = 0;
    printf("[HEAP_CHECKER] Initialized\n");
}

void heap_checker_track_alloc_ex(void *ptr, size_t size, const char *file, int line)
{
    if (!ptr)
        return;

    if (allocation_count >= MAX_ALLOCATIONS)
    {
        fprintf(stderr, "[HEAP_CHECKER] Error: Too many allocations to track\n");
        return;
    }

    allocations[allocation_count].ptr = ptr;
    allocations[allocation_count].size = size;
    allocations[allocation_count].file = file;
    allocations[allocation_count].line = line;

    allocation_count++;
    total_allocated += size;
    total_allocations++;

    if (total_allocated > peak_allocated)
    {
        peak_allocated = total_allocated;
    }
}

void heap_checker_track_alloc(void *ptr, size_t size)
{
    heap_checker_track_alloc_ex(ptr, size, "unknown", 0);
}

void heap_checker_track_free(void *ptr)
{
    if (!ptr)
        return;

    for (size_t i = 0; i < allocation_count; i++)
    {
        if (allocations[i].ptr == ptr)
        {
            total_allocated -= allocations[i].size;
            total_frees++;

            // 배열에서 항목 제거 (마지막 항목으로 교체하고 카운트 감소)
            allocations[i] = allocations[allocation_count - 1];
            allocation_count--;
            return;
        }
    }

    // 찾지 못한 경우 에러 출력
    fprintf(stderr, "[HEAP_CHECKER] Warning: Attempting to free untracked pointer %p\n", ptr);
}

bool heap_checker_check_leaks(void)
{
    if (allocation_count == 0)
    {
        printf("[HEAP_CHECKER] No memory leaks detected!\n");
        return true;
    }

    printf("[HEAP_CHECKER] Memory leaks detected! %zu allocations not freed:\n", allocation_count);

    for (size_t i = 0; i < allocation_count; i++)
    {
        printf("  %p: %zu bytes allocated at %s:%d\n",
               allocations[i].ptr,
               allocations[i].size,
               allocations[i].file,
               allocations[i].line);
    }

    return false;
}

void heap_checker_print_stats(void)
{
    printf("[HEAP_CHECKER] Memory Statistics:\n");
    printf("  Current allocations: %zu\n", allocation_count);
    printf("  Current bytes allocated: %zu\n", total_allocated);
    printf("  Peak bytes allocated: %zu\n", peak_allocated);
    printf("  Total allocations: %zu\n", total_allocations);
    printf("  Total frees: %zu\n", total_frees);
}

void heap_checker_dump(void)
{
    printf("[HEAP_CHECKER] Current Heap Contents (%zu allocations):\n", allocation_count);

    for (size_t i = 0; i < allocation_count; i++)
    {
        printf("  [%zu] %p: %zu bytes (from %s:%d)\n",
               i,
               allocations[i].ptr,
               allocations[i].size,
               allocations[i].file,
               allocations[i].line);

        // 메모리 내용 헥사 덤프 (선택적으로 추가)
        unsigned char *data = (unsigned char *)allocations[i].ptr;
        size_t dump_size = allocations[i].size > 32 ? 32 : allocations[i].size;

        printf("      Data: ");
        for (size_t j = 0; j < dump_size; j++)
        {
            printf("%02x ", data[j]);
            if (j % 8 == 7)
                printf(" ");
        }
        printf("\n");
    }
}
