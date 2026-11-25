#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUF_SZ 1024
#define RING_SZ 8
#define READ_EOF 0
#define stringify(a) stringify_(a)
#define stringify_(a) #a
#define printErr(str) perror(__FILE__ ":" stringify(__LINE__) "[ERROR]:" str)

typedef struct {
    char data[BUF_SZ];
    size_t count;
} Chunk;

typedef struct {
    Chunk chunks[RING_SZ];
    size_t head;  // Next slot for producer to write
    size_t tail;  // Next slot for consumer to read
} RingBuffer;

static void producer(int fd, RingBuffer *rb, 
                     sem_t *free_slots, sem_t *used_slots);
static void consumer(RingBuffer *rb, 
                     sem_t *free_slots, sem_t *used_slots);
static int write_all(int fd, const char *buf, size_t count);

int main(int argc, char *argv[]) {

    RingBuffer *rb = mmap(NULL,
                          sizeof(RingBuffer),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS,
                          -1,
                          0);
    if (rb == MAP_FAILED) {
        printErr("mmap failed");
        return 1;
    }
    rb->head = 0;
    rb->tail = 0;

    sem_unlink("/cat_free_slots");
    sem_unlink("/cat_used_slots");

    sem_t *free_slots = sem_open("/cat_free_slots", O_CREAT | O_EXCL, 0666, RING_SZ);
    sem_t *used_slots = sem_open("/cat_used_slots", O_CREAT | O_EXCL, 0666, 0);
    if (free_slots == SEM_FAILED || used_slots == SEM_FAILED) {
        printErr("sem_open failed");
        return 1;
    }

    int fd = STDIN_FILENO;
    if (argc > 1) {
        fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
            printErr("open failed");
            return 1;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        printErr("fork failed");
        return 1;
    }

    if (pid == 0) {
        producer(fd, rb, free_slots, used_slots);
        if (fd != STDIN_FILENO) {
            close(fd);
        }
        sem_close(free_slots);
        sem_close(used_slots);
        _exit(0);
    }

    if (fd != STDIN_FILENO) {
        close(fd);
    }

    consumer(rb, free_slots, used_slots);

    if (wait(NULL) < 0) {
        printErr("wait failed");
    }

    sem_close(free_slots);
    sem_close(used_slots);
    sem_unlink("/cat_free_slots");
    sem_unlink("/cat_used_slots");
    munmap(rb, sizeof(RingBuffer));

    return 0;
}

void producer(int fd, RingBuffer *rb, sem_t *free_slots, sem_t *used_slots) {
    while (true) {
        if (sem_wait(free_slots) < 0) {
            printErr("sem_wait free_slots failed");
            _exit(1);
        }

        Chunk *slot = &rb->chunks[rb->head];
        ssize_t read_count;
        do {
            read_count = read(fd, slot->data, BUF_SZ);
        } while (read_count < 0 && errno == EINTR);

        if (read_count < 0) {
            printErr("read failed");
            _exit(1);
        }

        slot->count = (size_t)read_count;
        rb->head = (rb->head + 1) % RING_SZ;

        if (sem_post(used_slots) < 0) {
            printErr("sem_post used_slots failed");
            _exit(1);
        }

        if (read_count == READ_EOF) {
            break;
        }
    }
}

void consumer(RingBuffer *rb, sem_t *free_slots, sem_t *used_slots) {
    while (true) {
        if (sem_wait(used_slots) < 0) {
            printErr("sem_wait used_slots failed");
            _exit(1);
        }

        Chunk *slot = &rb->chunks[rb->tail];
        size_t count = slot->count;
        rb->tail = (rb->tail + 1) % RING_SZ;

        if (count == READ_EOF) {
            if (sem_post(free_slots) < 0) {
                printErr("sem_post free_slots failed");
                _exit(1);
            }
            break;
        }

        if (write_all(STDOUT_FILENO, slot->data, count) < 0) {
            printErr("write failed");
            _exit(1);
        }

        if (sem_post(free_slots) < 0) {
            printErr("sem_post free_slots failed");
            _exit(1);
        }
    }
}

static int write_all(int fd, const char *buf, size_t count) {
    size_t total = 0;
    while (total < count) {
        ssize_t w = write(fd, buf + total, count - total);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += (size_t)w;
    }
    return 0;
}

