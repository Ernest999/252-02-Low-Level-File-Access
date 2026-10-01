#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "io_optimizer.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MIN_BLOCK_SIZE 1
#define MAX_BLOCK_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s   \n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];
    char *endptr = NULL;
    long block_size = strtol(argv[3], &endptr, 10);

    if (*endptr != '\0' || block_size < (long)MIN_BLOCK_SIZE || block_size > (long)MAX_BLOCK_SIZE) {
        fprintf(stderr, "Error: block_size must be an integer between %ld and %ld\n",
                (long)MIN_BLOCK_SIZE, (long)MAX_BLOCK_SIZE);
        return EXIT_FAILURE;
    }

    int in_fd = open(input_path, O_RDONLY);
    if (in_fd < 0) {
        perror("Error opening input file");
        return EXIT_FAILURE;
    }

    int out_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        perror("Error opening/creating output file");
        close(in_fd);
        return EXIT_FAILURE;
    }

    char *buffer = (char *)malloc((size_t)block_size);
    if (buffer == NULL) {
        perror("Error allocating buffer");
        close(in_fd);
        close(out_fd);
        return EXIT_FAILURE;
    }

    unsigned long long total_bytes = 0;
    unsigned long long read_calls = 0;
    unsigned long long write_calls = 0;

    struct timeval start_tv, end_tv;

    if (gettimeofday(&start_tv, NULL) != 0) {
        perror("Error getting start time");
        free(buffer);
        close(in_fd);
        close(out_fd);
        return EXIT_FAILURE;
    }

    while (1) {
        read_calls++;
        ssize_t bytes_read = read(in_fd, buffer, (size_t)block_size);

        if (bytes_read < 0) {
            if (errno == EINTR) {
                read_calls--;
                continue;
            }
            perror("Error during read");
            free(buffer);
            close(in_fd);
            close(out_fd);
            return EXIT_FAILURE;
        }

        if (bytes_read == 0) {
            break;
        }

        ssize_t bytes_written_total = 0;
        while (bytes_written_total < bytes_read) {
            write_calls++;
            ssize_t written = write(out_fd,
                                    buffer + bytes_written_total,
                                    (size_t)(bytes_read - bytes_written_total));
            if (written < 0) {
                if (errno == EINTR) {
                    write_calls--;
                    continue;
                }
                perror("Error during write");
                free(buffer);
                close(in_fd);
                close(out_fd);
                return EXIT_FAILURE;
            }
            bytes_written_total += written;
        }

        total_bytes += (unsigned long long)bytes_read;
    }

    if (gettimeofday(&end_tv, NULL) != 0) {
        perror("Error getting end time");
        free(buffer);
        close(in_fd);
        close(out_fd);
        return EXIT_FAILURE;
    }

    long long elapsed_us = (long long)(end_tv.tv_sec - start_tv.tv_sec) * 1000000LL +
                           (long long)(end_tv.tv_usec - start_tv.tv_usec);

    free(buffer);
    close(in_fd);
    close(out_fd);

    printf("bytes=%llu\n", total_bytes);
    printf("read_calls=%llu\n", read_calls);
    printf("write_calls=%llu\n", write_calls);
    printf("elapsed_us=%lld\n", elapsed_us);

    return EXIT_SUCCESS;
}