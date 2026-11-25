#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define READ_EOF 0
#define BUF_SZ 1024
#define stringify(a) stringify_(a)
#define stringify_(a) #a
#define printErr(str) perror(__FILE__ ":" stringify(__LINE__) "[ERROR]:" str)

static volatile sig_atomic_t GOT_ACK = 0;
static volatile sig_atomic_t RECEIVED_BIT = -1;
static volatile sig_atomic_t GOT_TERM = 0;

static void reader_ack_handler(int sig);
static void writer_bit_handler(int sig);
static int safely_write(int fd, const char* buf, size_t n);
static int install_handler(int sig, void (*fn)(int), 
                           const sigset_t *mask, int flags);
static int wait_for_ack(const sigset_t *wait_mask);
static int send_byte(pid_t peer, unsigned char byte, 
                     const sigset_t *wait_mask);
static int run_reader(int argc, char *argv[], pid_t writer_pid);
static int run_writer(pid_t reader_pid);

int main(int argc, char *argv[]) {
    // Block SIGUSR1 across fork so parent can record child PID before handling signals
    sigset_t init_mask, old_mask;
    sigemptyset(&init_mask);
    sigaddset(&init_mask, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &init_mask, &old_mask) < 0) {
        printErr("sigprocmask failed");
        return EXIT_FAILURE;
    }

    pid_t reader_pid = getpid();
    pid_t pid = fork();
    if (pid < 0) {
        printErr("fork failed");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        // Restore default mask in child before entering run_writer
        if (sigprocmask(SIG_SETMASK, &old_mask, NULL) < 0) {
            printErr("sigprocmask child failed");
            _exit(EXIT_FAILURE);
        }
        _exit(run_writer(reader_pid));
    }

    // Restore original mask in parent; 
    // SIGUSR1 is blocked inside run_reader until sigsuspend
    if (sigprocmask(SIG_SETMASK, &old_mask, NULL) < 0) {
        printErr("sigprocmask parent failed");
        waitpid(pid, NULL, 0);
        return EXIT_FAILURE;
    }

    int reader_status = run_reader(argc, argv, pid);
    if (wait(NULL) < 0) {
        printErr("wait failed");
        return EXIT_FAILURE;
    }

    if (reader_status != EXIT_SUCCESS)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static int run_reader(int argc, char *argv[], pid_t writer_pid) {
    sigset_t block_mask, wait_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &block_mask, NULL) < 0) {
        printErr("sigprocmask failed");
        return EXIT_FAILURE;
    }
    if (install_handler(SIGUSR1, reader_ack_handler, &block_mask, 0) < 0) {
        return EXIT_FAILURE;
    }

    sigemptyset(&wait_mask); // Unblocks SIGUSR1 while suspended

    // wait for writer's initial ready signal
    if (wait_for_ack(&wait_mask) < 0) {
        return EXIT_FAILURE;
    }

    unsigned char buf[BUF_SZ];

    if (argc <= 1) {
        while (true) {
            ssize_t n;
            do {
                n = read(STDIN_FILENO, buf, sizeof(buf));
            } while (n < 0 && errno == EINTR);

            if (n < 0) {
                printErr("read failed");
                kill(writer_pid, SIGTERM);
                return EXIT_FAILURE;
            }
            if (n == READ_EOF) {
                break;
            }
            for (ssize_t i = 0; i < n; i++) {
                if (send_byte(writer_pid, buf[i], &wait_mask) < 0) {
                    return EXIT_FAILURE;
                }
            }
        }
    } else {
        for (int i = 1; i < argc; i++) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                printErr("open failed");
                kill(writer_pid, SIGTERM);
                return EXIT_FAILURE;
            }

            while (true) {
                ssize_t n;
                do {
                    n = read(fd, buf, sizeof(buf));
                } while (n < 0 && errno == EINTR);

                if (n < 0) {
                    printErr("read failed");
                    close(fd);
                    kill(writer_pid, SIGTERM);
                    return EXIT_FAILURE;
                }
                if (n == READ_EOF) {
                    break;
                }
                for (ssize_t j = 0; j < n; j++) {
                    if (send_byte(writer_pid, buf[j], &wait_mask) < 0) {
                        close(fd);
                        return EXIT_FAILURE;
                    }
                }
            }

            close(fd);
        }
    }

    if (kill(writer_pid, SIGTERM) < 0) {
        printErr("kill SIGTERM failed");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int run_writer(pid_t reader_pid) {
    sigset_t block_mask, wait_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGUSR1);
    sigaddset(&block_mask, SIGUSR2);
    sigaddset(&block_mask, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &block_mask, NULL) < 0) {
        printErr("sigprocmask failed");
        return EXIT_FAILURE;
    }
    if (install_handler(SIGUSR1, writer_bit_handler, &block_mask, 0) < 0 ||
        install_handler(SIGUSR2, writer_bit_handler, &block_mask, 0) < 0 ||
        install_handler(SIGTERM, writer_bit_handler, &block_mask, 0) < 0) {
        return EXIT_FAILURE;
    }

    sigemptyset(&wait_mask); // Unblocks SIGUSR1, SIGUSR2, SIGTERM while suspended

    // hey reader im ready for the first bit
    if (kill(reader_pid, SIGUSR1) < 0) {
        printErr("kill ready failed");
        return EXIT_FAILURE;
    }

    unsigned char out_buf[BUF_SZ];
    size_t byte_pos = 0;
    unsigned char cur_byte = 0;
    int bit_pos = 7;

    while (true) {
        while (RECEIVED_BIT == -1 && !GOT_TERM) {
            if (sigsuspend(&wait_mask) < 0 && errno != EINTR) {
                printErr("sigsuspend failed");
                return EXIT_FAILURE;
            }
        }

        if (GOT_TERM) {
            if (byte_pos > 0 && 
                safely_write(STDOUT_FILENO, (const char *)out_buf, byte_pos) < 0) {
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }

        int bit = RECEIVED_BIT;
        RECEIVED_BIT = -1;

        if (bit == 1) {
            cur_byte |= (unsigned char)(1U << bit_pos);
        }
        bit_pos--;

        if (bit_pos < 0) {
            out_buf[byte_pos++] = cur_byte;
            cur_byte = 0;
            bit_pos = 7;

            if (byte_pos == BUF_SZ) {
                if (safely_write(STDOUT_FILENO, (const char *)out_buf, byte_pos) < 0) {
                    return EXIT_FAILURE;
                }
                byte_pos = 0;
            }
        }

        if (kill(reader_pid, SIGUSR1) < 0) {
            printErr("kill ack failed");
            return EXIT_FAILURE;
        }
    }
}

