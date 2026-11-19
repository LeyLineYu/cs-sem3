#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define READ_EOF  0
#define STDIN_FD  0
#define STDOUT_FD 1
#define BUF_SZ 1024
#define stringify(a) stringify_(a)
#define stringify_(a) #a
#define printErr(str) perror(__FILE__ ":" stringify(__LINE__) "[ERROR]:" str)

typedef unsigned int uint;
typedef int FD;

static size_t current_bit = 0;
static pid_t reader_pid = -1;
static pid_t writer_pid = -1;
static FD current_fd = -1;
static char** filenames = NULL;
static uint filecount = 0;
static uint current_filename = 0;

static char buf[BUF_SZ] = {0};
static size_t occupied = 0;
static size_t current  = 0;

// SIGUSR1 sent by reader means 0
// SIGUSR2 sent by reader means 1
// SIGTERM sent by reader means we're done
void handler_reader(int sig) {
    if (sig != SIGUSR1)
        return;

    if (current_fd == -1) {
        current_fd = open(filenames[current_filename], O_RDONLY);
        current_filename++;
        while (current_fd == -1) {    
            current_fd = open(filenames[current_filename], O_RDONLY);
            current_filename++;
        }
    }

    if (occupied == 0) {
        ssize_t readCount = 0;
        while (true) {
        readCount = read(current_fd, &buf, BUF_SZ);
        if (readCount == READ_EOF) {
            if (current_filename == filecount) {
                kill(writer_pid, SIGTERM);
                free(filenames);
                _exit(0);
            }
            current_fd = open(filenames[current_filename], O_RDONLY);
            current_filename++;
        } else {
            break;
        }
        }

        if (readCount < 0) {  
            printErr("read failed"); 
            kill(writer_pid, SIGTERM);
            free(filenames);
            _exit(0);
        }

        occupied = readCount;
    }

    char mask = 1 << (7 - current_bit++);
    char bit = buf[current] & mask ? SIGUSR2 : SIGUSR1;
    if (current_bit == 8) {
        current++;
        current_bit = 0;
    }
    if (current >= occupied) {
        occupied = 0;
        current = 0;
    }

    kill(writer_pid, bit);
}

void safely_write(FD fd, char* buf, size_t n) {
    for (ssize_t remainingCount = n; remainingCount > 0; ) {
        ssize_t writtenCount = write(fd, buf, remainingCount);
        if (writtenCount < 0) {
            printErr("write failed");
            break;
        }
        remainingCount -= writtenCount;
    }
}

// SIGUSR1 sent by writer means "got it, wrote down the bit"
void handler_writer(int sig) {
    //fprintf(stderr, "writer handler\n");
    if (sig == SIGUSR1 ||
        sig == SIGUSR2) {
        char bit = ((sig == SIGUSR1) ? 0 : 1) << (7 - current_bit++);
        buf[current] |= bit;

        if (current_bit == 8) {
            current++;
            current_bit = 0;
        }

        if (current == BUF_SZ) {
            safely_write(STDOUT_FD, buf, BUF_SZ);
            current = 0;
            memset(buf, 0, BUF_SZ);
        }
        //fprintf(stderr, "sent to reader\n");
        kill(reader_pid, SIGUSR1);
        return;
    }

    if (sig == SIGTERM) {
        safely_write(STDOUT_FD, buf, current);
        current = 0;
        signal(SIGTERM, SIG_DFL);
        kill(getpid(), SIGTERM);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 1)
        return 0;

    filenames = (char**)calloc(argc - 1, sizeof(char*));
    memcpy(filenames, argv + 1, sizeof(char*) * (argc - 1));
    filecount = argc - 1;

    signal(SIGUSR1, &handler_reader);
    reader_pid = getpid();
    pid_t forkPid = fork();
    if (forkPid == 0) {
        // child 
        signal(SIGUSR1, &handler_writer);
        signal(SIGUSR2, &handler_writer);
        signal(SIGTERM, &handler_writer);
        kill(reader_pid, SIGUSR1);
        while (true)
            pause();

        exit(0);
    }

    // parent
    writer_pid = forkPid; 
    while (true)
        pause();

    wait(NULL);
    return 0;
}
