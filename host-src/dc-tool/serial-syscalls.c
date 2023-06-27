/*
 * This file is part of the dcload Dreamcast loader
 *
 * Copyright (C) 2034 Andrew Kieschnick <andrewk@napalm-x.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "syscalls.h"
#include "serial-transport.h"
#include "gdb.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>
#include <dirent.h>
#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define send_uint(x) serial_xprt_write_uint(x)
#define recv_uint(x) serial_xprt_read_uint(x)
#define send_data(data, len, v) serial_xprt_write_chunk(data, len)
#define recv_data(data, len, v) serial_xprt_read_chunk(data, len)

static int dc_fstat(unsigned char *buffer __attribute__((unused)))
{
    int filedes;
    struct stat filestat;
    int retval;

    filedes = recv_uint();
    retval = fstat(filedes, &filestat);

    send_uint(filestat.st_dev);
    send_uint(filestat.st_ino);
    send_uint(filestat.st_mode);
    send_uint(filestat.st_nlink);
    send_uint(filestat.st_uid);
    send_uint(filestat.st_gid);
    send_uint(filestat.st_rdev);
    send_uint(filestat.st_size);
#ifdef __MINGW32__
    send_uint(0);
    send_uint(0);
#else
    send_uint(filestat.st_blksize);
    send_uint(filestat.st_blocks);
#endif
    send_uint(filestat.st_atime);
    send_uint(filestat.st_mtime);
    send_uint(filestat.st_ctime);

    send_uint(retval);

    return 0;
}

static int dc_write(unsigned char *buffer __attribute__((unused)))
{
    int filedes;
    int retval;
    int count;
    unsigned char *data;

    filedes = recv_uint();
    count = recv_uint();

    data = malloc(count);
    recv_data(data, count, 0);

    retval = write(filedes, data, count);

    send_uint(retval);

    free(data);
    return 0;
}

static int dc_read(unsigned char *buffer __attribute__((unused)))
{
    int filedes;
    int retval;
    int count;
    unsigned char *data;

    filedes = recv_uint();
    count = recv_uint();

    data = malloc(count);
    retval = read(filedes, data, count);

    send_data(data, count, 0);

    send_uint(retval);

    free(data);
    return 0;
}

static int dc_open(unsigned char *buffer __attribute__((unused)))
{
    int namelen;
    int retval;
    int flags;
    int ourflags = 0;
    int mode;
    char *pathname;

    namelen = recv_uint();

    pathname = malloc(namelen);

    recv_data(pathname, namelen, 0);

    flags = recv_uint();
    mode = recv_uint();

    /* translate flags */

    if (flags & 0x0001)
        ourflags |= O_WRONLY;
    if (flags & 0x0002)
        ourflags |= O_RDWR;
    if (flags & 0x0008)
        ourflags |= O_APPEND;
    if (flags & 0x0200)
        ourflags |= O_CREAT;
    if (flags & 0x0400)
        ourflags |= O_TRUNC;
    if (flags & 0x0800)
        ourflags |= O_EXCL;

    retval = open(pathname, ourflags | O_BINARY, mode);

    send_uint(retval);

    free(pathname);
    return 0;
}

static int dc_close(unsigned char *buffer __attribute__((unused)))
{
    int filedes;
    int retval;

    filedes = recv_uint();

    retval = close(filedes);

    send_uint(retval);
    return 0;
}

static int dc_create(unsigned char *buffer __attribute__((unused)))
{
    int namelen;
    char *pathname;
    int retval;
    int mode;

    namelen = recv_uint();

    pathname = malloc(namelen);

    recv_data(pathname, namelen, 0);

    mode = recv_uint();

    retval = creat(pathname, mode);

    send_uint(retval);

    free(pathname);
    return 0;
}

