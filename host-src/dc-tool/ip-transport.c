/*
 * dc-tool, a tool for use with the dcload loader
 *
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

#include "config.h" // needed for newer BFD library
#include "ip-transport.h"
#include "syscalls.h"
#include "utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>
#ifndef __MINGW32__
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

/* Convenience macro. */
#define send_cmd(v, w, x, y, z) if (ip_xprt_send_command(v, w, x, y, z) == -1) return -1

#define PACKET_TIMEOUT IP_XPRT_PACKET_TIMEOUT

#ifndef __MINGW32__
static int dcsocket = -1;
#else
static SOCKET dcsocket = INVALID_SOCKET;
#endif

static int recv_response(unsigned char *buffer, int timeout);

static unsigned int time_in_usec()
{
    struct timeval thetime;

    gettimeofday(&thetime, NULL);

    return (unsigned int)(thetime.tv_sec * 1000000) + (unsigned int)thetime.tv_usec;
}

/* receive total bytes from dc and store in data */
static int recv_data(void *data, unsigned int dcaddr, unsigned int total, unsigned int quiet)
{
    unsigned char buffer[2048];
    unsigned char *i;
    int c;
    unsigned char *map = (unsigned char *)malloc((total+1023)/1024);
    int packets = 0;
    unsigned int start;
    int retval;

    memset(map, 0, (total+1023)/1024);

    if (!quiet) {
	    send_cmd(CMD_SENDBIN, dcaddr, total, NULL, 0);
    }
    else {
	    send_cmd(CMD_SENDBINQ, dcaddr, total, NULL, 0);
    }

    start = time_in_usec();

    while (((time_in_usec() - start) < PACKET_TIMEOUT)&&(packets < ((total+1023)/1024 + 1))) {
        memset(buffer, 0, 2048);

        while(((retval = recv(dcsocket, (void *)buffer, 2048, 0)) == -1)&&((time_in_usec() - start) < PACKET_TIMEOUT));

        if (retval > 0) {
            start = time_in_usec();
            if (memcmp(((command_t *)buffer)->id, CMD_DONEBIN, 4)) {
                if (((ntohl(((command_t *)buffer)->address) - dcaddr)/1024) >= ((total + 1024)/1024)) {
                    printf("Obviously bad packet, avoiding segfault\n");
                    fflush(stdout);
                }
                else {
                    map[ (ntohl(((command_t *)buffer)->address) - dcaddr)/1024 ] = 1;
                    i = data + (ntohl(((command_t *)buffer)->address) - dcaddr);

                    memcpy(i, buffer + 12, ntohl(((command_t *)buffer)->size));
                }
            }
            packets++;
        }
    }

    for(c = 0; c < (total+1023)/1024; c++) {
        if (!map[c]) {
            if ( (total - c*1024) >= 1024) {
                send_cmd(CMD_SENDBINQ, dcaddr + c*1024, 1024, NULL, 0);
            }
            else {
                send_cmd(CMD_SENDBINQ, dcaddr + c*1024, total - c*1024, NULL, 0);
            }

            start = time_in_usec();
            while(((retval = recv(dcsocket, (void *)buffer, 2048, 0)) == -1)&&((time_in_usec() - start) < PACKET_TIMEOUT));

            if (retval > 0) {
                start = time_in_usec();

                if (memcmp(((command_t *)buffer)->id, CMD_DONEBIN, 4)) {
                    map[ (ntohl(((command_t *)buffer)->address) - dcaddr)/1024 ] = 1;
                    /* printf("recv_data: got chunk for %p, %d bytes\n",
                    (void *)ntohl(((command_t *)buffer)->address), ntohl(((command_t *)buffer)->size)); */
                    i = data + (ntohl(((command_t *)buffer)->address) - dcaddr);

                    memcpy(i, buffer + 12, ntohl(((command_t *)buffer)->size));
                }

                // Get the DONEBIN
                while(((retval = recv(dcsocket, (void *)buffer, 2048, 0)) == -1)&&((time_in_usec() - start) < PACKET_TIMEOUT));
            }

            // Force us to go back and recheck
            // XXX This should be improved after recv_data can return errors.
            c = -1;
        }
    }

    free(map);

    return 0;
}

int ip_xprt_recv_data(unsigned dcaddr, size_t len, void * dst)
{
    return recv_data(dst, dcaddr, len, 0);
}

int ip_xprt_recv_data_quiet(unsigned dcaddr, size_t len, void * dst)
{
    return recv_data(dst, dcaddr, len, 1 /* quiet */);
}

