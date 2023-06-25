/*
 * Copyright (C) 2023 Andrew Kieschnick <andrewk@austin.rr.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "download.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

int download(const char *filename, unsigned int address, unsigned int size, xprt_recv_data_t recv)
{
    int outputfd;

    unsigned char *data;
    struct timeval starttime, endtime;
    double stime, etime;

    outputfd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);

    if (outputfd < 0) {
        log_error(filename);
        return -1;
    }

    data = malloc(size);

    gettimeofday(&starttime, 0);

    recv(address, size, data);
    /* TODO: propagate and handle transport errors */

    gettimeofday(&endtime, 0);

    printf("Received %d bytes\n", size);

    stime = starttime.tv_sec + starttime.tv_usec / 1000000.0;
    etime = endtime.tv_sec + endtime.tv_usec / 1000000.0;

    printf("transferred at %f bytes / sec\n", (double) size / (etime - stime));
    fflush(stdout);

    write(outputfd, data, size);

    close(outputfd);
    free(data);

    return 0;
}
