#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include "proc_search.h"
#include "graph.h"
#include "type.h"
#include "mem.h"
#include "ui.h"
#include "log.h"
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>

// 커널과 동일한 프로토콜 ID 및 구조체 정의
#define NETLINK_JMW 30
#define MAX_PAYLOAD 1024 // max message payload size 
#define NL_MSG_REG 1

// Pipe
#define READ_PIPE 0
#define WRITE_PIPE 1

static struct syscall_data {
    pid_t pid;
	char syscall_name[10];
};

static void send_registration(int nl_socket_fd, struct sockaddr_nl *src_addr) {
    struct nlmsghdr *nlh = NULL;
    struct sockaddr_nl dest_addr;
    struct iovec iov;
    struct msghdr msg;

    char *reg_msg = "REGISTER_APP";
    int reg_len = strlen(reg_msg) + 1; // reg_len = 13

    
    /*
     *	allocate message buffer for Register Message to Main Kernel
     */ 
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(reg_len));
    if (!nlh) {
        perror("[USER] Error: Failed to allocate registration buffer");
        return;
    }

    /*
     *	memset -> message buffer initialize with 0
     *	len
     *	type
     *	flags 지정
     *
     *	nlmsg_pid = 발신자 pid : 커널쪽에서 monitor_pid로 저장
     */ 
    memset(nlh, 0, NLMSG_SPACE(reg_len));
    nlh->nlmsg_len = NLMSG_SPACE(reg_len);
    nlh->nlmsg_type = NL_MSG_REG;
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_pid = src_addr->nl_pid;

    //copy data to payload 
    strcpy(NLMSG_DATA(nlh), reg_msg);

    /*
     * 	set destination
     * 	destination is Main Kernel (PID = 0)
     */ 
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0; // 커널의 PID
    dest_addr.nl_groups = 0;

    // msg 구조체 설정
    memset(&iov, 0, sizeof(iov));
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // 전송
    if (sendmsg(nl_socket_fd, &msg, 0) < 0) {
        perror("[USER] Error: Failed to send registration message");
    } else {
	    log_msg("Registration message sent successfully (PID: %d)", src_addr->nl_pid);
    }

    free(nlh);
}

static int set_nl_socket() {
	int nl_socket_fd;
	struct sockaddr_nl src_addr;
	struct sockaddr_nl dest_addr;

	// Kernel에서 데이터 수신받은거 저장할 버퍼
	char buffer[MAX_PAYLOAD];
	struct syscall_data *received_data;

	/*
	 *	Create Netlink Socket
	 *	use AF_NETLINK, SOCK_RAW, NETLINK_JMW=protocol 30
	 *
	 */

	log_msg("Creating Netlink Listener (protocal: %d)", NETLINK_JMW);
	nl_socket_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_JMW); // netlink socket fd 생성
	if (nl_socket_fd < 0) {
		perror("[USER] Error : Failed to create Netlink Socket");
		return -1;
	}
	
	log_msg("Complete Create Socket");
	// sleep(1);

	/*
	 * 	memset으로 src_addr 0으로 초기화
	 * 	nl_family를 AF_NETLINK
	 * 	nl_pid : 송신자 pid - 커널쪽 netlink에 알려줌
	 * 	nl_groups = 0 : 유니캐스트
	 */
	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();
	src_addr.nl_groups = 0;

	/*
	 *	(struct sockaddr *)&src_addr : IP주소, port번호 저장하기 위한 변수 구조체
	 *	sizeof(src_addr) : 두번째 인자의 데이터 크기
	 *
	 */
	if (bind(nl_socket_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
		perror("[USER] Error : Failed to bind Netlink Socket");
		close(nl_socket_fd);
		return -1;
	}
	log_msg("Complete Bind");
	// sleep(1);

	send_registration(nl_socket_fd, &src_addr); // Kernel쪽에 소켓 등록요청
	log_msg("Registration Complete Netlink Socket to Kernel");
	// sleep(1);

	return nl_socket_fd;
}

/*
 *  Parent Proc
 */
