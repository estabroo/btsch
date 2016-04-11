/*
 * binary search on timestamps
 */

#define _GNU_SOURCE
#include <features.h>
#include <time.h>  // for strptime

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


uint8_t search_buffer[32*1024];  /* buffer to hold current junk */
const int search_buffer_size = sizeof(search_buffer) - 16;

typedef struct _ts_off_ {
    off_t    offset;
    time_t   ts;
    uint64_t bytes;
} ts_off_t;


int get_next_timestamp(off_t pos, int fd, ts_off_t* ts_info)
{
    struct tm conv;
    int       i;
    int       read;
    off_t     total_bytes_read = 0;
    char*     ptr;
    time_t    ts;
    int       loops = 0;
    char      date[20];


    memset(date, 0, sizeof(date));
    for (;;) {
        loops += 1;
        read = pread(fd, search_buffer, sizeof(search_buffer), pos);
        if (read <= 0) {
            perror("read failed");
            return -1;
        }
        total_bytes_read += read;
        for (i=0; i < search_buffer_size; i++) {
            if ((search_buffer[i] == '\n') && (search_buffer[i+1] != '\t') && (search_buffer[i+16] == '\t')) {
                /* convert start time to epoch */
                memset(&conv, 0, sizeof(conv));
                ptr = strptime(&search_buffer[i+1], "%y%m%d%n%T", &conv);
                if (ptr == (char*)&(search_buffer[i+16])) {
                    memcpy(date, &search_buffer[i+1], 18);
                    fprintf(stderr, "ts = %s\n", date);
                    ts = mktime(&conv);
                    ts_info->ts = ts;
                    ts_info->offset = pos + i + 1;
                    ts_info->bytes = total_bytes_read;
                    return 0;
                }
            }
        }
        pos += search_buffer_size;
    }
}


int parse_file(time_t start, time_t stop, off_t st_size, int in_fd, FILE* out_f)
{
    off_t    mid;
    off_t    low = 0;
    off_t    high = st_size;
    int      err;
    int      matched = 0;
    int      loops = 0;
    uint64_t total_bytes = 0;
    ts_off_t ts_info;

    while (!matched) {
        loops += 1;
        mid = (low + high) / 2;
        if ((mid == low) || (mid == high)) {
            fprintf(stderr, "start not found\n");
            exit(0);
        }
        err = get_next_timestamp(mid, in_fd, &ts_info);
        if (err) {
            fprintf(stderr, "start not found\n");
            exit(0);
        }
        fprintf(stderr, "%ld %ld %ld %ld %ld\n", low, high, mid, start, ts_info.ts);
        total_bytes += ts_info.bytes;
        if (ts_info.ts > start) {
            high = mid;
        } else if (ts_info.ts < start) {
            low = mid;
        } else {
            fprintf(stderr, "found timestamp at %ld %d %ld\n", ts_info.offset, loops, total_bytes);
            exit(0);
        }
    }
}


int main(int argc, char* argv[])
{
    struct stat stats;
    int         in_fd;
    FILE*       out_f;
    int         err;
    struct tm   conv;
    time_t      start;
    time_t      stop;
    char *      ptr;

    if ((argc != 4) && (argc != 5)) {
        fprintf(stderr, "usage: %s start_time stop_time input_filename [output_filename]\n", argv[0]);
        return 2;
    }

    /* convert start time to epoch */
    memset(&conv, 0, sizeof(conv));
    ptr = strptime(argv[1], "%y%m%d%n%T", &conv);
    if (ptr == (char*)NULL) {
        perror("Couldn't convert start time");
        return 1;
    }
    start = mktime(&conv);

    /* convert start time to epoch */
    memset(&conv, 0, sizeof(conv));
    ptr = strptime(argv[2], "%y%m%d%n%T", &conv);
    if (ptr == (char*)NULL) {
        //perror("Couldn't convert stop time");
        //return 1;
    }
    stop = mktime(&conv);

    /* setup output */
    if (argc == 4) {
        out_f = stdout;
    } else {
        if (strcmp(argv[4], "-") == 0) {
            out_f = stdout;
        } else {
            out_f = fopen(argv[4], "w");
            if (err != 0) {
                perror(argv[4]);
                return 1;
            }
        }
    }

    /* verify input */
    err = stat(argv[3], &stats);
    if (err != 0) {
        perror(argv[3]);
    }
    in_fd = open(argv[3], O_RDONLY);
    if (in_fd < 0) {
        perror(argv[3]);
        return 1;
    }

    return parse_file(start, stop, stats.st_size, in_fd, out_f);
}
