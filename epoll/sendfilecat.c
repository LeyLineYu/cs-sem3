#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/sendfile.h>

#define READ_EOF  0
#define STDIN_FD  0
#define STDOUT_FD 1
#define BUF_SZ 1024
#define stringify(a) stringify_(a)
#define stringify_(a) #a
#define printErr(str) perror(__FILE__ ":" stringify(__LINE__) "[ERROR]:" str)

typedef unsigned int uint;
typedef int FD;

bool sendfileFully(FD to, FD from, bool* is_impossible);

int main(int argc, char* argv[]) {
    if (argc < 1)
        return EXIT_SUCCESS;

    if (argc == 1) {
        bool is_impossible = false;
        if (sendfileFully(STDOUT_FD, STDIN_FD, &is_impossible))
            return EXIT_FAILURE;

        if (is_impossible) {
            fprintf(stderr, "Impossible to sendfile() current stdin\n");
            return EXIT_SUCCESS;
        }
    }

    for (uint i = 1; i < (uint)argc; i++) {
        FD fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            printErr("open failed");
            continue;
        }

        if (sendfileFully(STDOUT_FD, fd, NULL))
            return EXIT_FAILURE;

        close(fd);
    }

    return EXIT_SUCCESS;
}

bool sendfileFully(FD to, FD from, bool* is_impossible) {
    if (from < 0 || to < 0)
        return true;

    bool failed = false;
    off_t offset = 0;
    while (true) {
        for (ssize_t remainingCount = BUF_SZ; remainingCount > 0; ) {
            ssize_t writtenCount = sendfile(to, from, &offset, remainingCount);
            if (writtenCount < 0) {  
                if (errno == ESPIPE) {
                    if (is_impossible)
                        *is_impossible = true;
                    return false;
                }
                printErr("sendfile failed");
                failed = true;
                break;
            } if (writtenCount == 0) {
                return false;
            }
            remainingCount -= writtenCount;
        }
    }

    return failed;
}