static int dc_link(unsigned char *buffer __attribute__((unused)))
{
    int namelen1, namelen2;
    char *pathname1, *pathname2;
    int retval;

    namelen1 = recv_uint();
    pathname1 = malloc(namelen1);

    recv_data(pathname1, namelen1, 0);

    namelen2 = recv_uint();
    pathname2 = malloc(namelen2);

    recv_data(pathname2, namelen2, 0);

#ifdef __MINGW32__
    /* Copy the file on Windows */
    retval = CopyFileA(pathname1, pathname2, 0);
#else
    retval = link(pathname1, pathname2);
#endif

    send_uint(retval);

    free(pathname1);
    free(pathname2);
    return 0;
}

static int dc_unlink(unsigned char *buffer __attribute__((unused)))
{
    int namelen;
    char *pathname;
    int retval;

    namelen = recv_uint();

    pathname = malloc(namelen);

    recv_data(pathname, namelen, 0);

    retval = unlink(pathname);

    send_uint(retval);

    free(pathname);
    return 0;
}

static int dc_chdir(unsigned char *buffer __attribute__((unused)))
{
    int namelen;
    char *pathname;
    int retval;

    namelen = recv_uint();

    pathname = malloc(namelen);

    recv_data(pathname, namelen, 0);

    retval = chdir(pathname);

    send_uint(retval);

    free(pathname);
    return 0;
}

static int dc_chmod(unsigned char *buffer __attribute__((unused)))
{
    int namelen;
    int mode;
    char *pathname;
    int retval;

    namelen = recv_uint();

    pathname = malloc(namelen);

    recv_data(pathname, namelen, 0);

    mode = recv_uint();

    retval = chmod(pathname, mode);

    send_uint(retval);

    free(pathname);
    return 0;
}

static int dc_lseek(unsigned char *buffer __attribute__((unused)))
{
    int filedes;
    int offset;
    int whence;
    int retval;

    filedes = recv_uint();
    offset = recv_uint();
    whence = recv_uint();

    retval = lseek(filedes, offset, whence);

    send_uint(retval);
    return 0;
}

static int dc_time(unsigned char *buffer __attribute__((unused)))
{
    time_t t;

    time(&t);

    send_uint(t);
    return 0;
}

static int dc_stat(unsigned char *buffer __attribute__((unused)))
{
    int namelen;
    char *filename;
    struct stat filestat;
    int retval;

    namelen = recv_uint();

    filename = malloc(namelen);

    recv_data(filename, namelen, 0);

    retval = stat(filename, &filestat);

    send_uint(filestat.st_dev);
    send_uint(filestat.st_ino);
    send_uint(filestat.st_mode);
    send_uint(filestat.st_nlink);
    send_uint(filestat.st_uid);
    send_uint(filestat.st_gid);
    send_uint(filestat.st_rdev);
    send_uint(filestat.st_size);
#ifdef __MINGW32__
    send_uint(0);
    send_uint(0);
#else
    send_uint(filestat.st_blksize);
    send_uint(filestat.st_blocks);
#endif
    send_uint(filestat.st_atime);
    send_uint(filestat.st_mtime);
    send_uint(filestat.st_ctime);

    send_uint(retval);

    free(filename);
    return 0;
}

static int dc_utime(unsigned char *buffer __attribute__((unused)))
{
    char *pathname;
    int namelen;
    struct utimbuf tbuf;
    int foo;
    int retval;

    namelen = recv_uint();

    pathname = malloc(namelen);

    recv_data(pathname, namelen, 0);

    foo = recv_uint();

    if (foo) {
        tbuf.actime = recv_uint();
        tbuf.modtime = recv_uint();

        retval = utime(pathname, &tbuf);
    } else {
        retval = utime(pathname, 0);
    }

    send_uint(retval);

    free(pathname);
    return 0;
}

static int dc_opendir(unsigned char *buffer __attribute__((unused)))
{
    DIR *somedir;
    char *dirname;
    int namelen;

    namelen = recv_uint();

    dirname = malloc(namelen);

    recv_data(dirname, namelen, 0);

    somedir = opendir(dirname);

    // XXX(jpeach) pointer truncated to 32 bits!
    send_uint((unsigned int)somedir);

    free(dirname);
    return 0;
}

static int dc_closedir(unsigned char *buffer __attribute__((unused)))
{
    DIR *somedir;
    int retval;

    somedir = (DIR *) recv_uint();

    retval = closedir(somedir);

    send_uint(retval);
    return 0;
}

