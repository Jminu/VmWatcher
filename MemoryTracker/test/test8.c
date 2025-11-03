#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h> 
#include <sys/time.h> // gettimeofday와 struct timeval을 위해 필요

// --- 설정 상수 ---
#define BLOCK_SIZE_KB 128        // 할당/쓰기 단위 크기 (128 KB, mmap 유발)
#define BLOCK_SIZE (BLOCK_SIZE_KB * 1024)
#define MAX_BLOCKS 1000      // ★ 총 할당 블록 수 (6,000회)
#define ACTION_DELAY_US 5000 // ★ 각 동작 사이 지연 (5ms)

/*
 * 시간 측정 함수 (★ tv_nsec -> tv_usec 오류 수정 완료)
 */
long long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    // tv_usec (마이크로초)를 1000으로 나누어 밀리초로 변환
    return ((long long)tv.tv_sec * 1000) + (tv.tv_usec / 1000); 
}

/*
 * PID를 출력하고 모니터가 준비될 시간을 줍니다.
 */
void print_self_pid() {
    printf("=========================================\n");
    printf("[TEST START] High-Density Stress Test\n");
    printf("[TEST START] Self PID: %d\n", getpid());
    printf("[TEST] Waiting 7 seconds for monitor setup...\n"); 
    printf("=========================================\n");
    fflush(stdout);
    sleep(7); 
}

/*
 * 고부하 정적 메모리 테스트를 순차적으로 실행합니다.
 */
void static_stress_test() {
    char *memory_blocks[MAX_BLOCKS] = {NULL}; 
    int i;
    const long delay_us = ACTION_DELAY_US;

    printf("\n--- Starting High-Density Stress Test (1ms delay) ---\n");
    fflush(stdout);

    // --- Phase 1: Allocate 6000 blocks (VmSize 증가) ---
    printf("\n--- Phase 1: Allocating %d blocks (Expected 6.0s) ---\n", MAX_BLOCKS);
    fflush(stdout);
    for (i = 0; i < MAX_BLOCKS; i++) {
        memory_blocks[i] = (char *)malloc(BLOCK_SIZE);
        if (memory_blocks[i] == NULL) {
            perror("malloc failed");
            goto cleanup; 
        }
        usleep(delay_us); // 1ms 지연
    }
    int allocated_count = MAX_BLOCKS;

    printf("\n--- Phase 1 Finished. Waiting 1 second... ---\n");
    fflush(stdout);
    sleep(1);

    // --- Phase 2: Write to 6000 blocks (VmRSS 증가) ---
    printf("\n--- Phase 2: Writing to %d blocks (Expected 6.0s) ---\n", MAX_BLOCKS);
    fflush(stdout);
    for (i = 0; i < allocated_count; i++) {
        // 이 부분에서 Page Fault가 연속적으로 발생합니다.
        memset(memory_blocks[i], i, BLOCK_SIZE); 
        usleep(delay_us); // 1ms 지연
    }

    printf("\n--- Phase 2 Finished. Waiting 3 seconds (Stable Check)... ---\n");
    fflush(stdout);
    sleep(3);

cleanup:
    // --- Phase 3: 메모리 해제 (munmap 확인) ---
    printf("\n--- Phase 3: Freeing Memory (%d blocks) (Expected 6.0s) ---\n", allocated_count);
    fflush(stdout);
    for (i = 0; i < allocated_count; i++) {
        if (memory_blocks[i] != NULL) {
            free(memory_blocks[i]); 
            usleep(delay_us); // 1ms 지연
        }
    }
    printf("\n[END] Test finished. Exiting.\n");
    fflush(stdout);
}

int main() {
    print_self_pid();
    static_stress_test(); 
    sleep(1); 
    return 0;
}
