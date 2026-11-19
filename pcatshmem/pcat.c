#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>

#define BUF_SZ 1024
#define READ_EOF 0
#define stringify(a) stringify_(a)
#define stringify_(a) #a
#define printErr(str) perror(__FILE__ ":" stringify(__LINE__) "[ERROR]:" str)

typedef struct {
	char data[BUF_SZ];
	size_t count;
} Chunk;


void produser(int fd, Chunk* buf, sem_t *full, sem_t *empty);
void consumer(Chunk* buf, sem_t *full, sem_t *empty);


int main(int argc, char* argv[]){
	Chunk* buf = mmap(NULL, 
                      sizeof(Chunk), 
                      PROT_READ | PROT_WRITE, 
                      MAP_SHARED | MAP_ANON, -1, 0);
	
	sem_t* full = sem_open("/full", O_CREAT, 0666, 1);
	sem_t* empty = sem_open("/empty", O_CREAT, 0666, 0);

	int fd = 0;
	if (argc > 1){
		fd = open(argv[1], O_RDONLY);
	}
	if (fd < 0){
		printErr("open failed");
		exit(1);
	}

	pid_t p = fork();
	if (p < 0) {
		perror("fork failed");
		exit(1);
	}

	if (p == 0){
		produser(fd, buf, full, empty);
		if (fd != 0) 
            close(fd);
		return 0;
	}

	consumer(buf, full, empty);

	wait(NULL);

	sem_close(full);
	sem_close(empty);
	sem_unlink("/full");
	sem_unlink("/empty");
	munmap(buf, sizeof(Chunk));

	return 0;		
}

void produser(int fd, Chunk* buf, sem_t *full, sem_t *empty){
	while (true) {
		sem_wait(full);
	
		ssize_t readCount = read(fd, buf->data, 1024);
		if (readCount < 0) {
            printErr("read failed");
			exit(1);
		}
        buf->count = readCount;

		sem_post(empty);
		
		if (buf->count == READ_EOF)
            break;	
	}
}


void consumer(Chunk* buf, sem_t *full, sem_t *empty){
	while (true) {
		sem_wait(empty);
		
		if (buf->count == 0) break;

		ssize_t w = write(1, buf->data, buf->count);
		if (w < 0){
			printErr("write failed");
			exit(1);
		}
	
		sem_post(full);
	}
}
