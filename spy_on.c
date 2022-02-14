/* This program spies on page cache pages and prints the access
 * signature to stdout.
 *
 * Author: Novak Boškov <boskov@bu.edu>
 *
 * Created on Dec, 2019.
 *
 * Usage: ./spy_on <path/to/shared/file> [<consider_at_least_pages>]
 */
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>

// Minimal portion of pages in spied file needed for the file being
// considered accessed. This is needed in MySQL, because `mysqld` runs
// as a daemon.
static float at_least_pgs = .01;

int main(int argc, char *argv[]) {
    int mincore_ret;
    int f_map;
    struct stat f_map_stat;
    size_t file_pgs;
    int touched_pgs;
    size_t pg_size = sysconf(_SC_PAGESIZE);

    if (argc < 2) {
        printf("Needs 1 arg at least.\n");
        exit(EBADF);
    } else if (argc == 3) {
        char *end;
        float f = strtof(argv[2], &end);
        if (f)
            at_least_pgs = f;
    }

    f_map = open(argv[1], O_RDONLY);
    if (f_map == -1) {
        printf("Failed to open file %s\n", argv[1]);
        exit(errno);
    }

    fstat(f_map, &f_map_stat);
    file_pgs = (f_map_stat.st_size + (pg_size - 1)) / pg_size;
    // The arguments to mmap are not of a great importance, we will
    // use these pages just to spy on them. What we try to do here
    // with mapping the file through PROT_WRITE and MAP_SHARED pages
    // is to maximize the probability of being reclaimed. At the end
    // of the day, the main reclaim decision is made by LRU
    // priniciple.
    void *mapped_to = mmap(NULL, f_map_stat.st_size,
                           PROT_EXEC, MAP_SHARED, f_map, 0);

    close(f_map);

    unsigned char resident_pages[file_pgs];

    if (argc == 3) {
        do {
            touched_pgs = 0;

            mincore_ret = mincore(mapped_to, f_map_stat.st_size, resident_pages);
            if (mincore_ret == -1) {
                printf("Mincore failed!\n");
                exit(-1);
            }

            for (size_t i = 0; i < file_pgs; i++)
                if (resident_pages[i] & 1)
                    touched_pgs++;

            usleep(5 * 1000);
        } while (touched_pgs < at_least_pgs * file_pgs);
    } else {
        mincore_ret = mincore(mapped_to, f_map_stat.st_size, resident_pages);
        if (mincore_ret == -1) {
            printf("Mincore failed!\n");
            exit(-1);
        }
    }

    for (size_t i = 0; i < file_pgs; i++)
        fputc('0' + (resident_pages[i] & 1), stdout);
    printf("\n");

    fflush(stdout);
    return 0;
}
