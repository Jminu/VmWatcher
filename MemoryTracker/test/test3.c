#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getpid, usleep
#include <string.h> // memset
#include <time.h>   // time, srand
#include <sys/time.h> // gettimeofday for duration check

// --- 설정 상수 ---
#define BLOCK_SIZE_KB 128        // 할당/쓰기 단위 크기 (128 KB)
#define BLOCK_SIZE (BLOCK_SIZE_KB * 1024)
#define MAX_BLOCKS 100       // 최대 할당할 블록 수 (약 12.8 MB)
#define ACTION_DELAY_US 50000 // 각 동작(할당/쓰기) 사이 지연 (50ms)
#define TEST_DURATION_SEC 10 // 총 테스트 시간 (약 10초)

// 시간 측정 함수
long long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((long long)tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

void print_self_pid() {
    printf("=========================================\n");
    printf("[TEST START] Mixed Alloc/Write Test\n");
    printf("[TEST START] Self PID: %d\n", getpid());
    printf("[TEST] Running for approx %d seconds...\n", TEST_DURATION_SEC);
    printf("[TEST] Waiting 3 seconds for monitor setup...\n");
    printf("=========================================\n");
    fflush(stdout);
    sleep(10);
}

void mixed_memory_test() {
    char *memory_blocks[MAX_BLOCKS] = {NULL}; // 포인터 배열 초기화
    int allocated_count = 0; // 현재 할당된 블록 수
    int written_count = 0;   // 쓰기가 완료된 블록 수 (중복 쓰기 가능)

    srand(time(NULL));
    long long start_time_ms = get_current_time_ms();
    long long end_time_ms = start_time_ms + (TEST_DURATION_SEC * 1000);

    printf("\n--- Starting Mixed Allocation and Write Loop ---\n");

    while (get_current_time_ms() < end_time_ms) {
        // 랜덤하게 할당 또는 쓰기 결정 (할당 확률 약간 높게)
        int action = rand() % 10; // 0~9 사이 난수 생성

        if ((action < 6 || written_count == allocated_count) && allocated_count < MAX_BLOCKS) {
            // --- 할당 (Action: 0-5) ---
            printf("[ALLOC %d] Allocating %d KB...\n", allocated_count + 1, BLOCK_SIZE_KB);
            fflush(stdout);

            memory_blocks[allocated_count] = (char *)malloc(BLOCK_SIZE);
            if (memory_blocks[allocated_count] == NULL) {
                perror("malloc failed during test");
                // Clean up allocated blocks before exiting
                for(int i=0; i < allocated_count; ++i) free(memory_blocks[i]);
                exit(EXIT_FAILURE);
            }
            allocated_count++;
            // VmSize 증가 확인

        } else if (allocated_count > 0) {
            // --- 쓰기 (Action: 6-9) ---
            // 이미 할당된 블록 중 랜덤하게 하나 선택하여 쓰기
            int block_index = rand() % allocated_count;

            // 이미 쓰여진 블록이라도 다시 쓸 수 있음
            printf("[WRITE %d] Writing to block #%d...\n", written_count + 1, block_index + 1);
            fflush(stdout);

            // 🚨 VmRSS 증가 강제
            if (memory_blocks[block_index] != NULL) {
                memset(memory_blocks[block_index], (rand() % 256), BLOCK_SIZE);
                written_count++;
            } else {
                 printf("[WARN] Attempted write to NULL block index %d\n", block_index);
            }
            // VmRSS 증가 확인

        } else {
             // 할당된 블록이 없으면 할당만 시도
             usleep(ACTION_DELAY_US);
             continue;
        }

        // 각 동작 후 짧은 지연
        usleep(ACTION_DELAY_US);
    }

    printf("\n--- Loop Finished (approx %d seconds) ---\n", TEST_DURATION_SEC);
    printf("Total blocks allocated: %d\n", allocated_count);
    printf("Total write operations: %d\n", written_count);
    printf("--------------------------------------------------\n");
    printf("[TEST END] Waiting 3 seconds before cleanup...\n");
    fflush(stdout);
    sleep(3);

    // --- 메모리 해제 ---
    printf("\n--- STEP 3: Freeing Memory ---\n");
    for (int i = 0; i < allocated_count; i++) {
        if (memory_blocks[i] != NULL) {
            free(memory_blocks[i]);
        }
    }
    printf("[END] Test finished. Exiting.\n");
    fflush(stdout);
}

int main() {
    print_self_pid();
    mixed_memory_test();
    sleep(1);
    return 0;
}
