#define _GNU_SOURCE

#include<stdio.h>
#include<time.h>
#include<unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <sys/types.h>

#define USE_RDTSC


static int PAGE_SIZE;


static inline uint64_t rdtsc(void) {
    unsigned hi, lo;
    // serialize before reading tsc
    asm volatile("lfence" ::: "memory");
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* -------------------------------
   CLOCK_REALTIME in nanoseconds
   ------------------------------- */
static inline uint64_t realtime_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#ifdef USE_RDTSC

#define CLOCK_FUNC() \
    realtime_ns()

#else
#define CLOCK_FUNC() \
    rdtsc()

#endif

#define COUNTER_FUNC() \
    rdtsc()



int main(int argc, char *argv[]) {

    int f_map;
    int page_to_read = 0;
    long file_sz;
    size_t file_pgs;
    struct stat f_map_stat;
    size_t pg_size = sysconf(_SC_PAGESIZE);
    char buff[pg_size];

    printf("page size = %zu\n", pg_size);

    if (argc < 2) {
        printf("Needs 1 arg.\n");
        exit(EBADF);
    }
    
    if (argc == 3) {
	page_to_read = atoi(argv[2]);
    } 

    printf("argv[1] = %s\n", argv[1]);

    //warm up timers
    CLOCK_FUNC();
    COUNTER_FUNC();



    //time the open
    clock_t open_begin = COUNTER_FUNC();
    uint64_t open_begin_ns = CLOCK_FUNC();
    f_map = open(argv[1], O_RDONLY );//| O_DIRECT | O_SYNC);
    clock_t open_end = COUNTER_FUNC();
    uint64_t open_end_ns = CLOCK_FUNC();
    printf("Open time: %lu, %lu, ratio=%f\n",
           (open_end - open_begin),
           (open_end_ns - open_begin_ns),
           (double)(open_end - open_begin) / (double)(open_end_ns - open_begin_ns));


    if (f_map == -1) {
        printf("Failed to open file %s\n", argv[1]);
        exit(errno);
    }

    clock_t stat_begin = COUNTER_FUNC();
    uint64_t stat_begin_ns = CLOCK_FUNC();
    fstat(f_map, &f_map_stat);
    clock_t stat_end = COUNTER_FUNC();
    uint64_t stat_end_ns = CLOCK_FUNC();
    printf("Stat time: %lu, %lu, ratio=%f\n",
           (stat_end - stat_begin),
           (stat_end_ns - stat_begin_ns),
           (double)(stat_end - stat_begin) / (double)(stat_end_ns - stat_begin_ns));

    file_pgs = (f_map_stat.st_size + (pg_size - 1)) / pg_size;
    printf("file includes %lu pages\n", file_pgs);

    // The arguments to mmap are not of a great importance, we will
    // use these pages just to spy on them. What we try to do here
    // with mapping the file through PROT_WRITE and MAP_SHARED pages
    // is to maximize the probability of being reclaimed. At the end
    // of the day, the main reclaim decision is made by LRU
    // priniciple.
    clock_t map_begin = COUNTER_FUNC();
    uint64_t map_begin_ns = CLOCK_FUNC();

#if MMAP_FILE
    void *mapped_to = mmap(NULL, f_map_stat.st_size,
                           PROT_EXEC, MAP_SHARED, f_map, 0);

    clock_t map_end = COUNTER_FUNC();
    uint64_t map_end_ns = CLOCK_FUNC();
        printf("Mapping time: %lu, %lu, ratio=%f\n",
            (map_end - map_begin),
            (map_end_ns - map_begin_ns),
            (double)(map_end - map_begin) / (double)(map_end_ns - map_begin_ns));
#endif
    if (argc == 3)
    {
        clock_t seek_begin = COUNTER_FUNC();
        uint64_t seek_begin_ns = CLOCK_FUNC();
        file_pgs = 1;
	    lseek(f_map, pg_size * page_to_read, SEEK_SET);
        clock_t seek_end = COUNTER_FUNC();
        uint64_t seek_end_ns = CLOCK_FUNC();
        printf("Seeked to pos 0x%llx -> page %d\n", (unsigned long long)(pg_size * page_to_read), page_to_read);
        printf("Seek time: %lu, %lu, ratio=%f\n",
               (seek_end - seek_begin),
               (seek_end_ns - seek_begin_ns),
               (double)(seek_end - seek_begin) / (double)(seek_end_ns - seek_begin_ns));
    }
    
    for (size_t i = 0; i < file_pgs; i++)
    {
        
        //double time_spent = 0.0;
        clock_t begin = COUNTER_FUNC();
        uint64_t begin_ns = CLOCK_FUNC();
    
            read(f_map, buff, pg_size);

        clock_t end = COUNTER_FUNC();
        uint64_t end_ns = CLOCK_FUNC();
        
        // calculate elapsed time by finding difference (end - begin) and
        // dividing the difference by CLOCKS_PER_SEC to convert to seconds
        //time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
        //printf("The elapsed time is %f seconds", time_spent);
         printf("read time: %lu, %lu, ratio=%f\n",
             (end - begin),
             (end_ns - begin_ns),
             (double)(end - begin) / (double)(end_ns - begin_ns));

        usleep(3000);

    }
    clock_t end = COUNTER_FUNC();
    uint64_t end_ns = CLOCK_FUNC();
        printf("Total time: %lu, %lu, ratio=%f\n",
            (end - map_begin),
            (end_ns - map_begin_ns),
            (double)(end - map_begin) / (double)(end_ns - map_begin_ns));

    printf("\n");

    fflush(stdout);

    close(f_map);
    return 0;
}
