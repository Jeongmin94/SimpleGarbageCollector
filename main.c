#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <mach-o/getsect.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <pthread.h>

// 16 bytes word aligned
typedef struct header
{
    unsigned int size;
    unsigned int dummy;
    struct header *next;
} header_t;

static header_t base;           // 크기가 0인 블록으로 자유 리스트의 시작을 표시하는 데 사용
static header_t *freep = &base; // 자유 리스트의 첫 블록을 가리킴
static header_t *usedp;         // 사용 중인 블록 리스트의 첫 번째 (현재 쓰이지 않음)

/*
 * 자유 리스트를 스캔하면서 블록을 넣을 위치를 찾는다.
 * 기본적으로, 우리가 찾는 것은 현재 해제하려는 블록이
 * 예전에 어떤 더 큰 블록에서 분할되었을 가능성이 있는 블록이다.
 */
static void
add_to_freelist(header_t *bp)
{
    header_t *p;

    for (p = freep; !(p < bp && bp < p->next); p = p->next)
        if (p >= p->next && (p < bp || bp < p->next))
            break;

    // bp를 넣었을 때, 다음 블록과 크기가 겹치는 경우에는 뒤 블록과 합침
    if (bp + bp->size == p->next)
    {
        bp->size += p->next->size;
        bp->next = p->next->next;
    }
    else
    {
        bp->next = p->next;
    }

    // bp의 앞이 이전 블록과 겹치는 경우, 앞 블록과 합침
    if (p + p->size == bp)
    {
        p->size += bp->size;
        p->next = bp->next;
    }
    else
    {
        p->next = bp;
    }

    freep = p;
}

#define MIN_ALLOC_SIZE 4096

/*
 * 커널에게 추가 메모리 요청
 */
static header_t *
morecore(size_t num_units)
{
    void *vp;
    header_t *up;

    if (num_units > MIN_ALLOC_SIZE)
        num_units = MIN_ALLOC_SIZE / sizeof(header_t);

    if ((vp == sbrk(num_units * sizeof(header_t))) == (void *)-1)
        return NULL;

    up = (header_t *)vp;
    up->size = num_units;
    add_to_freelist(up);

    return freep;
}

/*
 * 자유 리스트에서 청크를 찾고, 이것을 사용 리스트에 추가
 */
void *
GC_malloc(size_t alloc_size)
{
    size_t num_units;
    header_t *p, *prevp;

    // 요청한 바이트(alloc_size)를 블록 단위로 변환
    // alloc_size/sizeof(header_t) 하면 소수점이 날아가 블록이 부족할 수 있어 올림(ceil)
    // 헤더를 위한 블록 +1
    num_units = (alloc_size + sizeof(header_t) - 1) / sizeof(header_t) + 1;
    prevp = freep;

    for (p = prevp->next;; prevp = p, p = p->next)
    {
        // Big Enough
        if (p->size >= num_units)
        {
            // exact size
            if (p->size == num_units)
            {
                prevp->next = p->next;
            }
            else
            {
                p->size -= num_units;
                p += p->size;
                p->size = num_units;
            }

            freep = prevp;

            // add to p to the used list
            if (usedp == NULL)
            {
                usedp = p->next = p;
            }
            else
            {
                p->next = usedp->next;
                usedp->next = p;
            }

            return (void *)(p + 1);
        }

        // not enough memory
        // p == freep: 원형 링크드 리스트 순회하여 처음으로 돌아온 경우
        // 메모리가 부족하여 새로운 메모리 할당 요청
        if (p == freep)
        {
            p = morecore(num_units);
            // request for more memory failed
            if (p == NULL)
                return NULL;
        }
    }
}

// header_t는 world-alinged 되어 있으므로, 하위 몇 비트는 항상 0이 된다.
// 따라서 이 중 가장 하위 비트(least significant bit)를 사용하여
// 현재 블록이 마크되었는지를 표시할 수 있다.
// 0xfffffffc
// 1111 1111 1111 1111 1111 1111 1111 1100
#define UNTAG(p) ((header_t *)(((uintptr_t)(p)) & 0xfffffffc))

/*
 * 메모리의 특정 영역을 스캔하여, 사용 리스트(used list)에 있는 항목들을 적절히 마크한다.
 * 두 인자(start, end)는 모두 워드(word) 정렬되어 있어야 한다.
 */
static void
scan_region(uintptr_t *sp, uintptr_t *end)
{
    header_t *bp;

    for (; sp < end; sp++)
    {
        uintptr_t v = *sp;
        bp = usedp;
        do
        {
            if (bp + 1 <= v && v < bp + 1 + bp->size)
            {
                bp->next = (header_t *)(((uintptr_t)bp->next) | 1);
                break;
            }
        } while ((bp = UNTAG(bp->next)) != usedp);
    }
}

/**
 * 마크된 블록을 스캔하여, 그 안에 아직 마크되지 않은 다른 블록을 참조하고 있는지 확인한다.
 */
