#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdbool.h>

void printTimeval(FILE* sink, struct timeval* t);

int main(int argc, char* argv[]) {
  if (argc <= 1)
    return 0;

  struct timeval tpStart = {0};
  struct timeval tpEnd = {0};
  gettimeofday(&tpStart, NULL);

  struct rusage childUsage = {0};
  int pid = fork();
  if (!pid) {
    execvp(argv[1], argv + 1);
    return 0;
  }

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
          "Total: %3ld,%06ld seconds\n"
          "Sys:   %3ld,%06ld seconds\n"
          "User:  %3ld,%06ld seconds\n", 
          secsElapsed, usecsElapsed,
          childUsage.ru_utime.tv_sec, childUsage.ru_utime.tv_usec,
          childUsage.ru_stime.tv_sec, childUsage.ru_stime.tv_usec);
  return 0;
}

