#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    if (argc < 3)
        return 0;

    int stdout_rep = atoi(argv[1]);
    int stderr_rep = atoi(argv[2]);

    for (int i = 0; i < stdout_rep; i++) {
        fprintf(stdout, "Wawawa\n");
    }
    for (int i = 0; i < stderr_rep; i++) {
        fprintf(stderr, "Wawawa\n");
    }
}
