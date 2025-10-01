#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <signal.h>

int shm_key;
int shm_id;

typedef struct {
    int seconds;
    int nanoseconds; 
} Clock;

typedef struct {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
} PCB;

PCB processTable[20];

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
    float timeLimit = 0.0;
    float interval = 0.0;
    int childrenLaunched = 0;
    int childrenTerminated = 0;
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

    //initialize the process table
    for (int i = 0; i < 20; i++) {
        processTable[i].occupied = 0;
        processTable[i].pid = 0;
        processTable[i].startSeconds = 0;
        processTable[i].startNano = 0;
    }

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

    int shm_id = shmget(shm_key, sizeof(Clock), IPC_CREAT | 0666);
    if (shm_id < 0) {
        fprintf(stderr,"Parent:... Error in shmget\n");
        exit(1);
    }

    Clock *clock = (Clock *) shmat(shm_id, 0, 0);
    if (clock <= 0) {
        fprintf(stderr,"OSS:... Error in shmat\n");
        exit(1);
    }

    clock->seconds = 0;
    clock->nanoseconds = 0;

    printf("OSS starting, PID: %d PPID: %d\n", getpid(), getppid());
    printf("Called with:\n");
    printf("-n %d\n", totalChildren);
    printf("-s %d\n", maxSimul);
    printf("-t %2f\n", timeLimit);
    printf("-i %3f\n", interval);

    // Convert time limit to seconds and nanoseconds
    int timeLimitSec = (int)timeLimit;
    int timeLimitNano = (int)((timeLimit - timeLimitSec) * 1000000000);
    
    int lastPrintTime = 0;
    int lastLaunchTime = 0;

    while (childrenLaunched < totalChildren || childrenTerminated < totalChildren) {
        //advance clock by 1 million nanoseconds
        clock->nanoseconds += 1000000;

        if (clock->nanoseconds >= 1000000000) {
            clock->seconds++;
            clock->nanoseconds -= 1000000000;
        }

        //check if time limit reached
        if (clock->seconds > timeLimitSec || 
           (clock->seconds == timeLimitSec && clock->nanoseconds > timeLimitNano)) {
            printf("Time limit reached. Terminating.\n");
            break;
        }

        //print process table every half second
        if (clock->seconds >= lastPrintTime + 0.5) {
            printf("OSS PID:%d SysClockS: %d, SysClockNano: %d\n", getpid(), clock->seconds, clock->nanoseconds);
            printf("Process Table:\n");
            printf("Entry\tOccupied\tPID\tStartS\tStartN\n");

            for (int i = 0; i < 20; i++) {
                printf("%d\t%d\t%d\t%d\t%d\n", 
                    i, processTable[i].occupied, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano);
            }
            printf("\n");
            lastPrintTime = clock->seconds;
        }
        
        //check for terminated children
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        while (pid > 0) {
            //find and free process table entry
            for (int i = 0; i < 20; i++) {
                if (processTable[i].occupied && processTable[i].pid == pid) {
                    processTable[i].occupied = 0;
                    processTable[i].pid = 0;
                    processTable[i].startSeconds = 0;
                    processTable[i].startNano = 0;
                    childrenTerminated++;
                    printf("Child process %d terminated.\n", pid);
                    break;
                }
            }
        }

        //launch new children when conditions met
        if (childrenLaunched < totalChildren) {
            int currentChildren = childrenLaunched - childrenTerminated;
            int timeSinceLastLaunchSec = (clock->seconds - lastLaunchTime) * 1000000000 + (clock->nanoseconds - lastLaunchTime);

            if (currentChildren < maxSimul && 
                timeSinceLastLaunchSec >= interval * 1000000000) {
                //find free entry in table entry
                for (int i = 0; i < 20; i++) {
                    if (!processTable[i].occupied) {
                        break;
                    }
                }

                //find empty slot in process table
                int slot = -1;
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (!processTable[i].occupied) {
                        slot = i;
                        break;
                    }
                }

                pid_t child_pid = fork();
                if (child_pid == 0) {
                    char secStr[20];
                    char nanoStr[20];
                    snprintf(secStr, sizeof(secStr), "%d", 1); //1 second
                    snprintf(nanoStr, sizeof(nanoStr), "%d", 0); //0 nanoseconds
                    execl("./worker", "worker", secStr, nanoStr, NULL);
                } else if (child_pid > 0) {
                    //parent process
                    processTable[slot].occupied = 1;
                    processTable[slot].pid = child_pid;
                    processTable[slot].startSeconds = clock->seconds;
                    processTable[slot].startNano = clock->nanoseconds;
                    childrenLaunched++;
                    lastLaunchTime = clock->seconds;
                    printf("Launched child process %d\n", child_pid);
                } else {
                    fprintf(stderr, "Fork failed\n");
                }
                lastLaunchTime = clock->seconds * 1000000000 + clock->nanoseconds;
            }
        }
    }

    while (childrenTerminated < childrenLaunched) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            //find and free process table entry
            for (int i = 0; i < 20; i++) {
                if (processTable[i].occupied && processTable[i].pid == pid) {
                    processTable[i].occupied = 0;
                    processTable[i].pid = 0;
                    processTable[i].startSeconds = 0;
                    processTable[i].startNano = 0;
                    childrenTerminated++;
                    printf("Child process %d terminated.\n", pid);
                    break;
                }
            }
        }

        clock->nanoseconds += 10000;
        if (clock->nanoseconds >= 1000000000) {
            clock->seconds++;
            clock->nanoseconds -= 1000000000;
        }
    }

    // Calculate total run time
    int totalSeconds = 0;
    int totalNano = 0;
    for (int i = 0; i < 20; i++) {
        if (processTable[i].occupied) {
            int runSec = clock->seconds - processTable[i].startSeconds;
            int runNano = clock->nanoseconds - processTable[i].startNano;
            if (runNano < 0) {
                runSec--;
                runNano += 1000000000;
            }
            totalSeconds += runSec;
            totalNano += runNano;
            if (totalNano >= 1000000000) {
                totalSeconds += totalNano / 1000000000;
                totalNano = totalNano % 1000000000;
            }
        }
    }

    printf("OSS PID:%d Terminating\n", getpid());
    printf("%d workers were launched and terminated\n", childrenLaunched);
    printf("Workers ran for a combined time of %d seconds %d nanoseconds\n", totalSeconds, totalNano);

    shmdt(clock);
    clock = 0;
    shmctl(shm_id,IPC_RMID,NULL);

    return 0;
}