/* send size bytes to dc from addr to dcaddr*/
static int send_data(unsigned char * addr, unsigned int dcaddr, unsigned int size)
{
    unsigned char buffer[2048] = {0};
    unsigned char * i = 0;
    unsigned int a = dcaddr;
    unsigned int start = 0;
    int count = 0;

    if (!size)
	    return -1;

    do {
	    send_cmd(CMD_LOADBIN, dcaddr, size, NULL, 0);
    } while(recv_response(buffer, PACKET_TIMEOUT) == -1);

    while(memcmp(((command_t *)buffer)->id, CMD_LOADBIN, 4)) {
        printf("send_data: error in response to CMD_LOADBIN, retrying... %c%c%c%c\n",buffer[0],buffer[1],buffer[2],buffer[3]);
        do {
            send_cmd(CMD_LOADBIN, dcaddr, size, NULL, 0);
        } while (recv_response(buffer, PACKET_TIMEOUT) == -1);
    }

    for(i = addr; i < addr + size; i += 1024) {
        if ((addr + size - i) >= 1024) {
            send_cmd(CMD_PARTBIN, dcaddr, 1024, i, 1024);
        }
        else {
            send_cmd(CMD_PARTBIN, dcaddr, (addr + size) - i, i, (addr + size) - i);
        }
        dcaddr += 1024;

        /* give the DC a chance to empty its rx fifo
        * this increases transfer rate on 100mbit by about 3.4x
        */
        count++;
        if (count == 15) {
            start = time_in_usec();
            while ((time_in_usec() - start) < PACKET_TIMEOUT/51);
            count = 0;
	    }
    }

    start = time_in_usec();
    /* delay a bit to try to make sure all data goes out before CMD_DONEBIN */
    while ((time_in_usec() - start) < PACKET_TIMEOUT/10);

    do {
	    send_cmd(CMD_DONEBIN, 0, 0, NULL, 0);
    } while (recv_response(buffer, PACKET_TIMEOUT) == -1);

    while(memcmp(((command_t *)buffer)->id, CMD_DONEBIN, 4)) {
        printf("send_data: error in response to CMD_DONEBIN, retrying...\n");

        do {
            send_cmd(CMD_LOADBIN, dcaddr, size, NULL, 0);
        } while (recv_response(buffer, PACKET_TIMEOUT) == -1);
    }

    while ( ntohl(((command_t *)buffer)->size) != 0) {
    /*	printf("%d bytes at 0x%x were missing, resending\n", ntohl(((command_t *)buffer)->size),ntohl(((command_t *)buffer)->address)); */
        send_cmd(CMD_PARTBIN, ntohl(((command_t *)buffer)->address), ntohl(((command_t *)buffer)->size), addr + (ntohl(((command_t *)buffer)->address) - a), ntohl(((command_t *)buffer)->size));

        do {
            send_cmd(CMD_DONEBIN, 0, 0, NULL, 0);
        } while (recv_response(buffer, PACKET_TIMEOUT) == -1);

        while(memcmp(((command_t *)buffer)->id, CMD_DONEBIN, 4)) {
            printf("send_data: error in response to CMD_DONEBIN, retrying...\n");

            do {
                send_cmd(CMD_LOADBIN, dcaddr, size, NULL, 0);
            } while (recv_response(buffer, PACKET_TIMEOUT) == -1);
        }
    }

    return 0;
}

int ip_xprt_send_data(void *data, size_t len, unsigned dcaddr)
{
    return send_data(data, dcaddr, len);
}

static int recv_response(unsigned char *buffer, int timeout)
{
    int start = time_in_usec();
    int rv = -1;

    while(((time_in_usec() - start) < timeout) && (rv == -1))
	    rv = recv(dcsocket, (void *)buffer, 2048, 0);

    return rv;
}

int ip_xprt_recv_packet(unsigned char *buffer, int timeout)
{
    return recv_response(buffer, timeout);
}

