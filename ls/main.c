#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int one_per_line;  // -1
    int long_format;   // -l
    int inode;         // -i
    int numeric_ids;   // -n
    int all;           // -a
    int recursive;     // -R
} ls_options_t;

static int cmp_names(const void *a, const void *b);
static char get_filetype_char(mode_t mode);
static void print_permissions(mode_t mode);
static void print_long_entry(const char *full_path, const char *name,
                             const struct stat *st, const ls_options_t *opts);
static char *join_path(const char *dir, const char *name);
static void list_dir(const char *path, const ls_options_t *opts, 
                     bool print_header);

int main(int argc, char *argv[]) {
    ls_options_t opts = {0};

    static struct option long_options[] = {
        {"one",       no_argument, 0, '1'},
        {"long",      no_argument, 0, 'l'},
        {"inode",     no_argument, 0, 'i'},
        {"numeric",   no_argument, 0, 'n'},
        {"all",       no_argument, 0, 'a'},
        {"recursive", no_argument, 0, 'R'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "1linaR", 
                              long_options, NULL)) != -1) {
        switch (opt) {
            case '1':
                opts.one_per_line = 1;
                break;
            case 'l':
                opts.long_format = 1;
                break;
            case 'i':
                opts.inode = 1;
                break;
            case 'n':
                opts.numeric_ids = 1;
                opts.long_format = 1; 
                break;
            case 'a':
                opts.all = 1;
                break;
            case 'R':
                opts.recursive = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-1lin aR] [file...]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    int num_targets = argc - optind;
    if (num_targets == 0) {
        list_dir(".", &opts, opts.recursive);
    } else {
        for (int i = optind; i < argc; i++) {
            const char *target = argv[i];
            struct stat st;
            if (lstat(target, &st) == -1) {
                fprintf(stderr, "my_ls: cannot access '%s': %s\n", target, strerror(errno));
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                if (i > optind) {
                    putchar('\n');
                }
                list_dir(target, &opts, num_targets > 1 || opts.recursive);
            } else {
                if (opts.inode) {
                    printf("%llu ", (unsigned long long)st.st_ino);
                }
                if (opts.long_format) {
                    print_long_entry(target, target, &st, &opts);
                } else {
                    printf("%s%c", target, opts.one_per_line ? '\n' : ' ');
                }
            }
        }
        if (!opts.long_format && !opts.one_per_line && num_targets > 0) {
            struct stat last_st;
            if (lstat(argv[argc - 1], &last_st) == 0 && !S_ISDIR(last_st.st_mode)) {
                putchar('\n');
            }
        }
    }

    return EXIT_SUCCESS;
}


static int cmp_names(const void *a, const void *b) {
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);
}

static char get_filetype_char(mode_t mode) {
    if (S_ISDIR(mode))
        return 'd';
    if (S_ISLNK(mode))
        return 'l';

    return '-';
}

static void print_permissions(mode_t mode) {
    putchar((mode & S_IRUSR) ? 'r' : '-');
    putchar((mode & S_IWUSR) ? 'w' : '-');
    putchar((mode & S_IXUSR) ? 'x' : '-');

    putchar((mode & S_IRGRP) ? 'r' : '-');
    putchar((mode & S_IWGRP) ? 'w' : '-');
    putchar((mode & S_IXGRP) ? 'x' : '-');

    putchar((mode & S_IROTH) ? 'r' : '-');
    putchar((mode & S_IWOTH) ? 'w' : '-');
    putchar((mode & S_IXOTH) ? 'x' : '-');
}

