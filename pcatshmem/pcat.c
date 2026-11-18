#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>

#define READ_EOF 0
#define STDIN_FD  0
#define STDOUT_FD 1
#define stringify(a) stringify_(a)
#define stringify_(a) #a
#define printErr(str) perror(__FILE__ ":" stringify(__LINE__) "[ERROR]:" str)

typedef unsigned int uint;
typedef int FD;

bool echoFile(FD from, FD to);

#define N_BLOCKS 40
#define BLOCK_SZ 1024

typedef struct {
    size_t read_index; __attribute__((aligned(64)))
    size_t write_index; 
    char buf[BLOCK_SZ][N_BLOCKS];
} SharedBuf;

int main(int argc, char* argv[]) {
  if (argc < 1)
    return 0;

  int buf_fd = shm_open("/shbuf", O_RDWR | O_CREAT | O_EXCL, 0666);
  ftruncate(buf_fd, sizeof(SharedBuf));

  pid_t forkPid = fork();
  if (forkPid == 0) {
    // child 
    if (argc == 1) {
      if (!echoFile(STDIN_FD, fromChild.sink))
        return errno;

      return 0;
    }

    for (uint i = 1; i < (uint)argc; i++) {
      FD fd = open(argv[i], O_RDONLY);
      if (fd < 0) {
        printErr("open failed");
        continue;
      }

      echoFile(fd, fromChild.sink);
      close(fd);
    }
    close(fromChild.sink);

    exit(0);
  }

  // parent
  
  close(fromChild.sink);
  echoFile(fromChild.source, STDOUT_FD);
  close(fromChild.source);

  wait(NULL);
  close(buf_fd);

  return 0;
}

bool echoFile(FD from, FD to) {
  if (from < 0 || to < 0)
    return false;

  char buf[BUF_SZ] = {0};
  bool failed = false;
  while (true) {
    ssize_t readCount = read(from, &buf, BUF_SZ);
    if (readCount == READ_EOF)
      break;
    else if (readCount < 0) {  
      printErr("read failed");
      failed = true;
      break;
    }

    for (ssize_t remainingCount = readCount; remainingCount > 0; ) {
      ssize_t writtenCount = write(to, buf, remainingCount);
      if (writtenCount < 0) {  
        printErr("write failed");
        failed = true;
        break;
      }
      remainingCount -= writtenCount;
    }
  }

  return !failed;
}
