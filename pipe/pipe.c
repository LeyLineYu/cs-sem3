#include <unistd.h>
#include <stdio.h>

#define READ_EOF 0
#define stringify(a) stringify_(a)
#define stringify_(a) #a
#define printErr(str) perror(__FILE__ ":" stringify(__LINE__) "[ERROR]:" str)

int main(void) {
  
  int fs[2] = {0};
  if (pipe(fs)) {
    printErr("pipe failed: ");
    return 1;
  }

  const char* message = "Hello, World!";
  #define message_len 7
  #define BUF_SZ 256
  _Static_assert(message_len < BUF_SZ, "message_len must fit in BUF_SZ");

  int retpid = fork();
  if (!retpid) {
    close(fs[1]);
    char buf[BUF_SZ] = {0};
    ssize_t readCount = read(fs[0], &buf, sizeof(buf));
    if (readCount == READ_EOF)
      return 0;
    else if (readCount < 0) {  
      printErr("read failed");
      return 1;
    }
    printf("hi im a child and i got: %.*s\n", 
           message_len, buf);
    return 0;
  }

  close(fs[0]);
  write(fs[1], message, message_len);
  printf("hi im a parent and i sent: %.*s\n", 
         message_len, message);

  return 0;
}
