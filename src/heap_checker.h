#ifndef HEAP_CHECKER_H
#define HEAP_CHECKER_H

#include <stddef.h>
#include <stdbool.h>

// 메모리 할당 추적 초기화
void heap_checker_init(void);

// 메모리 할당 등록 (기본)
void heap_checker_track_alloc(void *ptr, size_t size);

// 메모리 할당 등록 (확장 - 파일과 라인 정보 포함)
void heap_checker_track_alloc_ex(void *ptr, size_t size, const char *file, int line);

// 메모리 해제 등록
void heap_checker_track_free(void *ptr);

// 누수 검사 실행
bool heap_checker_check_leaks(void);

// 현재 할당된 메모리 통계 출력
void heap_checker_print_stats(void);

// 메모리 사용 현황 덤프
void heap_checker_dump(void);

// 힙 체커 매크로 정의
#ifdef USE_HEAP_CHECKER
// 위치 정보를 포함한 메모리 추적
#define TRACK_ALLOC(ptr, size) heap_checker_track_alloc_ex(ptr, size, __FILE__, __LINE__)
#define TRACK_FREE(ptr) heap_checker_track_free(ptr)
#else
// 빈 매크로 (추적 비활성화)
#define TRACK_ALLOC(ptr, size)
#define TRACK_FREE(ptr)
#endif

#endif // HEAP_CHECKER_H
