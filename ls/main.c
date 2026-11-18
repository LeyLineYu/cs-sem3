#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_NAME_LENGTH 256 - 1
#define BUF_SZ MAX_NAME_LENGTH * 2

int main(int argc, char* argv[]) {
  if (argc < 1)
    return 0;

  char* dirName     = (argc > 1) ? argv[1] : ".";
  size_t dirNameLen = (argc > 1) ? strlen(dirName) : 1;
  if (dirNameLen > MAX_NAME_LENGTH) {
    fprintf(stderr, "directory name too long\n");
    return 1;
  }

  char fullPath[BUF_SZ] = {0};
  char* assembledPath = strncat(fullPath, dirName, BUF_SZ - 1);

  DIR* d = opendir(argc > 1 ? argv[1] : ".");
  if (!d) {
    perror("opendir");
    return 1;
  }
  bool firstIteration = true;
  while (true) {
    struct dirent* e = readdir(d);
    if (!e)
      break;

    strncat(assembledPath, e->d_name, BUF_SZ - 1 - dirNameLen);

    struct stat st = {0};
    stat(e->d_name, &st);

    if (!firstIteration)
      putc(' ', stdout);

    printf("%s %jd ", 
           e->d_name, 
           (intmax_t)st.st_size);
    firstIteration = false;
  }
  putc('\n', stdout);

  closedir(d);
  return 0;
}
