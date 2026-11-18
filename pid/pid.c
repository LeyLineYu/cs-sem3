#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
  if (argc <= 1)
    return 0;

  int pid = getpid();
  printf("Hello! My pid is %d!\n", pid);
  int ppid = getppid();
  printf("And my parent's pid is %d!\n", ppid);

  int max = atoi(argv[1]);
  for (int i = 0; i < max; i++) {
    int retpid = fork();
    if (!retpid) {
      ppid = getppid();
      printf("I am a child, my PID is %d and my PPID is %d!\n", getpid(), ppid);
      break;
    }
  }

  return 0;
}
