#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int shm_key;
int shm_id;

int main(int argc, char *argv[]) {
    int iterations = atoi(argv[1]);
    pid_t pid = getpid();
    pid_t ppid = getppid();

    for (int i = 1; i <= iterations; i++) {
        printf("Worker starting, PID: %d, PPID: %d\n", pid, ppid);
        printf("Called with:\n");
        printf("Intervals: %d seconds, %d nanoseconds\n", iterations, atoi(argv[2]));
    }

    int shm_key = ftok("oss.c", 0);
    if (shm_key <= 0 ) {
        fprintf(stderr,"Worker:... Error in ftok\n");
        exit(1);
    }

    int shm_id = shmget(shm_key, sizeof(int)*2, 0666);
    if (shm_id < 0) {
        fprintf(stderr,"Worker:... Error in shmget\n");
        exit(1);
    }

    int *clock = (int *) shmat(shm_id, 0, 0);
    if (clock <= 0) {
        fprintf(stderr,"Worker:... Error in shmat\n");
        exit(1);
    }
    int *seconds = &(clock[0]);
    int *nanoseconds = &(clock[1]);
    
    
    shmdt(clock);
    return 0;
}