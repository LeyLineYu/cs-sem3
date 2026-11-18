#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>

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
    sem_t* sem = sem_open("/mysem", O_CREAT, 0777, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        return;
    }

    for (size_t i = 0; i < FORK_COUNT; i++) {
        pid_t childPid = fork();
        if (childPid == 0) {
            sem_wait(sem);
            criticalSection();
            sem_post(sem);
            return;
        }
    }

    for (size_t i = 0; i < FORK_COUNT; i++)
        wait(NULL);

    sem_close(sem);
    sem_unlink("/mysem");
}

void noSem() {
    for (size_t i = 0; i < FORK_COUNT; i++) {
        pid_t childPid = fork();
        if (childPid == 0) {
            criticalSection();
            return;
        }
    }

    for (size_t i = 0; i < FORK_COUNT; i++)
        wait(NULL);
}

void criticalSection() {
    char buf[256] = {0};
    size_t writtenCount = sprintf(buf, "Hi! My pid is %d", getpid());
    write(1, buf, writtenCount);
    write(1, "Hello", sizeof("Hello") - 1);
    write(1, "\n", sizeof("\n") - 1);
}