static void print_long_entry(const char *full_path, const char *name,
                             const struct stat *st, const ls_options_t *opts) {
    char ftype = get_filetype_char(st->st_mode);
    putchar(ftype);
    print_permissions(st->st_mode);

    printf(" %lu", (unsigned long)st->st_nlink);

    if (opts->numeric_ids) {
        printf(" %u %u", (unsigned int)st->st_uid, (unsigned int)st->st_gid);
    } else {
        struct passwd *pw = getpwuid(st->st_uid);
        struct group *gr = getgrgid(st->st_gid);
        if (pw) {
            printf(" %s", pw->pw_name);
        } else {
            printf(" %u", (unsigned int)st->st_uid);
        }
        if (gr) {
            printf(" %s", gr->gr_name);
        } else {
            printf(" %u", (unsigned int)st->st_gid);
        }
    }

    printf(" %lld", (long long)st->st_size);

    char timebuf[64];
    struct tm tm_info;
    if (localtime_r(&st->st_mtime, &tm_info) != NULL) {
        strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", &tm_info);
    } else {
        strcpy(timebuf, "??? ?? ??:??");
    }
    printf(" %s %s", timebuf, name);

    if (ftype == 'l') {
        char target[4096];
        ssize_t len = readlink(full_path, target, sizeof(target) - 1);
        if (len >= 0) {
            target[len] = '\0';
            printf(" -> %s", target);
        }
    }

    putchar('\n');
}

static char *join_path(const char *dir, const char *name) {
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    int needs_slash = (dlen > 0 && dir[dlen - 1] != '/');
    size_t total = dlen + (needs_slash ? 1 : 0) + nlen + 1;

    char *out = malloc(total);
    if (!out) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    memcpy(out, dir, dlen);
    size_t pos = dlen;
    if (needs_slash) {
        out[pos++] = '/';
    }
    memcpy(out + pos, name, nlen);
    out[pos + nlen] = '\0';
    return out;
}

static void list_dir(const char *path, const ls_options_t *opts, 
                     bool print_header) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "my_ls: cannot open directory '%s': %s\n", path, strerror(errno));
        return;
    }

    if (print_header)
        printf("%s:\n", path);

    size_t cap = 64;
    size_t count = 0;
    char **names = malloc(cap * sizeof(char *));
    if (!names) {
        perror("malloc");
        closedir(dir);
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!opts->all && entry->d_name[0] == '.') {
            continue;
        }
        if (count == cap) {
            cap *= 2;
            char **tmp = realloc(names, cap * sizeof(char *));
            if (!tmp) {
                perror("realloc");
                for (size_t i = 0; i < count; i++) free(names[i]);
                free(names);
                closedir(dir);
                exit(EXIT_FAILURE);
            }
            names = tmp;
        }
        names[count] = strdup(entry->d_name);
        if (!names[count]) {
            perror("strdup");
            for (size_t i = 0; i < count; i++) free(names[i]);
            free(names);
            closedir(dir);
            exit(EXIT_FAILURE);
        }
        count++;
    }
    closedir(dir);

    qsort(names, count, sizeof(char *), cmp_names);

    int printed_any_inline = 0;
    for (size_t i = 0; i < count; i++) {
        char *full_path = join_path(path, names[i]);
        struct stat st;
        if (lstat(full_path, &st) == -1) {
            fprintf(stderr, "my_ls: cannot access '%s': %s\n", 
                    full_path, strerror(errno));
            free(full_path);
            continue;
        }

        if (opts->long_format) {
            if (opts->inode)
                printf("%llu ", (unsigned long long)st.st_ino);

            print_long_entry(full_path, names[i], &st, opts);
        } else {
            if (opts->inode)
                printf("%llu ", (unsigned long long)st.st_ino);

            printf("%s", names[i]);
            if (opts->one_per_line) {
                putchar('\n');
            } else {
                putchar(' ');
                printed_any_inline = 1;
            }
        }

        free(full_path);
    }

    if (!opts->long_format && !opts->one_per_line && printed_any_inline)
        putchar('\n');

    if (opts->recursive) {
        for (size_t i = 0; i < count; i++) {
            if (strcmp(names[i], ".") == 0 || strcmp(names[i], "..") == 0) {
                continue;
            }

            char *full_path = join_path(path, names[i]);
            struct stat st;
            if (lstat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                putchar('\n');
                list_dir(full_path, opts, 1);
            }
            free(full_path);
        }
    }

    for (size_t i = 0; i < count; i++)
        free(names[i]);

    free(names);
}
