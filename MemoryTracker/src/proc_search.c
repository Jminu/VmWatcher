#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include "proc_search.h"
#include "graph.h"
#include "log.h"
#include "ui.h"

#define MAX_LINE_LENGTH 128

FILE *open_proc_stat(pid_t pid) {
	FILE *status_fd = NULL;
	char dir_name[] = "/proc/";
	char proc_num[16];
	char ch[] = "/status";

	sprintf(proc_num, "%d", pid);

	char full_proc_path[64];
	snprintf(full_proc_path, sizeof(full_proc_path), "%s%s%s", dir_name, proc_num, ch);

	status_fd = fopen(full_proc_path, "r");
	if (status_fd == NULL) {
		cusor_to(12, 1);
		printf("관찰중인 프로세스가 죽었음\n");
		return NULL; // exit(1)은 프로그램이 그냥 죽어버림, 따라서 NULL리턴
	}
	log_msg("[FILE] /proc/%d/status Open Success", pid); // (1, 1)에 출력
	
	return status_fd;
}
