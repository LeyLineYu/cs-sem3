#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    size_t head;      // Next slot for producer to write
    size_t tail;      // Next slot for consumer to read
    sem_t free_slots; 
    sem_t used_slots; 
} RingBuffer;

static void producer(int argc, char *argv[], RingBuffer *rb);
static void consumer(RingBuffer *rb);
static bool stream_fd(int fd, RingBuffer *rb);
static bool enqueue_chunk(RingBuffer *rb, const char *data, size_t count);
static bool write_all(int fd, const char *buf, size_t count);

int main(int argc, char *argv[]) {
    int exit_code = EXIT_FAILURE;
    bool rb_mapped = false;
    bool free_inited = false;
    bool used_inited = false;
    pid_t pid = -1;

    RingBuffer *rb = mmap(NULL,
                          sizeof(RingBuffer),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS,
                          -1,
                          0);
    if (rb == MAP_FAILED) {
        printErr("mmap failed");
        goto cleanup;
    }
    rb->head = 0;
    rb->tail = 0;
    rb_mapped = true;

    if (sem_init(&rb->free_slots, 1, RING_SZ) < 0) {
        printErr("sem_init free_slots failed");
        goto cleanup;
    }
    free_inited = true;

    if (sem_init(&rb->used_slots, 1, 0) < 0) {
        printErr("sem_init used_slots failed");
        goto cleanup;
    }
    used_inited = true;

    pid = fork();
    if (pid < 0) {
        printErr("fork failed");
        goto cleanup;
    }

    if (pid == 0) {
        producer(argc, argv, rb);
        _exit(EXIT_SUCCESS);
    }

    consumer(rb);

    if (waitpid(pid, NULL, 0) < 0) {
        printErr("waitpid failed");
        goto cleanup;
    }
    pid = -1;

    exit_code = EXIT_SUCCESS;
cleanup:
    if (pid > 0)
        waitpid(pid, NULL, 0);
    if (used_inited)
        sem_destroy(&rb->used_slots);
    if (free_inited)
        sem_destroy(&rb->free_slots);
    if (rb_mapped)
        munmap(rb, sizeof(RingBuffer));

    return exit_code;
}

void producer(int argc, char *argv[], RingBuffer *rb) {
    if (argc <= 1) {
        if (stream_fd(STDIN_FILENO, rb))
            _exit(EXIT_FAILURE);
    } else {
        for (int i = 1; i < argc; i++) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                printErr("open failed");
                _exit(EXIT_FAILURE);
            }

            bool failed = stream_fd(fd, rb);
            close(fd);
            if (failed)
                _exit(EXIT_FAILURE);
        }
    }

    // enqueue one EOF chunk when all files are done
    if (enqueue_chunk(rb, NULL, READ_EOF))
        _exit(EXIT_FAILURE);
}

static void consumer(RingBuffer *rb) {
    while (true) {
        if (sem_wait(&rb->used_slots) < 0) {
            printErr("sem_wait used_slots failed");
            _exit(EXIT_FAILURE);
        }

        Chunk *slot = &rb->chunks[rb->tail];
        size_t count = slot->count;
        rb->tail = (rb->tail + 1) % RING_SZ;

        if (count == READ_EOF) {
            if (sem_post(&rb->free_slots) < 0) {
                printErr("sem_post free_slots failed");
                _exit(EXIT_FAILURE);
            }
            break;
        }

        if (write_all(STDOUT_FILENO, slot->data, count)) {
            printErr("write failed");
            _exit(EXIT_FAILURE);
        }

        if (sem_post(&rb->free_slots) < 0) {
            printErr("sem_post free_slots failed");
            _exit(EXIT_FAILURE);
        }
    }
}

static bool write_all(int fd, const char *buf, size_t count) {
    size_t total = 0;
    while (total < count) {
        ssize_t w = write(fd, buf + total, count - total);
        if (w < 0)
            return true;
        total += (size_t)w;
    }
    return false;
}

static bool stream_fd(int fd, RingBuffer *rb) {
    char tmp[BUF_SZ];

    while (true) {
        ssize_t read_count = read(fd, tmp, BUF_SZ);

        if (read_count < 0) {
            printErr("read failed");
            return true;
        }

        if (read_count == READ_EOF)
            return false;

        if (enqueue_chunk(rb, tmp, (size_t)read_count)) {
            return true;
        }
    }
}

static bool enqueue_chunk(RingBuffer *rb, const char *data, size_t count) {
    if (sem_wait(&rb->free_slots) < 0) {
        printErr("sem_wait free_slots failed");
        return true;
    }

    Chunk *slot = &rb->chunks[rb->head];
    if (count > 0 && data != NULL)
        memcpy(slot->data, data, count);

    slot->count = count;
    rb->head = (rb->head + 1) % RING_SZ;

    if (sem_post(&rb->used_slots) < 0) {
        printErr("sem_post used_slots failed");
        return true;
    }

    return false;
}
