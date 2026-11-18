#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdbool.h>

int main(int argc, char* argv[]) {
  if (argc <= 1)
    return 0;

  int max = argc - 1;
  printf("sorted:");
  for (int i = 0; i < max; i++) {
    int retpid = fork();
    int sleepTime = atoi(argv[i + 1]);
    if (sleepTime < 0) {
      //panic!!!
    }
    if (!retpid) {
      usleep(sleepTime);   
      _exit(sleepTime);
    }
  }

  for (int i = 0; i < max; i++) {
    int ret = 0;
    wait(&ret);
    printf(" %d", ret >> 8);
  }
  printf("\n");

  return 0;
}
