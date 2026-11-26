#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define WORKER_N 3
#define BUF_SZ   1024

typedef int FD;

// Must be identical to int array[2];
typedef struct {
  FD source; // is read from
  FD sink;   // is written into
} Pipe;

void worker(FD from, FD to);

int main(int argc, char* argv[]) {
    size_t pipeCount = (WORKER_N - 1) * 2;
    Pipe* pipes = (Pipe*)calloc(pipeCount, sizeof(Pipe));
    if (!pipes) {
        fprintf(stderr, "No memory for pipes\n");
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < pipeCount; i++)
       pipe((int*)(pipes + i));

    for (size_t i = 0; i < pipeCount; i++) {
        fprintf(stderr, 
                "#%zu: %d->%d\n", i, (pipes + i)->sink, (pipes + i)->source);
    }

    Pipe* fromChildPipes = pipes;
    Pipe* toChildPipes = pipes + WORKER_N;

    for (size_t i = 0; i < WORKER_N; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            worker((i == 0) 
                    ? STDIN_FILENO 
                    : (toChildPipes + i - 1)->source, 
                   (i == WORKER_N - 1) 
                    ? STDOUT_FILENO 
                    : (fromChildPipes + i - 1)->sink);
            for (size_t i = 0; i < pipeCount; i++) {
                close((pipes + i)->sink);
                close((pipes + i)->source);
            }

            free(pipes);
            exit(EXIT_SUCCESS);
        }
    }

    printf("hi\n");

    for (size_t i = 0; i < WORKER_N; i++)
        wait(NULL);

    for (size_t i = 0; i < pipeCount; i++) {
        close((pipes + i)->sink);
        close((pipes + i)->source);
    }
    free(pipes);
    return EXIT_SUCCESS;
}

void worker(FD from, FD to) {
    printf("Hi i go from %d to %d\n", from, to);
}
