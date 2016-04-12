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


/* search buffer size is a compromise between over-reading and system calls */
uint8_t search_buffer[32*1024];  /* buffer to hold current junk */


typedef struct _ts_off_ {
    off_t    offset;
    time_t   ts;
    off_t    nearest;
    time_t   nearest_ts;
    uint64_t bytes;
} ts_off_t;


int get_next_timestamp(off_t pos, int fd, ts_off_t* ts_info)
{
    struct tm conv;
    int       bytes_read;
    off_t     i;
    off_t     total_bytes_read = 0;
    off_t     offset;
    char*     ptr;
    time_t    ts;

#ifdef BTSCH_DEBUG
    char      date[16];
    memset(date, 0, sizeof(date));
#endif

    for (;;) {
        bytes_read = pread(fd, search_buffer, sizeof(search_buffer), pos);
        if (bytes_read <= 0) {
            return -1;
        }
        total_bytes_read += bytes_read;
        for (i=0; i < (bytes_read - 16); i++) {
            if ((search_buffer[i] == '\n') &&
                (search_buffer[i+1] != '\t') &&
                (search_buffer[i+16] == '\t')) {
                /* convert start time to epoch */
                memset(&conv, 0, sizeof(conv));
                ptr = strptime(&search_buffer[i+1], "%y%m%d%n%T", &conv);
                if (ptr == (char*)&(search_buffer[i+16])) {
#ifdef BTSCH_DEBUG
                    memcpy(date, &search_buffer[i+1], 15);
                    fprintf(stderr, "ts = '%s'\n", date);
#endif
                    ts = mktime(&conv);
                    ts_info->ts = ts;
                    ts_info->offset = pos + i + 1;
                    ts_info->bytes = total_bytes_read;
                    return 0;
                }
            }
        }
        if (i == 0) {
            break; // eof
        }
        /* use an overlapped read in case we chopped a timestamp on the previous one */
        pos += i;
    }
}


ts_off_t* search(time_t ts, ts_off_t* ts_info, off_t low, off_t high, int in_fd)
{
    off_t    mid = 0;
    uint64_t total_bytes = 0;
    int      err;

    for (;;) {
        mid = (low + high) / 2;
        if ((mid == low) || (mid == high)) {
            return (ts_off_t*)NULL;
        }
        err = get_next_timestamp(mid, in_fd, ts_info);
        if (err) {
            return (ts_off_t*)NULL;
        }
        total_bytes += ts_info->bytes;
        if (ts_info->ts > ts) {
            high = mid;
            ts_info->nearest = ts_info->offset;
            ts_info->nearest_ts = ts_info->ts;
        } else if (ts_info->ts < ts) {
            low = mid;
        } else {
#ifdef BTSCH_DEBUG
            fprintf(stderr, "found timestamp at %ld %ld\n", ts_info->offset, total_bytes);
#endif
            break;
        }
    }
    return ts_info;
}


int parse_file(time_t start, time_t stop, off_t st_size, int in_fd, FILE* out_f)
{
    off_t     offset;
    int       i;
    int       bytes;
    int       read_bytes;
    uint64_t  total_bytes = 0;
    ts_off_t  ts_last;
    ts_off_t  ts_start;
    ts_off_t* ts_ptr;

    ts_start.ts = start;
    ts_start.offset = 0;

    if (start > 0) {
        ts_ptr = search(start, &ts_start, 0, st_size, in_fd);
        if (ts_ptr == (ts_off_t*)NULL) {
            fprintf(stderr, "start not found\n");
            return 1;
        }
    }

    memset(&ts_last, 0, sizeof(ts_last));
    ts_ptr = search(stop, &ts_last, ts_start.offset, st_size, in_fd);
    if (ts_ptr == (ts_off_t*)NULL) {
        ts_last.ts = stop;
        if ((ts_last.nearest > 0) && (ts_last.nearest < st_size)) {
            ts_last.offset = ts_last.nearest;
            fprintf(stderr, "stop not found, using %ld instead\n", ts_last.nearest_ts);
        } else {
            ts_last.offset = st_size;
            fprintf(stderr, "stop not found, using end of file\n");
        }
    }

    /* write out requested section */
    total_bytes = ts_last.offset - ts_start.offset;
#ifdef BTSCH_DEBUG
    fprintf(stderr, "total_bytes = %ld\n", total_bytes);
#endif
    offset = ts_start.offset;
    for (i=0; i < total_bytes;) {
        read_bytes = total_bytes - i;
        if (read_bytes > sizeof(search_buffer)) {
            read_bytes = sizeof(search_buffer);
        }
        bytes = pread(in_fd, search_buffer, read_bytes, offset);
        if (bytes <= 0) {
            break;
        }
        fwrite(search_buffer, bytes, 1, out_f);
        offset += bytes;
        i += bytes;
    }
    return 0;
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
        fprintf(stderr, "\tstart and stop times are in the format of 'YYMMDD hh:mm:ss'\n");
        fprintf(stderr, "\tuse 0 for start time to start at the beginning of file\n");
        fprintf(stderr, "\tthis tool uses strptime which will blow up in 2068\n");
        return 2;
    }

    /* convert start time to epoch */
    if (strcmp(argv[1], "0") != 0) {
        memset(&conv, 0, sizeof(conv));
        ptr = strptime(argv[1], "%y%m%d%n%T", &conv);
        if (ptr == (char*)NULL) {
            perror("Couldn't convert start time");
            return 1;
        }
        start = mktime(&conv);
    } else {
        start = 0;
    }

    /* convert stop time to epoch */
    memset(&conv, 0, sizeof(conv));
    ptr = strptime(argv[2], "%y%m%d%n%T", &conv);
    if (ptr == (char*)NULL) {
        perror("Couldn't convert stop time");
        return 1;
    }
    stop = mktime(&conv);

    if (stop < start) {
        fprintf(stderr, "stop is less than start\n");
        return 1;
    }

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
