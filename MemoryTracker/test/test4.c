#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getpid, usleep
#include <string.h> // memset
#include <sys/time.h> // gettimeofday

// --- 설정 상수 ---
#define BLOCK_SIZE_KB 128        // 할당/쓰기 단위 크기 (128 KB)
#define BLOCK_SIZE (BLOCK_SIZE_KB * 1024)
#define MAX_BLOCKS 100       // 최대 할당할 블록 수
#define ACTION_DELAY_MS 250 // 각 동작 사이 지연 (250ms)

/*
 * PID를 출력하고 모니터가 준비될 시간을 줍니다. (7초)
 */
void print_self_pid() {
    printf("=========================================\n");
    printf("[TEST START] Static Alloc/Write Test (Approx 10s run)\n");
    printf("[TEST START] Self PID: %d\n", getpid());
    printf("[TEST] Waiting 7 seconds for monitor setup...\n"); // (★) 7초로 수정
    printf("=========================================\n");
    fflush(stdout);
    sleep(7); // (★) 7초로 수정
}

/*
 * 랜덤 대신 정적인 순서로 메모리를 할당하고 씁니다.
 */
void static_memory_test() {
    char *memory_blocks[MAX_BLOCKS] = {NULL}; // 포인터 배열 초기화
    int allocated_count = 0;
    int written_count = 0;
    int i;
    const long delay_us = ACTION_DELAY_MS * 1000; // usleep은 마이크로초 단위

    printf("\n--- Starting Static Allocation and Write Test ---\n");
    fflush(stdout);

    // --- Phase 1: Alloc 5 blocks (약 1.25초) ---
    printf("\n--- Phase 1: Allocating 5 blocks ---\n");
    fflush(stdout);
    for (i = 0; i < 5 && allocated_count < MAX_BLOCKS; i++) {
        printf("[ALLOC %d]\n", allocated_count + 1);
        fflush(stdout);
        memory_blocks[allocated_count] = (char *)malloc(BLOCK_SIZE);
        if (memory_blocks[allocated_count] == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        allocated_count++;
        usleep(delay_us); 
    }

    printf("\n--- Phase 1 Finished. Waiting 1 second... ---\n");
    fflush(stdout);
    sleep(1); // (★) 1초 대기

    // --- Phase 2: Write to 5 blocks (약 1.25초) ---
    printf("\n--- Phase 2: Writing to 5 blocks ---\n");
    fflush(stdout);
    for (i = 0; i < allocated_count; i++) {
        printf("[WRITE %d] to block #%d\n", written_count + 1, i + 1);
        fflush(stdout);
        memset(memory_blocks[i], i, BLOCK_SIZE); // Page Fault
        written_count++;
        usleep(delay_us);
    }

    printf("\n--- Phase 2 Finished. Waiting 1 second... ---\n");
    fflush(stdout);
    sleep(1); // (★) 1초 대기

    // --- Phase 3: Allocate & Write 5 new blocks (약 2.5초) ---
    printf("\n--- Phase 3: Allocating and Writing 5 new blocks ---\n");
    fflush(stdout);
    for (i = 0; i < 5 && allocated_count < MAX_BLOCKS; i++) {
        // 1. 할당 (mmap)
        printf("[ALLOC %d]\n", allocated_count + 1);
        fflush(stdout);
        memory_blocks[allocated_count] = (char *)malloc(BLOCK_SIZE);
        if (memory_blocks[allocated_count] == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        usleep(delay_us);
        
        // 2. 쓰기 (Page Fault)
        printf("[WRITE %d] to new block #%d\n", written_count + 1, allocated_count + 1);
        fflush(stdout);
        memset(memory_blocks[allocated_count], i + 10, BLOCK_SIZE);
        
        allocated_count++;
        written_count++;
        usleep(delay_us);
    }

    printf("\n--- Test Finished (Total %d blocks) ---\n", allocated_count);
    printf("[TEST END] Holding memory for 2 seconds before cleanup...\n");
    fflush(stdout);
    sleep(2); // (★) 2초 대기

    // --- Phase 4: Freeing Memory (munmap) (약 2.5초) ---
    printf("\n--- Phase 4: Freeing Memory (munmap) ---\n");
    fflush(stdout);
    for (i = 0; i < allocated_count; i++) { // 총 10개 블록 해제
        if (memory_blocks[i] != NULL) {
            printf("[FREE %d]\n", i + 1);
            fflush(stdout);
            free(memory_blocks[i]); // munmap 발생
            usleep(delay_us);
        }
    }
    printf("[END] Test finished. Exiting.\n");
    fflush(stdout);
}

int main() {
    print_self_pid();
    static_memory_test(); 
    sleep(3);
    return 0;
}
