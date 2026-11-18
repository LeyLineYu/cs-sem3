#include <string.h>
#include <unistd.h>

#define STDOUT_FD 1
#define writecstr(fd, str) write(fd, str, sizeof(str) - 1)

int main(int argc, char* argv[]) {
	writecstr(STDOUT_FD, "Hi!\n");
	bool noNewline = false;
	size_t i = 1;
	if (argc <= 1) {
	        writecstr(STDOUT_FD, "\n");
		return 0;
	}
        while (strcmp(argv[i], "-n") == 0) {
		noNewline = true;
		i++;
	}

	for (; i < (size_t)argc; i++) {
		char* cur = argv[i];
	        write(STDOUT_FD, cur, strlen(cur));
		if (i + 1 < (size_t)argc)
	 	       writecstr(STDOUT_FD, " ");
	}

	if (!noNewline)
	        writecstr(STDOUT_FD, "\n");
	return 0;
}
