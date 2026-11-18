#include <ctype.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/resource.h>

typedef struct {
  size_t byte_n;
  size_t word_n;
  size_t line_n;
} FileMetadata;

#define READ_EOF 0
#define STDIN_FD  0
#define STDOUT_FD 1
#define BUF_SZ 1024 * 4

#define stringify(a) stringify_(a)
#define stringify_(a) #a
#define printErr(str) perror(__FILE__ ":" stringify(__LINE__) "[ERROR]:" str)

bool slurpAndAnalyzeFileContents(int fd, FileMetadata* m);

int main(int argc, char* argv[]) {
  if (argc <= 1)
    return 0;

  int fs[2] = {0};
  if (pipe(fs)) {
    printErr("pipe failed: ");
    return 1;
  }

  struct timeval tpStart = {0};
  struct timeval tpEnd = {0};
  gettimeofday(&tpStart, NULL);

  struct rusage childUsage = {0};

  int retpid = fork();
  if (!retpid) {
    close(fs[0]);
    dup2(fs[1], 1);
    close(fs[1]);
    execvp(argv[1], argv + 1); 
    return 0;
  }

  close(fs[1]);


  FileMetadata m = {0};
  slurpAndAnalyzeFileContents(fs[0], &m);

  int wait_ret = wait3(NULL, 0, &childUsage);
  if (wait_ret < 0)
    perror("wait3 failed");


  gettimeofday(&tpEnd, NULL);
  int64_t secsElapsed = tpEnd.tv_sec - tpStart.tv_sec;
  int64_t usecsElapsed = tpEnd.tv_usec - tpStart.tv_usec;
  if (usecsElapsed < 0) {
    usecsElapsed += 1000000;
    secsElapsed -= 1;
  }

  for (int i = 1; i < argc; i++) {
    fputs(argv[i], stderr);
    fputc(' ', stderr);
  }

  fputc('\n', stderr);
  fprintf(stderr,
      "Byte: %zu\n"
      "Word: %zu\n"
      "Line: %zu\n"
      "Total: %3ld,%06ld seconds\n"
      "Sys:   %3ld,%06ld seconds\n"
      "User:  %3ld,%06ld seconds\n", 
      m.byte_n,
      m.word_n,
      m.line_n,
      secsElapsed, usecsElapsed,
      childUsage.ru_utime.tv_sec, childUsage.ru_utime.tv_usec,
      childUsage.ru_stime.tv_sec, childUsage.ru_stime.tv_usec);
  return 0;
}

bool slurpAndAnalyzeFileContents(int fd, FileMetadata* m) {
  if (fd < 0 || !m)
    return false;

  *m = (FileMetadata){
    .byte_n = 0,
    .word_n = 0,
    .line_n = 0
  };

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

    m->byte_n += readCount;
    bool is_whitespace = true;
    for (size_t i = 0; i < (size_t)readCount; i++) {
      if (isspace(buf[i])) {
        if (buf[i] == '\n')
          m->line_n++;
        is_whitespace = true;
        continue;
      }

      if (is_whitespace) 
        m->word_n++;
      is_whitespace = false;
    }
  }

  return !failed;
}
