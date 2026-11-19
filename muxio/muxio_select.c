#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/resource.h>

typedef struct {
  size_t byte_n;
  size_t word_n;
  size_t line_n;
} FileMetadata;

#define READ_EOF  0
#define STDIN_FD  0
#define STDOUT_FD 1
#define BUF_SZ 1024 * 4

#define stringify(a) stringify_(a)
#define stringify_(a) #a
#define printErr(str) perror(__FILE__ ":" stringify(__LINE__) "[ERROR]:" str)

void makeNonBlocking(int fd);
bool slurpAndAnalyzeFileContents(int fd, FileMetadata* m, bool *again);
void printStats(FileMetadata* out_m, FileMetadata* err_m, 
                struct rusage* r, int64_t secsElapsed, int64_t usecsElapsed);

int main(int argc, char* argv[]) {
  if (argc <= 1)
    return 0;

  int stdout_pipe[2] = {0};
  if (pipe(stdout_pipe)) {
    printErr("pipe failed: ");
    return 1;
  }
  int stderr_pipe[2] = {0};
  if (pipe(stderr_pipe)) {
    printErr("pipe failed: ");
    return 1;
  }

  struct timeval tpStart = {0};
  struct timeval tpEnd = {0};
  gettimeofday(&tpStart, NULL);

  struct rusage childUsage = {0};

  int retpid = fork();
  if (!retpid) {
    close(stdout_pipe[0]);
    dup2(stdout_pipe[1], 1);
    close(stdout_pipe[1]);

    close(stderr_pipe[0]);
    dup2(stderr_pipe[1], 2);
    close(stderr_pipe[1]);

    execvp(argv[1], argv + 1); 
    return 0;
  }

  close(stdout_pipe[1]);
  close(stderr_pipe[1]);


  FileMetadata m1 = {0};
  makeNonBlocking(stdout_pipe[0]);
  makeNonBlocking(stderr_pipe[0]);
  FileMetadata m2 = {0};

  fd_set readfds;
  int nfds = stdout_pipe[0] > stderr_pipe[0] ? stdout_pipe[0] : stderr_pipe[0];
  nfds++;

  bool any_failed = false;
  do {
      FD_ZERO(&readfds);
      FD_SET(stdout_pipe[0], &readfds);
      FD_SET(stderr_pipe[0], &readfds);
      int ready = select(nfds, &readfds, NULL, NULL, NULL);
      if (ready == -1) {
          printErr("select failed");
          exit(1);
      }

      any_failed = false;
      if (FD_ISSET(stdout_pipe[0], &readfds))
          slurpAndAnalyzeFileContents(stdout_pipe[0], &m1, &any_failed);
      if (FD_ISSET(stderr_pipe[0], &readfds))
          slurpAndAnalyzeFileContents(stderr_pipe[0], &m2, &any_failed);
  } while (any_failed);

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
  printStats(&m1, &m2, &childUsage, secsElapsed, usecsElapsed);
  return 0;
}

void makeNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
      perror("fcntl");
      return;
  }
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) == -1) {
      perror("fcntl");
      return;
  }
}

void printStats(FileMetadata* out_m, FileMetadata* err_m, 
                struct rusage* r, int64_t secsElapsed, int64_t usecsElapsed) {
  fprintf(stderr,
      "STDOUT:\n"
      "\tByte: %zu\n"
      "\tWord: %zu\n"
      "\tLine: %zu\n"
      "STDERR:\n"
      "\tByte: %zu\n"
      "\tWord: %zu\n"
      "\tLine: %zu\n"
      "Total: %3ld,%06ld seconds\n"
      "Sys:   %3ld,%06ld seconds\n"
      "User:  %3ld,%06ld seconds\n", 
      out_m->byte_n,
      out_m->word_n,
      out_m->line_n,
      err_m->byte_n,
      err_m->word_n,
      err_m->line_n,
      secsElapsed, usecsElapsed,
      r->ru_utime.tv_sec, r->ru_utime.tv_usec,
      r->ru_stime.tv_sec, r->ru_stime.tv_usec);
}

bool slurpAndAnalyzeFileContents(int fd, FileMetadata* m, bool *again) {
  if (fd < 0 || !m || !again)
    return false;

  char buf[BUF_SZ] = {0};
  bool failed = false;
  while (true) {
      ssize_t readCount = 0;
      readCount = read(fd, &buf, BUF_SZ);
      if (readCount == READ_EOF)
          break;
      else if (readCount < 0) {  
          if (errno == EAGAIN) {
              *again = true;
              return false;
          }
          printErr("read failed");
          failed = true;
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
