/*
 * Receiver for strided page cache covert channel
 * Times access to every 32nd page of a file to detect cached pages
 * 
 * Usage: ./receiver_stride <file> [num_bits] [cycle_threshold]
 *   num_bits: number of strided pages to check (default: auto-detect)
 *   cycle_threshold: cycles threshold for cached vs not cached (default: 10000000)
 */

#define _GNU_SOURCE

#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define PAGE_STRIDE 32
#define USE_READ_FOR_PROBING 1
#define DEFAULT_CYCLE_THRESHOLD (100ULL * 1000ULL) //100k cycles as default threshold for cached vs not cached

static inline uint64_t rdtsc(void)
{
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t measure_page_access_cycles(int f_map, size_t pg_size, 
                                                    char* buff, size_t page_to_read,
                                                    uint64_t *read_ns)
{
    uint64_t start, end;
    struct timespec ts_start, ts_end;
    
    clock_gettime(CLOCK_REALTIME, &ts_start);
    
    // Seek to the page
    if (lseek(f_map, pg_size * page_to_read, SEEK_SET) == -1) {
        perror("lseek");
        return UINT64_MAX;
    }
    
    // Time the read operation
    start = rdtsc();
    ssize_t bytes_read = read(f_map, buff, pg_size);
    end = rdtsc();
    
    clock_gettime(CLOCK_REALTIME, &ts_end);
    
    if (bytes_read < 0) {
        perror("read");
        return UINT64_MAX;
    }
    
    if (read_ns) {
        *read_ns = (ts_end.tv_sec - ts_start.tv_sec) * 1000000000 +
                   (ts_end.tv_nsec - ts_start.tv_nsec);
    }
    
    return end - start;
}

int main(int argc, char *argv[])
{
    int f_map;
    struct stat f_map_stat;
    size_t pg_size = sysconf(_SC_PAGESIZE);
    char buff[pg_size];
    bool verbose = false;
    int arg_idx = 1;
    uint64_t cycle_threshold = DEFAULT_CYCLE_THRESHOLD;
    
    // Check for -v flag
    if (argc >= 2 && strcmp(argv[1], "-v") == 0) {
        verbose = true;
        arg_idx = 2;
    }

    if (argc < arg_idx + 1) {
        fprintf(stderr, "Usage: %s [-v] <file> [num_bits] [cycle_threshold]\n", argv[0]);
        fprintf(stderr, "  num_bits: number of strided pages to check (default: all available)\n");
        fprintf(stderr, "  cycle_threshold: threshold in cycles (default: %lu)\n", DEFAULT_CYCLE_THRESHOLD);
        exit(1);
    }
    
    const char *filename = argv[arg_idx];
    
    // Open file
    f_map = open(filename, O_RDONLY);
    if (f_map == -1) {
        fprintf(stderr, "Failed to open file %s: %s\n", filename, strerror(errno));
        exit(errno);
    }

    if (fstat(f_map, &f_map_stat) == -1) {
        perror("fstat");
        close(f_map);
        exit(errno);
    }

    if (f_map_stat.st_size == 0) {
        fprintf(stderr, "File is empty.\n");
        close(f_map);
        return 1;
    }
    
    size_t file_pgs = (f_map_stat.st_size + (pg_size - 1)) / pg_size;
    size_t max_stride_pages = file_pgs / PAGE_STRIDE;
    size_t num_bits = max_stride_pages;
    
    // Override number of bits to check if specified
    if (argc > arg_idx + 1) {
        size_t requested = strtoul(argv[arg_idx + 1], NULL, 10);
        if (requested > 0 && requested <= max_stride_pages) {
            num_bits = requested;
        }
    }
    
    // Override cycle threshold if specified
    if (argc > arg_idx + 2) {
        uint64_t requested = strtoull(argv[arg_idx + 2], NULL, 10);
        if (requested > 0) {
            cycle_threshold = requested;
        }
    }
    
    if (num_bits == 0) {
        fprintf(stderr, "Error: num_bits is 0\n");
        close(f_map);
        return 1;
    }
    
    if (verbose) {
        fprintf(stderr, "File: %s\n", filename);
        fprintf(stderr, "File size: %ld bytes\n", f_map_stat.st_size);
        fprintf(stderr, "Page size: %zu bytes\n", pg_size);
        fprintf(stderr, "Total pages: %zu\n", file_pgs);
        fprintf(stderr, "Max strided pages: %zu\n", max_stride_pages);
        fprintf(stderr, "Testing bits: %zu\n", num_bits);
        fprintf(stderr, "Cycle threshold: %lu\n", cycle_threshold);
        fprintf(stderr, "Page stride: %d\n", PAGE_STRIDE);
    }
    
    // Arrays to store results
    unsigned char *resident_bits = malloc(num_bits);
    uint64_t *cycle_times = malloc(num_bits * sizeof(uint64_t));
    uint64_t *ns_times = malloc(num_bits * sizeof(uint64_t));
    
    if (!resident_bits || !cycle_times || !ns_times) {
        perror("malloc");
        close(f_map);
        exit(1);
    }
    
    uint64_t min_cycles = UINT64_MAX;
    uint64_t max_cycles = 0;
    uint64_t total_cycles = 0;
    uint64_t total_ns = 0;
    size_t cached_count = 0;
    
    uint64_t measurement_start = rdtsc();
    
    // Measure each strided page
    for (size_t bit_idx = 0; bit_idx < num_bits; bit_idx++) {
        size_t page_num = bit_idx * PAGE_STRIDE;
        uint64_t read_ns_val = 0;
        
        uint64_t cycles = measure_page_access_cycles(f_map, pg_size, buff, page_num, &read_ns_val);
        
        if (cycles == UINT64_MAX) {
            fprintf(stderr, "Warning: Failed to measure page %zu (bit %zu)\n", page_num, bit_idx);
            resident_bits[bit_idx] = 0;
            cycle_times[bit_idx] = 0;
            ns_times[bit_idx] = 0;
            continue;
        }
        
        cycle_times[bit_idx] = cycles;
        ns_times[bit_idx] = read_ns_val;
        total_cycles += cycles;
        total_ns += read_ns_val;
        
        if (cycles < min_cycles) min_cycles = cycles;
        if (cycles > max_cycles) max_cycles = cycles;
        
        // Determine if page is cached
        if (cycles < cycle_threshold) {
            resident_bits[bit_idx] = 1;
            cached_count++;
        } else {
            resident_bits[bit_idx] = 0;
        }
        
        if (verbose) {
            fprintf(stderr, "Bit %zu (page %zu): %lu cycles, %lu ns -> %s\n",
                    bit_idx, page_num, cycles, read_ns_val,
                    resident_bits[bit_idx] ? "CACHED" : "not cached");
        }
        
        // Small delay between measurements
        usleep(100);
    }
    
    uint64_t measurement_end = rdtsc();
    
    // Print CSV header if verbose
    if (verbose) {
        printf("filename,page_size,num_bits,stride,cached_count,threshold_cycles,");
        printf("min_cycles,max_cycles,avg_cycles,avg_ns,total_measurement_cycles,");
        printf("bit_pattern,cycle_values\n");
    }
    
    // Print CSV data
    uint64_t avg_cycles = num_bits > 0 ? total_cycles / num_bits : 0;
    uint64_t avg_ns = num_bits > 0 ? total_ns / num_bits : 0;
    
    printf("%s,%zu,%zu,%d,%zu,%lu,%lu,%lu,%lu,%lu,%lu,",
           filename,
           pg_size,
           num_bits,
           PAGE_STRIDE,
           cached_count,
           cycle_threshold,
           min_cycles,
           max_cycles,
           avg_cycles,
           avg_ns,
           measurement_end - measurement_start);
    
    // Print bit pattern
    for (size_t i = 0; i < num_bits; i++) {
        fputc('0' + resident_bits[i], stdout);
    }
    printf(",");
    
    // Print cycle values (space-separated)
    for (size_t i = 0; i < num_bits; i++) {
        printf("%lu", cycle_times[i]);
        if (i < num_bits - 1) printf(" ");
    }
    printf("\n");
    
    fflush(stdout);
    
    free(resident_bits);
    free(cycle_times);
    free(ns_times);
    close(f_map);
    
    return 0;
}
