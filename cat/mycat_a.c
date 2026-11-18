#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

#define READ_EOF 0
#define STDIN_FD  0
#define STDOUT_FD 1
#define BUF_SZ 1024
#define stringify(a) stringify_(a)
#define stringify_(a) #a
#define printErr(str) perror(__FILE__ ":" stringify(__LINE__) "[ERROR]:" str)

typedef unsigned int uint;
typedef int FD;

bool echoFile(FD fd);

int main(int argc, char* argv[]) {
  if (argc < 1)
    return 0;

  if (argc == 1) {
    if (!echoFile(STDIN_FD))
      return errno;

    return 0;
  }

  for (uint i = 1; i < (uint)argc; i++) {
     FD fd = open(argv[i], O_RDONLY);
     if (fd < 0) {
       printErr("open failed");
       continue;
     }
     
     echoFile(fd);
     close(fd);
  }

  return 0;
}

bool echoFile(FD fd) {
  if (fd < 0)
    return false;

  char buf[BUF_SZ] = {0};
  bool failed = false;
  while (true) {
    ssize_t readCount = read(fd, &buf, BUF_SZ);
    if (readCount == READ_EOF)
      break;
    else if (readCount < 0) {  
      printErr("read failed");
      failed = true;
      break;
    }

    for (ssize_t remainingCount = readCount; remainingCount > 0; ) {
      ssize_t writtenCount = write(STDOUT_FD, buf, remainingCount);
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