static void
scan_heap(void)
{
    uintptr_t *vp;
    header_t *bp, *up;

    for (bp = UNTAG(usedp->next); bp != usedp; bp = UNTAG(bp->next))
    {
        // 태깅되지 않은 경우
        if (!((uintptr_t)bp->next & 1))
            continue;

        // 태깅된 경우, bp+1(헤더 다음 영역)부터 한 블록씩 태깅된 영역이 있는지 확인
        for (vp = (uintptr_t *)(bp + 1);
             vp < (bp + bp->size + 1);
             vp++)
        {
            // 현재 데이터 v가 다른 블록 up의 데이터 영역에 있는지 확인하는 부분
            // mark propagation
            uintptr_t v = *vp;
            up = UNTAG(bp->next);
            do
            {
                if (up != bp &&
                    up + 1 <= v && v < up + 1 + up->size)
                {
                    up->next = ((header_t *)(((uintptr_t)up->next) | 1));
                    break;
                }
            } while ((up = UNTAG(up->next)) != bp);
        }
    }
}

uintptr_t stack_bottom;

/**
 * 스택의 가장 바닥 주소를 찾아내고, 가비지 컬렉션에 필요한 자료구조를 설정한다.
 */
void GC_init(void)
{
    static int initted;

    if (initted)
        return;

    initted = 1;

    // macOS에서는 pthread API를 사용하여 스택 정보를 얻음
    pthread_t self = pthread_self();
    void *stack_addr = pthread_get_stackaddr_np(self);
    size_t stack_size = pthread_get_stacksize_np(self);

    // stack_bottom을 스택의 가장 높은 주소(시작점)로 정의
    stack_bottom = (uintptr_t)stack_addr;

    printf("Stack information - addr: %p, allocated size: %zu, bottom(highest): %p\n",
           stack_addr, stack_size, (void *)stack_bottom);
    printf("Theoretical lowest address: %p\n", (void *)(stack_bottom - stack_size));

    usedp = NULL;
    base.next = freep = &base;
    base.size = 0;
}

/**
 * 현재 사용 중인 메모리 블록을 마크하고,
 * 사용되지 않은 블록들은 해제하여(free) 다시 사용할 수 있도록 만든다.
 */
void GC_collect(void)
{
    printf("function GC_collect(void) called\n");
    header_t *p, *prevp, *tp;
    uintptr_t stack_top;

    // macOS에서는 링커가 제공하는 심볼 대신 getsectbyname을 사용
    unsigned long data_size, bss_size;
    uintptr_t *data_start, *bss_start;

    // DATA 세그먼트 가져오기
    const struct section_64 *data_sect = getsectbyname("__DATA", "__data");
    if (data_sect)
    {
        data_start = (uintptr_t *)(data_sect->addr);
        data_size = data_sect->size;
    }
    printf("DATA segments and size: %p, %zu\n", data_start, data_size);

    // BSS 세그먼트 가져오기
    const struct section_64 *bss_sect = getsectbyname("__DATA", "__bss");
    if (bss_sect)
    {
        bss_start = (uintptr_t *)(bss_sect->addr);
        bss_size = bss_sect->size;
    }
    printf("BSS segments and size: %p, %zu\n", bss_start, bss_size);

    /* Scan the stack. */
    // 올바른 스택 탑 주소 획득
    stack_top = (uintptr_t)__builtin_frame_address(0);
    printf("Stack top address (current frame): %p\n", (void *)stack_top);
    printf("Currently used stack size: %zu bytes\n", (size_t)(stack_bottom - stack_top));

    if (usedp == NULL)
        return;

    /* Scan the BSS and initialized data segments. */
    if (data_sect)
    {
        scan_region(data_start, (uintptr_t *)((char *)data_start + data_size));
    }
    if (bss_sect)
    {
        scan_region(bss_start, (uintptr_t *)((char *)bss_start + bss_size));
    }

    /* Scan the stack. */
    // Apple Silicon에서 스택은 높은 주소에서 낮은 주소로 자라기 때문에
    // stack_bottom이 높은 주소이고 stack_top이 낮은 주소
    printf("Scanning stack region from %p to %p (%zu bytes)\n",
           (void *)stack_top, (void *)stack_bottom, (size_t)(stack_bottom - stack_top));
    scan_region((uintptr_t *)stack_top, (uintptr_t *)stack_bottom);

    /* Mark from the head. */
    scan_heap();

    /* And now we collect! */
    for (prevp = usedp, p = UNTAG(usedp->next);; prevp = p, p = UNTAG(p->next))
    {
    next_chunk:
        if (!(unsigned int)p->next & 1)
        {
            // the chunk hasn't been marked. thus, it must be set free
            tp = p;
            p = UNTAG(p->next);
            add_to_freelist(tp);

            if (usedp == tp)
            {
                usedp = NULL;
                break;
            }

            prevp->next = (header_t *)((uintptr_t)p | ((uintptr_t)prevp->next & 1));
            goto next_chunk;
        }
        // 다음 GC를 위해 마크 비트 초기화
        p->next = (header_t *)(((uintptr_t)p->next) & ~1);
        if (p == usedp)
            break;
    }
}

int main()
{
    GC_init();
    GC_collect();
    return 0;
}