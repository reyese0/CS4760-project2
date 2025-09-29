#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

int shm_key;
int shm_id;

void print_help() {
    printf("How to use: oss [-h] [-s simul] [-t timelimitForChildren] [-i intervalInMsToLaunchChildren]\n");
    printf("  -h      Show help message\n");
    printf("  -n proc Total number of children to launch\n");
    printf("  -s simul Maximum number of children to run simultaneously\n");
    printf("  -t amount of simulated time that should pass before it terminates\n");
    printf("  -i interval in milliseconds between launching new child processes\n");
}

int main(int argc, char *argv[]) {
    int totalChildren = 0;
    int maxSimul = 0;
    int timeLimit = 0;
    int interval = 0;
    char opt;
    const char optstring[] = "hs:t:i:";

    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch (opt) {
            case 'h':
                print_help();
                exit(0);
            case 'n':
                totalChildren = atoi(optarg);
            case 's':
                maxSimul = atoi(optarg);
                break;
            case 't':
                timeLimit = atoi(optarg);
                break;
            case 'i':
                interval = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Invalid option\n");
                print_help();
                exit(1);
        }
    }

    printf("OSS starting, PID: %d PPID: %d\n", getpid(), getppid());
    printf("Called with:\n");
    printf("-n %d\n", totalChildren);
    printf("-s %d\n", maxSimul);
    printf("-t %d\n", timeLimit);
    printf("-i %d\n", interval);

    //checks for required parameters
    if (maxSimul <= 0 || timeLimit <= 0 || interval <= 0) {
        fprintf(stderr, "Invalid parameters: maxSimul, timeLimit, and interval must be positive\n");
        print_help();
        exit(1);
    }

    int shm_key = ftok("oss.c", 0);
    if (shm_key <= 0 ) {
        fprintf(stderr,"Parent:... Error in ftok\n");
        exit(1);
    }

    int shm_id = shmget(shm_key, sizeof(int)*2, IPC_CREAT | 0666);
    if (shm_id < 0) {
        fprintf(stderr,"Parent:... Error in shmget\n");
        exit(1);
    }

    int *clock = (int *) shmat(shm_id, 0, 0);
    if (clock <= 0) {
        fprintf(stderr,"Parent:... Error in shmat\n");
        exit(1);
    }

    int *seconds = &(clock[0]);
    int *nanoseconds = &(clock[1]);
    *seconds = 0;
    *nanoseconds = 0;

    //launch worker processes
    pid_t child_pid = fork();
    if (child_pid == 0) {
        char *args[] = {"./worker"};
        execlp(args[0], args[0], (char *) 0);
    }

    wait(0);

    shmdt(clock);
    clock = 0;
    shmctl(shm_id,IPC_RMID,NULL);

    return 0;
}