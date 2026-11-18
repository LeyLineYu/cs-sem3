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

bool echoFile(FILE* f);

int main(int argc, char* argv[]) {
  if (argc < 1)
    return 0;

  if (argc == 1) {
    if (!echoFile(stdin))
      return errno;

    return 0;
  }

  for (uint i = 1; i < (uint)argc; i++) {
     FILE* f = fopen(argv[i], "r");
     if (!f) {
       printErr("fopen failed");
       continue;
     }
     
     echoFile(f);
     fclose(f);
  }

  return 0;
}

bool echoFile(FILE* f) {
  if (!f)
    return false;

  char buf[BUF_SZ] = {0};
  bool failed = false;
  while (true) {
    ssize_t readCount = fread(buf, sizeof(char),
                              BUF_SZ, f);
    if (readCount == READ_EOF)
      break;
    else if (readCount < 0) {  
      printErr("read failed");
      failed = true;
      break;
    }

    for (ssize_t remainingCount = readCount; remainingCount > 0; ) {
      ssize_t writtenCount = fwrite(buf, sizeof(char),
                                    remainingCount, stdout);
      if (writtenCount < remainingCount) {  
        printErr("write failed");
        failed = true;
        break;
      }
      remainingCount -= writtenCount;
    }
  }

  return !failed;
}
