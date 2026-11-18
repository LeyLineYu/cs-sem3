#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/sem.h>

static const size_t N_FORKS = 10;
static const size_t N_INC   = 100000;

typedef int shm_t;
typedef int sem_t;

int main(void) {
    shm_t shm = shmget(IPC_PRIVATE, getpagesize(), 0666 | IPC_CREAT);
    sem_t sem = semget(IPC_PRIVATE, 1, 0777 | O_CREAT | O_EXCL);
    int* count =(int*)shmat(shm, NULL, 0);

    struct sembuf init = {
        .sem_num = 0,
        .sem_op  = 1,
        .sem_flg = 0
    };
    semop(sem, &init, 1);

    for (size_t i = 0; i < N_FORKS; i++) {
        pid_t forkPid = fork();
        if (forkPid == 0) {
            for (size_t i = 0; i < N_INC; i++) {
                struct sembuf p = {
                    .sem_num = 0,
                    .sem_op  = -1,
                    .sem_flg = 0
                };
                semop(sem, &p, 1);

                (*count)++;

                struct sembuf v = {
                    .sem_num = 0,
                    .sem_op  = 1,
                    .sem_flg = 0
                };
                semop(sem, &v, 1);
            }
            exit(0);
        }
    }

    for (size_t i = 0; i < N_FORKS; i++)
        wait(NULL);

    printf("%d\n", *count);
    shmctl(shm, IPC_RMID, 0);
    semctl(sem, IPC_RMID, 0);

    return EXIT_SUCCESS;
}