static void listen_syscall(FILE *log_fd) {
	signal(SIGPIPE, SIG_IGN); // SIGPIPE 시그널(자식 종료 시그널) 무시

	int nl_socket_fd = 0;
	struct nlmsghdr *nlh = NULL;
	struct iovec iov;
	struct msghdr msg;

	pid_t hooked_pid = -1;
	char hooked_syscall[10];

	struct syscall_data *received_data;

	pid_t pid;
	printf("[Watch PID]: ");
	scanf("%d", &pid); // 관찰하려는 프로세스 PID입력

	nl_socket_fd = set_nl_socket(); // get netlink socket fd

	/*
	 *	buffer allocation for Kernel Message
	 *	payload will continue behind this header
	 */
	nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD)); // header 설정
	if (!nlh) {
		perror("[USER] Error : Failed to allocate NLMSG buffer");
		free(nlh);
		close(nl_socket_fd);
		exit(1);	
	}

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = (void *)nlh;
	iov.iov_len = NLMSG_SPACE(MAX_PAYLOAD);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	log_msg("Complete Listening Setting");
	// sleep(1);
	printf("[USER] Listening...\n");
	sleep(1);
	clear_screen();

	/*
	 *	관찰 프로세스 /proc/[pid]경로 생성
	 */
	char monitored_pid[16];
	sprintf(monitored_pid, "%d", pid);
	char monitored_proc_path[64];
	snprintf(monitored_proc_path, sizeof(monitored_proc_path), "/proc/%s", monitored_pid);

	while (1) {
		if (access(monitored_proc_path, F_OK) == -1) { // 관찰중인 프로세스가 살아있는지 검사
			break;
		}

		int len = recvmsg(nl_socket_fd, &msg, 0); // 커널에서 메세지 대기중..

		if (len < 0) {
			perror("[USER] Error during recvmsg");
			free(nlh);
			close(nl_socket_fd);
			exit(1);
		}

		received_data = (struct syscall_data*)NLMSG_DATA(nlh);
		hooked_pid = received_data->pid;

		if (hooked_pid != pid) { // 관찰중인 프로세스 아니라면 생략
			continue;
		}

		strcpy(hooked_syscall, received_data->syscall_name);


		static long cnt_total = 0;
		static long cnt_brk = 0;
		static long cnt_mmap = 0;
		static long cnt_munmap = 0;
		static long cnt_page_fault = 0;

		cursor_to(1, 1); // (1) - (1, 1)로 이동
		clear_line_n2m(1, 50); // (2) - 1열부터 50열까지 지움
		cursor_to(1, 1); // (3) - 다시 (1, 1)로 이동


		FILE *status_fd = open_proc_stat(hooked_pid); // 관찰중인 프로세스 열어봄
		if (status_fd == NULL) {
			cursor_to(17, 1);
			printf("[CHILD] 관찰중인 프로세스가 종료됨\n");
			break;
		}

		MEM_INFO mem_info = get_mem_info(status_fd);
		fclose(status_fd);

		if (strcmp(received->syscall_name, "brk") == 0) {
			cnt_brk++;
		}
		else if (strcmp(received->syscall_name, "mmap") == 0) {
			cnt_mmap++;
		}
		else if (strcmp(received->syscall_name, "munmap") == 0) {
			cnt_munmap++;
		}
		else {
			cnt_page_fault++;
		}

		clear_line_n2m(1, 50);
		cursor_to(2, 1);
		log_msg_file(log_fd, "[RECEIVED] %s", received->syscall_name);

		clear_line_n2m(1, 50);
		cursor_to(3, 1);
		log_msg_file(log_fd, "[HOOKED PID] %d", received->pid);

		clear_line_n2m(1, 50);
		cursor_to(4, 1);
		log_msg_file(log_fd, "[brk]: %ld [mmap]: %ld [munmap]: %ld [page fault]: %ld", cnt_brk, cnt_mmap, cnt_munmap, cnt_page_fault);

		print_ratio_graph(mem_info.vm_rss, mem_info.vm_size, log_fd);
	}

	/*
		관찰중인 프로세스 종료되었을 때..
	*/
	close(nl_socket_fd); // 넷링크 소켓 닫고
	free(nlh);

	cursor_to(18, 1);
	printf("[Parent] Listen Exiting...\n"); // 부모 프로세스 종료
}

/*
 *	log_fd가 가리키는 파일에 로그를 남긴다.
 *	fork 사용하면서, parent에서는 이벤트 감지, child에서는 로그 출력 및 그래프 출력
 */
void run(FILE *log_fd) {
	pid_t pid;

	// 시작
	struct timeval start_tv;
	struct timeval end_tv;
	
	gettimeofday(&start_tv, NULL);
	
	listen_syscall(log_fd); // 실행 코드

	gettimeofday(&end_tv, NULL);


	long long start_ms = (long long)start_tv.tv_sec * 1000 + (long long)start_tv.tv_usec / 1000;
	long long end_ms = (long long)end_tv.tv_sec * 1000 + end_tv.tv_usec / 1000;
	long long duration_ms = end_ms - start_ms;

	cursor_to (21, 1);
    printf("Total Elapsed Time: %lld ms\n", duration_ms); // %lld로 수정
    fflush(stdout); // 즉시 터미널에 출력

	// 종료
}
