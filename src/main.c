#include <stdio.h>
#include "gc.h"
#include "heap_checker.h"

int main()
{
    // 가비지 콜렉터 초기화
    GC_init();

    // 힙 체커 초기화
    heap_checker_init();

    // 메모리 누수 확인
    heap_checker_check_leaks();

    return 0;
}