int ip_xprt_send_command(const char *command, unsigned int addr, unsigned int size, unsigned char *data, unsigned int dsize)
{
    unsigned char c_buff[2048];
    unsigned int tmp;
    int error = 0;

    memcpy(c_buff, command, 4);
    tmp = htonl(addr);
    memcpy(c_buff + 4, &tmp, 4);
    tmp = htonl(size);
    memcpy(c_buff + 8, &tmp, 4);
    if (data != 0)
	memcpy(c_buff + 12, data, dsize);

    error = send(dcsocket, (void *)c_buff, 12+dsize, 0);

    if(error == -1) {
#ifndef __MINGW32__
        if(errno == EAGAIN)
            return 0;
        fprintf(stderr, "error: %s\n", strerror(errno));
#else
        /* WSAEWOULDBLOCK is a non-fatal error,  so continue */
        if(WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;

        fprintf(stderr, "error: %d\n", WSAGetLastError());
#endif

	    return -1;
    }

    return 0;
}

static int open_socket(const char *hostname)
{
    struct sockaddr_in sin;
    struct hostent *host = 0;

    dcsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

#ifndef __MINGW32__
    if (dcsocket < 0) {
#else
    if (dcsocket == INVALID_SOCKET) {
#endif
        log_error("socket");
        return -1;
    }

    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(31313);

    host = gethostbyname(hostname);

    if (!host) {
        log_error("gethostbyname");
        return -1;
    }

    memcpy((char *)&sin.sin_addr, host->h_addr, host->h_length);

    if (connect(dcsocket, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        log_error("connect");
        return -1;
    }

#ifdef __MINGW32__
    unsigned long flags = 1;
	int failed = 0;
    failed = ioctlsocket(dcsocket, FIONBIO, &flags);
    if (failed == SOCKET_ERROR) {
        log_error("ioctlsocket");
        return -1;
    }
#else
    fcntl(dcsocket, F_SETFL, O_NONBLOCK);
#endif

    return 0;
}

int ip_xprt_initialize(const char *hostname)
{
    return open_socket(hostname);
}

void ip_xprt_cleanup(void)
{
#ifndef __MINGW32__
    close(dcsocket);
#else
    closesocket(dcsocket);
#endif
}


#define CatchError(x) if(x) return -1;

int ip_xprt_dispatch_commands(int isofd)
{
    unsigned char buffer[2048];
    struct timespec time = {.tv_sec = 0, .tv_nsec = 500000000};

    while (ip_xprt_recv_packet(buffer, IP_XPRT_PACKET_TIMEOUT) == -1) {
        nanosleep(&time, NULL);
    }

    if (!(memcmp(buffer, CMD_EXIT, 4)))
        return 1;
    if (!(memcmp(buffer, CMD_FSTAT, 4)))
        CatchError(ip_xprt_system_calls.fstat(buffer));
    if (!(memcmp(buffer, CMD_WRITE, 4)))
        CatchError(ip_xprt_system_calls.write(buffer));
    if (!(memcmp(buffer, CMD_READ, 4)))
        CatchError(ip_xprt_system_calls.read(buffer));
    if (!(memcmp(buffer, CMD_OPEN, 4)))
        CatchError(ip_xprt_system_calls.open(buffer));
    if (!(memcmp(buffer, CMD_CLOSE, 4)))
        CatchError(ip_xprt_system_calls.close(buffer));
    if (!(memcmp(buffer, CMD_CREAT, 4)))
        CatchError(ip_xprt_system_calls.create(buffer));
    if (!(memcmp(buffer, CMD_LINK, 4)))
        CatchError(ip_xprt_system_calls.link(buffer));
    if (!(memcmp(buffer, CMD_UNLINK, 4)))
        CatchError(ip_xprt_system_calls.unlink(buffer));
    if (!(memcmp(buffer, CMD_CHDIR, 4)))
        CatchError(ip_xprt_system_calls.chdir(buffer));
    if (!(memcmp(buffer, CMD_CHMOD, 4)))
        CatchError(ip_xprt_system_calls.chmod(buffer));
    if (!(memcmp(buffer, CMD_LSEEK, 4)))
        CatchError(ip_xprt_system_calls.lseek(buffer));
    if (!(memcmp(buffer, CMD_TIME, 4)))
        CatchError(ip_xprt_system_calls.time(buffer));
    if (!(memcmp(buffer, CMD_STAT, 4)))
        CatchError(ip_xprt_system_calls.stat(buffer));
    if (!(memcmp(buffer, CMD_UTIME, 4)))
        CatchError(ip_xprt_system_calls.utime(buffer));
    if (!(memcmp(buffer, CMD_BAD, 4)))
        fprintf(stderr, "command 15 should not happen... (but it did)\n");
    if (!(memcmp(buffer, CMD_OPENDIR, 4)))
        CatchError(ip_xprt_system_calls.opendir(buffer));
    if (!(memcmp(buffer, CMD_CLOSEDIR, 4)))
        CatchError(ip_xprt_system_calls.closedir(buffer));
    if (!(memcmp(buffer, CMD_READDIR, 4)))
        CatchError(ip_xprt_system_calls.readdir(buffer));
    if (!(memcmp(buffer, CMD_CDFSREAD, 4)))
        CatchError(ip_xprt_system_calls.cdfs_redir_read_sectors(isofd, buffer));
    if (!(memcmp(buffer, CMD_GDBPACKET, 4)))
        CatchError(ip_xprt_system_calls.gdbpacket(buffer));
    if(!(memcmp(buffer, CMD_REWINDDIR, 4)))
        CatchError(ip_xprt_system_calls.rewinddir(buffer));

    return 0;
}

int ip_xprt_execute(unsigned dcaddr, unsigned console, unsigned cdfsredir)
{
    unsigned char buffer[2048];

    printf("Sending execute command (0x%x, console=%d, cdfsredir=%d)...",dcaddr,console,cdfsredir);

    do {
            if (ip_xprt_send_command(CMD_EXECUTE, dcaddr, (cdfsredir << 1) | console, NULL, 0) == -1) {
                return -1;
            }
    } while (ip_xprt_recv_packet(buffer, IP_XPRT_PACKET_TIMEOUT) == -1);

    printf("executing\n");
    return 0;
}
