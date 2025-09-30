#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int shm_key;
int shm_id;

typedef struct {
    int seconds;
    int nanoseconds; 
} Clock;

int main(int argc, char *argv[]) {
    int inputSec = atoi(argv[1]);
    int inputNano = atoi(argv[2]);
    pid_t pid = getpid();
    pid_t ppid = getppid();

    printf("Worker starting, PID: %d, PPID: %d\n", pid, ppid);
    printf("Called with:\n");
    printf("Intervals: %d seconds, %d nanoseconds\n", atoi(argv[1]), atoi(argv[2]));

    int shm_key = ftok("oss.c", 0);
    if (shm_key <= 0 ) {
        fprintf(stderr,"Worker:... Error in ftok\n");
        exit(1);
    }

    int shm_id = shmget(shm_key, sizeof(Clock), 0666);
    if (shm_id < 0) {
        fprintf(stderr,"Worker:... Error in shmget\n");
        exit(1);
    }

    Clock *clock = (Clock *) shmat(shm_id, 0, 0);
    if (clock <= 0) {
        fprintf(stderr,"Worker:... Error in shmat\n");
        exit(1);
    }

    //get start time
    int startSec = clock->seconds;
    int startNano = clock->nanoseconds;

    //calculate termination time
    int endSec = startSec + inputSec;
    int endNano = startNano + inputNano;

    printf("WORKER PID:%d PPID:%d\n", pid, ppid);
    printf("SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n", startSec, startNano, endSec, endNano);
    printf("--Just Starting\n");

    //track how many seconds have been reported
    int lastSec = startSec;

    while (1) {
        //check current time
        int currSec = clock->seconds;
        int currNano = clock->nanoseconds;

        //print every second
        if (currSec > lastSec) {
            printf("WORKER PID:%d PPID:%d\n", pid, ppid);
            printf("SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n", currSec, currNano, endSec, endNano);
            printf("--%d seconds have passed since starting\n", currSec - startSec);
            lastSec = currSec;
        }

        //check for termination condition
        if ( (currSec > endSec) || (currSec == endSec && currNano >= endNano) ) {
            printf("WORKER PID:%d PPID:%d\n", pid, ppid);
            printf("SysClockS: %d SysclockNano: %d\n", currSec, currNano)
            printf("--Terminating\n");
            break;
        }
    }

    shmdt(clock);
    return 0;
}