static int dc_readdir(unsigned char *buffer __attribute__((unused)))
{
    DIR *somedir;
    struct dirent *somedirent;

    somedir = (DIR *) recv_uint();

    somedirent = readdir(somedir);

    if (somedirent) {
        send_uint(1);
        send_uint(somedirent->d_ino);
#ifdef _WIN32
        send_uint(0);
        send_uint(0);
        send_uint(0);
#else
#ifdef __APPLE_CC__
        send_uint(0);
#else
#if !defined(__FreeBSD__) && !defined(__CYGWIN__)
        send_uint(somedirent->d_off);
#endif
#endif
#ifndef __CYGWIN__
        send_uint(somedirent->d_reclen);
#endif
        send_uint(somedirent->d_type);
#endif
        send_uint(strlen(somedirent->d_name)+1);
        send_data(somedirent->d_name, strlen(somedirent->d_name)+1, 0);
    } else {
        send_uint(0);
    }

    return 0;
}

static int dc_rewinddir(unsigned char *buffer __attribute__((unused)))
{
    DIR *somedir;

    somedir = (DIR *) recv_uint();

    rewinddir(somedir);

    send_uint(0);
    return 0;
}

static int dc_cdfs_redir_read_sectors(int isofd, unsigned char *buffer __attribute__((unused)))
{
    int start;
    int num;
    unsigned char * buf;

    start = recv_uint();
    num = recv_uint();

    start -= 150;

    lseek(isofd, start * 2048, SEEK_SET);

    buf = malloc(num * 2048);

    read(isofd, buf, num * 2048);

    send_data(buf, num * 2048, 0);
    free(buf);
    return 0;
}

static int dc_gdbpacket(unsigned char *buffer __attribute__((unused)))
{
    size_t in_size, out_size;

    static char gdb_buf[GDBBUFSIZE];

    int retval = 0;

    in_size = recv_uint();
    out_size = recv_uint();

    if (in_size)
        recv_data(gdb_buf, in_size > GDBBUFSIZE ? GDBBUFSIZE : in_size, 0);

    if (gdb_server_socket == INVALID_SOCKET) {
        send_uint(-1);
        return -1;
    }

    if (gdb_client_socket == INVALID_SOCKET) {
        printf( "waiting for gdb client connection...\n" );
        gdb_client_socket = accept( gdb_server_socket, NULL, NULL );

        if (gdb_client_socket == INVALID_SOCKET) {
            perror("error accepting gdb server connection");
            send_uint(-1);
            return -1;
        }
    }

    if (in_size)
        send(gdb_client_socket, gdb_buf, in_size, 0);

    if (out_size) {
        retval = recv(gdb_client_socket, gdb_buf, out_size > GDBBUFSIZE ? GDBBUFSIZE : out_size, 0);
        if (retval == 0)
            gdb_client_socket = INVALID_SOCKET;
    }
#ifdef __MINGW32__
    if(retval == SOCKET_ERROR) {
        fprintf(stderr, "Got socket error: %d\n", WSAGetLastError());
        return -1;
    }
#endif
    send_uint(retval);
    if (retval > 0) {
        send_data(gdb_buf, retval, 0);
    }

    return 0;
}

const dc_system_calls_t serial_xprt_system_calls = {
    .fstat = dc_fstat,
    .write = dc_write,
    .read = dc_read,
    .open = dc_open,
    .close = dc_close,
    .create = dc_create,
    .link = dc_link,
    .unlink = dc_unlink,
    .chdir = dc_chdir,
    .chmod = dc_chmod,
    .lseek = dc_lseek,
    .time = dc_time,
    .stat = dc_stat,
    .utime = dc_utime,
    .opendir = dc_opendir,
    .readdir = dc_readdir,
    .closedir = dc_closedir,
    .rewinddir = dc_rewinddir,
    .cdfs_redir_read_sectors = dc_cdfs_redir_read_sectors,
    .gdbpacket = dc_gdbpacket,
};
