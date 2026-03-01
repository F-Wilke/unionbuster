/*
 * Sender for strided page cache covert channel
 * Loads every 32nd page of a file into the page cache to encode information
 * 
 * Usage: ./sender_stride <file> <bit_pattern>
 *   bit_pattern: string of 0s and 1s indicating which pages to prime
 *                e.g., "10110" means prime pages 0, 64, 96 (indices 0, 2, 3)
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define PAGE_STRIDE 32
#define USE_RDTSC

static inline uint64_t rdtsc(void) {
    unsigned hi, lo;
    asm volatile("lfence" ::: "memory");
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t realtime_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#ifdef USE_RDTSC
#define CLOCK_FUNC() realtime_ns()
#else
#define CLOCK_FUNC() rdtsc()
#endif

#define COUNTER_FUNC() rdtsc()

int main(int argc, char *argv[]) {
    int f_map;
    struct stat f_map_stat;
    size_t pg_size = sysconf(_SC_PAGESIZE);
    char buff[pg_size];
    bool verbose = false;
    int arg_idx = 1;
    
    // Check for -v flag
    if (argc >= 2 && strcmp(argv[1], "-v") == 0) {
        verbose = true;
        arg_idx = 2;
    }

    if (argc < arg_idx + 2) {
        fprintf(stderr, "Usage: %s [-v] <file> <bit_pattern>\n", argv[0]);
        fprintf(stderr, "  bit_pattern: string of 0s and 1s (e.g., \"10110\")\n");
        fprintf(stderr, "  Each bit controls PAGE_STRIDE*index page (STRIDE=%d)\n", PAGE_STRIDE);
        exit(1);
    }
    
    const char *filename = argv[arg_idx];
    const char *bit_pattern = argv[arg_idx + 1];
    size_t num_bits = strlen(bit_pattern);
    
    // Validate bit pattern
    for (size_t i = 0; i < num_bits; i++) {
        if (bit_pattern[i] != '0' && bit_pattern[i] != '1') {
            fprintf(stderr, "Error: bit_pattern must contain only 0s and 1s\n");
            exit(1);
        }
    }
    
    // Warm up timers
    CLOCK_FUNC();
    COUNTER_FUNC();
    
    uint64_t total_begin_ns = CLOCK_FUNC();
    uint64_t total_begin_cycles = COUNTER_FUNC();
    
    // Open file
    uint64_t open_begin_cycles = COUNTER_FUNC();
    uint64_t open_begin_ns = CLOCK_FUNC();
    f_map = open(filename, O_RDONLY);
    uint64_t open_end_cycles = COUNTER_FUNC();
    uint64_t open_end_ns = CLOCK_FUNC();
    
    if (f_map == -1) {
        fprintf(stderr, "Failed to open file %s: %s\n", filename, strerror(errno));
        exit(errno);
    }
    
    // Get file size
    if (fstat(f_map, &f_map_stat) == -1) {
        perror("fstat");
        close(f_map);
        exit(errno);
    }
    
    size_t file_pgs = (f_map_stat.st_size + (pg_size - 1)) / pg_size;
    size_t max_stride_pages = file_pgs / PAGE_STRIDE;
    
    if (verbose) {
        fprintf(stderr, "File: %s\n", filename);
        fprintf(stderr, "File size: %ld bytes\n", f_map_stat.st_size);
        fprintf(stderr, "Page size: %zu bytes\n", pg_size);
        fprintf(stderr, "Total pages: %zu\n", file_pgs);
        fprintf(stderr, "Max strided pages: %zu\n", max_stride_pages);
        fprintf(stderr, "Bit pattern: %s (%zu bits)\n", bit_pattern, num_bits);
    }
    
    if (num_bits > max_stride_pages) {
        fprintf(stderr, "Warning: bit_pattern (%zu bits) exceeds available strided pages (%zu)\n",
                num_bits, max_stride_pages);
        fprintf(stderr, "Only first %zu bits will be used\n", max_stride_pages);
        num_bits = max_stride_pages;
    }
    
    size_t pages_primed = 0;
    uint64_t total_read_cycles = 0;
    uint64_t total_read_ns = 0;
    
    // Prime pages according to bit pattern
    for (size_t bit_idx = 0; bit_idx < num_bits; bit_idx++) {
        if (bit_pattern[bit_idx] == '1') {
            size_t page_num = bit_idx * PAGE_STRIDE;
            off_t offset = (off_t)page_num * (off_t)pg_size;
            
            // Seek to page
            if (lseek(f_map, offset, SEEK_SET) == -1) {
                fprintf(stderr, "Warning: Cannot seek to page %zu: %s\n", 
                        page_num, strerror(errno));
                continue;
            }
            
            // Read page to prime cache
            uint64_t read_begin_cycles = COUNTER_FUNC();
            uint64_t read_begin_ns = CLOCK_FUNC();
            
            ssize_t bytes_read = read(f_map, buff, pg_size);
            
            uint64_t read_end_cycles = COUNTER_FUNC();
            uint64_t read_end_ns = CLOCK_FUNC();
            
            if (bytes_read < 0) {
                fprintf(stderr, "Warning: Read error at page %zu: %s\n", 
                        page_num, strerror(errno));
                continue;
            }
            
            uint64_t read_cycles = read_end_cycles - read_begin_cycles;
            uint64_t read_ns = read_end_ns - read_begin_ns;
            
            total_read_cycles += read_cycles;
            total_read_ns += read_ns;
            pages_primed++;
            
            if (verbose) {
                fprintf(stderr, "Primed bit %zu -> page %zu (offset 0x%lx): %lu cycles, %lu ns\n",
                        bit_idx, page_num, offset, read_cycles, read_ns);
            }
            
            // Small delay to ensure page is settled in cache
            usleep(1000);
        }
    }
    
    uint64_t total_end_ns = CLOCK_FUNC();
    uint64_t total_end_cycles = COUNTER_FUNC();
    
    // Print CSV header if verbose
    if (verbose) {
        printf("page_size,filename,bit_pattern,num_bits,pages_primed,stride,open_cycles,open_ns,");
        printf("avg_read_cycles,avg_read_ns,total_cycles,total_ns\n");
    }
    
    // Print CSV data
    uint64_t avg_read_cycles = pages_primed > 0 ? total_read_cycles / pages_primed : 0;
    uint64_t avg_read_ns = pages_primed > 0 ? total_read_ns / pages_primed : 0;
    
    printf("%zu,%s,%s,%zu,%zu,%d,%lu,%lu,%lu,%lu,%lu,%lu\n",
           pg_size,
           filename,
           bit_pattern,
           num_bits,
           pages_primed,
           PAGE_STRIDE,
           (open_end_cycles - open_begin_cycles),
           (open_end_ns - open_begin_ns),
           avg_read_cycles,
           avg_read_ns,
           (total_end_cycles - total_begin_cycles),
           (total_end_ns - total_begin_ns));
    
    fflush(stdout);
    close(f_map);
    
    return 0;
}
