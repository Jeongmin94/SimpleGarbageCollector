#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 힙 체커 활성화
#define USE_HEAP_CHECKER
#include "../src/heap_checker.h"

// 필요한 가비지 콜렉터 함수 선언
extern void GC_init(void);
extern void *GC_malloc(size_t size);
extern void GC_collect(void);

// 간단한 테스트 구조체
typedef struct Node
{
    int value;
    struct Node *next;
} Node;

// 연결 리스트 생성 함수
Node *create_list(int count)
{
    Node *head = NULL;
    Node *current = NULL;

    for (int i = 0; i < count; i++)
    {
        Node *new_node = GC_malloc(sizeof(Node));
        TRACK_ALLOC(new_node, sizeof(Node)); // 힙 체커로 추적

        new_node->value = i;
        new_node->next = NULL;

        if (head == NULL)
        {
            head = new_node;
            current = new_node;
        }
        else
        {
            current->next = new_node;
            current = new_node;
        }
    }

    return head;
}

// 연결 리스트 출력 함수
void print_list(Node *head)
{
    printf("List: ");
    for (Node *curr = head; curr != NULL; curr = curr->next)
    {
        printf("%d -> ", curr->value);
    }
    printf("NULL\n");
}

// 메모리 누수 테스트: 일부 노드는 접근할 수 없게 만듦
void test_memory_leak()
{
    printf("\n=== 메모리 누수 테스트 ===\n");

    // 리스트 생성 전 할당 확인
    heap_checker_print_stats();

    // 리스트 생성
    Node *list = create_list(10);
    printf("Created linked list with 10 nodes\n");
    print_list(list);

    // 일부러 연결을 끊어 메모리 누수 발생
    Node *leak_point = list;
    for (int i = 0; i < 3; i++)
    {
        if (leak_point)
            leak_point = leak_point->next;
    }

    if (leak_point)
    {
        Node *leaked_nodes = leak_point->next;
        leak_point->next = NULL; // 의도적으로 연결 끊기

        printf("연결 리스트 중간 부분 연결 해제\n");
        print_list(list);

        // 사용자가 직접 참조하지 않는 노드는 힙 체커에서 누수로 감지 가능
        heap_checker_print_stats();

        // GC 수행 전 안전 요소 추가
        printf("가비지 콜렉션 준비...\n");

        // GC 전 더미 데이터 몇 개 할당하고 사용
        int *dummy1 = GC_malloc(16);
        TRACK_ALLOC(dummy1, 16);
        if (dummy1)
            dummy1[0] = 123;

        int *dummy2 = GC_malloc(32);
        TRACK_ALLOC(dummy2, 32);
        if (dummy2)
        {
            dummy2[0] = 456;
            dummy2[1] = 789;
        }

        printf("가비지 콜렉션 실행...\n");
        GC_collect();

        // 가비지 콜렉션 후에도 힙 체커는 여전히 모든 할당을 추적함
        heap_checker_print_stats();
    }
}

// 정상적인 메모리 사용 테스트
void test_normal_allocation()
{
    printf("\n=== 정상 할당 테스트 ===\n");

    // 데이터 생성 및 사용
    int *numbers = GC_malloc(10 * sizeof(int));
    TRACK_ALLOC(numbers, 10 * sizeof(int));

    for (int i = 0; i < 10; i++)
    {
        numbers[i] = i * 10;
    }

    printf("할당된 배열: ");
    for (int i = 0; i < 10; i++)
    {
        printf("%d ", numbers[i]);
    }
    printf("\n");

    heap_checker_print_stats();
    heap_checker_dump();
}

int main()
{
    // 초기화
    GC_init();
    heap_checker_init();

    printf("=== 가비지 콜렉터 + 힙 체커 테스트 ===\n\n");

    // 테스트 케이스 실행
    test_normal_allocation();
    test_memory_leak();

    // 최종 메모리 상태
    printf("\n=== 최종 메모리 상태 ===\n");
    heap_checker_check_leaks();

    return 0;
}