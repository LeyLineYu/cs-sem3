#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/sem.h>

static const size_t FORK_COUNT = 20;

void criticalSection();
void noSem();
void withSem();

int main(int argc, char* argv[]) {
    if (argc < 1)
        return 0;

    bool doSem = true;
    if (argc > 1) {
        if (strcmp(argv[1], "false") == 0) {
            fprintf(stderr, "No sem\n");
            doSem = false;
        }
    }

    if (doSem) {
        withSem();
    } else {
        noSem();
    }

    return 0;
}

void withSem() {
    int sem = semget(IPC_PRIVATE, 1, 0777 | O_CREAT);

    struct sembuf init = {
        .sem_num = 0,
        .sem_op  = FORK_COUNT,
        .sem_flg = 0
    };
    semop(sem, &init, 1);

    for (size_t i = 0; i < FORK_COUNT; i++) {
        pid_t childPid = fork();
        if (childPid == 0) {
            write(1, "ready\n", sizeof("ready\n") - 1);

            struct sembuf p = {
                .sem_num = 0,
                .sem_op  = -1,
            };
            struct sembuf z = {
                .sem_num = 0,
                .sem_op  = 0,
            };
            semop(sem, &p, 1);
            semop(sem, &z, 1);
            criticalSection();
            return;
        }
    }

    for (size_t i = 0; i < FORK_COUNT; i++)
        wait(NULL);

    semctl(sem, 0, IPC_RMID, 0);
    semctl(sem, 1, IPC_RMID, 0);
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
