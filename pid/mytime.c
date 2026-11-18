#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdbool.h>

int main(int argc, char* argv[]) {
  if (argc <= 1)
    return 0;

  struct timeval tpStart = {0};
  struct timeval tpEnd = {0};
  gettimeofday(&tpStart, NULL);

  int pid = fork();
  if (!pid) {
    execvp(argv[1], argv + 1);
    return 0;
  }

  wait(NULL);
  gettimeofday(&tpEnd, NULL);
  int64_t secsElapsed = tpEnd.tv_sec - tpStart.tv_sec;
  int64_t usecsElapsed = (tpEnd.tv_usec - tpStart.tv_usec) < 0
                         ? 0 
                         : (tpEnd.tv_usec - tpStart.tv_usec);
  printf("Time elapsed: %3lld,%06lld seconds\n", 
         secsElapsed, usecsElapsed);
  return 0;
}