static void reader_ack_handler(int sig) {
    (void)sig;
    GOT_ACK = 1;
}

static void writer_bit_handler(int sig) {
    if (sig == SIGUSR1) {
        RECEIVED_BIT = 0;
    } else if (sig == SIGUSR2) {
        RECEIVED_BIT = 1;
    } else if (sig == SIGTERM) {
        GOT_TERM = 1;
    }
}

static int install_handler(int sig, void (*fn)(int), 
                           const sigset_t *mask, int flags) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = fn;
    sa.sa_flags = flags;
    if (mask != NULL) {
        sa.sa_mask = *mask;
    } else {
        sigemptyset(&sa.sa_mask);
    }
    if (sigaction(sig, &sa, NULL) < 0) {
        printErr("sigaction failed");
        return -1;
    }
    return 0;
}

static int safely_write(int fd, const char* buf, size_t n) {
    for (ssize_t remainingCount = n; remainingCount > 0; ) {
        ssize_t writtenCount = write(fd, buf, remainingCount);
        if (writtenCount < 0) {
            printErr("write failed");
            return -1;
        }
        remainingCount -= writtenCount;
    }
    return 0;
}

static int wait_for_ack(const sigset_t *wait_mask) {
    while (!GOT_ACK) {
        if (sigsuspend(wait_mask) < 0 && errno != EINTR) {
            printErr("sigsuspend failed");
            return -1;
        }
    }
    GOT_ACK = 0;
    return 0;
}

static int send_byte(pid_t peer, unsigned char byte, 
                     const sigset_t *wait_mask) {
    for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
        int bit = (byte >> bit_idx) & 1;
        int sig = (bit == 1) ? SIGUSR2 : SIGUSR1;

        if (kill(peer, sig) < 0) {
            printErr("kill failed");
            return -1;
        }
        if (wait_for_ack(wait_mask) < 0) {
            return -1;
        }
    }
    return 0;
}
