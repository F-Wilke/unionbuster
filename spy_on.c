/* Timing-based spy on page cache pages using rdtsc.
 *
 * Usage: ./spy_on <path/to/shared/file> [<consider_at_least_pages>]
 *
 * Similar semantics to the original program, but instead of mincore()
 * we decide page residency via access time: cached pages are faster.
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// Minimal portion of pages in spied file needed for the file being
// considered accessed. This is kept for compatibility with the original.
static float at_least_pgs = .01;

// This threshold must be tuned for the platform.
// Rough idea: cached access ~100s of cycles, disk access >> 10^4 cycles.
// Start with something like 2000 and adjust based on measurements.
// static const uint64_t CYCLE_THRESHOLD = 25931470ULL;
static const uint64_t CYCLE_THRESHOLD = 10ULL * 1000ULL * 1000ULL;

static inline uint64_t rdtsc(void)
{
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t measure_page_access_cycles(int f_map, size_t pg_size, char* buff, int page_to_read)
{
    lseek(f_map, (pg_size * page_to_read) * -1, SEEK_END);

    uint64_t start = rdtsc();
    read(f_map, buff, pg_size);
    uint64_t end = rdtsc();

    return end - start;
}

int main(int argc, char *argv[])
{
    int f_map;
    struct stat f_map_stat;
    size_t file_pgs;
    size_t pg_size = sysconf(_SC_PAGESIZE);
    char buff[pg_size];

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

    if (fstat(f_map, &f_map_stat) == -1) {
        perror("fstat");
        close(f_map);
        exit(errno);
    }

    if (f_map_stat.st_size == 0) {
        printf("File is empty.\n");
        close(f_map);
        return 0;
    }

    file_pgs = (f_map_stat.st_size + (pg_size - 1)) / pg_size;

    // Map the file read-only; we just probe by reading.
    char *mapped_to = mmap(NULL, f_map_stat.st_size,
                           PROT_READ, MAP_SHARED, f_map, 0);
    if (mapped_to == MAP_FAILED) {
        perror("mmap");
        close(f_map);
        exit(errno);
    }

    close(f_map);

    // resident_pages[i] == 1 if we think page i is cached, 0 otherwise
    unsigned char resident_pages[file_pgs];

    // Randomized order of page traversal
    size_t *page_indices = malloc(file_pgs * sizeof(size_t));
    if (!page_indices) {
        perror("malloc page_indices");
        munmap(mapped_to, f_map_stat.st_size);
        exit(1);
    }
    for (size_t i = 0; i < file_pgs; i++)
        page_indices[i] = i;

    // Randomized offset inside each page (word-granularity)
    size_t words_per_page = pg_size / sizeof(int);
    if (words_per_page == 0)
        words_per_page = 1;

    if (argc == 3) {
        // Poll until enough pages look "hot"
        do {
            int touched_pgs = 0;

            // Fisher-Yates shuffle of page_indices
            for (size_t i = file_pgs - 1; i > 0; i--) {
                size_t j = (size_t)rand() % (i + 1);
                size_t tmp = page_indices[i];
                page_indices[i] = page_indices[j];
                page_indices[j] = tmp;
            }

            for (size_t idx = 0; idx < file_pgs; idx++) {
                size_t i = page_indices[idx];
                size_t word_index = (size_t)rand() % words_per_page;
                char *page_addr = mapped_to + i * pg_size + word_index * sizeof(int);

                // Clamp to file size
                if ((size_t)(page_addr - mapped_to) >= (size_t)f_map_stat.st_size)
                    continue;

                uint64_t cycles = measure_page_access_cycles(f_map, pg_size, buff, i);
                if (cycles < CYCLE_THRESHOLD) {
                    resident_pages[i] = 1;
                    touched_pgs++;
                } else {
                    resident_pages[i] = 0;
                }
            }

            if (touched_pgs >= at_least_pgs * file_pgs)
                break;

            usleep(5 * 1000);
        } while (1);
    } else {
        // Single measurement in randomized page order and offset
        for (size_t i = file_pgs - 1; i > 0; i--) {
            size_t j = (size_t)rand() % (i + 1);
            size_t tmp = page_indices[i];
            page_indices[i] = page_indices[j];
            page_indices[j] = tmp;
        }

        for (size_t idx = 0; idx < file_pgs; idx++) {
            size_t i = page_indices[idx];
            size_t word_index = (size_t)rand() % words_per_page;
            char *page_addr = mapped_to + i * pg_size + word_index * sizeof(int);

            if ((size_t)(page_addr - mapped_to) >= (size_t)f_map_stat.st_size)
                continue;

            uint64_t cycles = measure_page_access_cycles(f_map, pg_size, buff, i);
            resident_pages[i] = (cycles < CYCLE_THRESHOLD) ? 1 : 0;

            //debug print
            printf("Page %lu: %lu cycles -> %s\n",
                   i, cycles,
                   (cycles < CYCLE_THRESHOLD) ? "RESIDENT" : "NOT RESIDENT");
        }
    }

    for (size_t i = 0; i < file_pgs; i++)
        fputc('0' + (resident_pages[i] & 1), stdout);
    printf("\n");
    fflush(stdout);

    free(page_indices);
    munmap(mapped_to, f_map_stat.st_size);
    return 0;
}
