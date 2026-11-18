#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc < 3)
        return 1;

    char* src_path = argv[1];
    char* dst_path = argv[2];

    int src = open(src_path, O_RDONLY, 0666);
    if (src < 0) {
        fprintf(stderr, "src open failed\n");
        return 1;
    }
    int dst = open(dst_path, O_CREAT | O_RDWR, 0666);
    if (dst < 0) {
        fprintf(stderr, "dst open failed\n");
        return 1;
    }
    
    struct stat src_stats = {0};
    if (fstat(src, &src_stats) < 0) {
        fprintf(stderr, "fstat failed\n");
        return 1;
    }
    char* src_buf = mmap(NULL, src_stats.st_size, 
                         PROT_READ, 
                         MAP_PRIVATE, src, 0);
    
    ftruncate(dst, src_stats.st_size);
    char* dst_buf = mmap(NULL, src_stats.st_size,
                         PROT_WRITE, 
                         MAP_SHARED, dst, 0);
    
    memcpy(dst_buf, src_buf, src_stats.st_size);
    munmap(dst_buf, src_stats.st_size);
    munmap(src_buf, src_stats.st_size);
    close(dst);
    close(src);
    return 0;
}
