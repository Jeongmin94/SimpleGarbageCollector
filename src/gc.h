#ifndef GC_H
#define GC_H

#include <stddef.h>
#include <stdint.h>

// 16 bytes word aligned
typedef struct header
{
    unsigned int size;
    unsigned int dummy;
    struct header *next;
} header_t;

/**
 * 가비지 컬렉터 초기화 함수.
 * 스택의 바닥 주소를 찾고 필요한 자료구조를 설정한다.
 */
void GC_init(void);

/**
 * 메모리 할당 함수.
 * 가비지 콜렉터가 추적할 수 있는 메모리를 할당한다.
 *
 * @param size 할당할 메모리의 크기(바이트)
 * @return 할당된 메모리의 포인터, 실패 시 NULL
 */
void *GC_malloc(size_t size);

/**
 * 가비지 콜렉션 수행 함수.
 * 사용하지 않는 메모리를 식별하고 해제한다.
 */
void GC_collect(void);

#endif // GC_H