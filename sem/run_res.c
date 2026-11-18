#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/sem.h>

static const size_t FORK_COUNT = 100;
static const size_t LAPS_COUNT = 3;

void criticalSection();
void noSem();
void withSem();

int main(void) {
    withSem();
    return 0;
}

void withSem() {
    int sem = semget(IPC_PRIVATE, 2, 0777 | O_CREAT);

    struct sembuf initReady = {
        .sem_num = 0,
        .sem_op  = FORK_COUNT,
        .sem_flg = 0
    };
    semop(sem, &initReady, 1);

    for (size_t i = 0; i < FORK_COUNT; i++) {
        pid_t childPid = fork();
        if (childPid == 0) {
            for (size_t i = 0; i < LAPS_COUNT; i++) {
                struct sembuf z = {
                    .sem_num = 1,
                    .sem_op  = 0
                };
                semop(sem, &z, 1);
                z.sem_num = 0;

                write(1, "ready\n", sizeof("ready\n") - 1);
                struct sembuf pReady = {
                    .sem_num = 0,
                    .sem_op  = -1
                };
                struct sembuf vRan = {
                    .sem_num = 1,
                    .sem_op  = 1
                };
                semop(sem, &pReady, 1);
                semop(sem, &z, 1);
                criticalSection();
                semop(sem, &vRan, 1);
            }

            return;
        }
    }

    for (size_t i = 0; i < LAPS_COUNT; i++) {
        struct sembuf pRan = {
            .sem_num = 1,
            .sem_op  = -FORK_COUNT,
        };
        semop(sem, &pRan, 1);
        semop(sem, &initReady, 1);
    }

    for (size_t i = 0; i < FORK_COUNT; i++)
        wait(NULL);

    semctl(sem, IPC_RMID, 0);
}

void noSem() {
    for (size_t i = 0; i < FORK_COUNT; i++) {
        pid_t childPid = fork();
        if (childPid == 0) {
            write(1, "ready\n", sizeof("ready\n") - 1);
            criticalSection();
            return;
        }
    }

    for (size_t i = 0; i < FORK_COUNT; i++)
        wait(NULL);
}

void criticalSection() {
    write(1, "run!\n", sizeof("run!\n") - 1);
}
