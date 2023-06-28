/*
 * dc-tool, a tool for use with the dcload ethernet loader
 *
 * Copyright (C) 2001 Andrew Kieschnick <andrewk@austin.rr.com>
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

#ifdef WITH_BFD
#include <bfd.h>
#else
#include <libelf.h>
#endif

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

#include "syscalls.h"
#include "ip-transport.h"

#include "utils.h"
#include "commands.h"
#include "gdb.h"

int _nl_msg_cat_cntr;

#define DEBUG(x, ...) fprintf(stderr, "DEBUG: "); fprintf(stderr, x, __VA_ARGS__)

#define VERSION PACKAGE_VERSION

#ifndef O_BINARY
#define O_BINARY 0
#endif

void cleanup(char **fnames)
{
    int counter;

    for(counter = 0; counter < 4; counter++)
    {
        if(fnames[counter] != 0)
            free(fnames[counter]);
    }

    ip_xprt_cleanup();
    gdb_socket_close();
}

extern char *optarg;

void usage(void)
{
    printf("\n%s %s by <andrewk@napalm-x.com>\n\n",PACKAGE,VERSION);
    printf("-x <filename> Upload and execute <filename>\n");
    printf("-u <filename> Upload <filename>\n");
    printf("-d <filename> Download to <filename>\n");
    printf("-a <address>  Set address to <address> (default: 0x8c010000)\n");
    printf("-s <size>     Set size to <size>\n");
    printf("-t <ip>       Communicate with <ip> (default is: %s)\n",DREAMCAST_IP);
    printf("-n            Do not attach console and fileserver\n");
    printf("-q            Do not clear screen before download\n");
#ifndef __MINGW32__
    printf("-c <path>     Chroot to <path> (must be super-user)\n");
#endif
    printf("-i <isofile>  Enable cdfs redirection using iso image <isofile>\n");
    printf("-r            Reset (only works when dcload is in control)\n");
    printf("-g            Start a GDB server\n");
    printf("-h            Usage information (you\'re looking at it)\n\n");
}

int execute(unsigned int address, unsigned int console, unsigned int cdfsredir)
{
    unsigned char buffer[2048];

    printf("Sending execute command (0x%x, console=%d, cdfsredir=%d)...",address,console,cdfsredir);

    do {
            if (ip_xprt_send_command(CMD_EXECUTE, address, (cdfsredir << 1) | console, NULL, 0) == -1) {
                return -1;
            }
    } while (ip_xprt_recv_packet(buffer, IP_XPRT_PACKET_TIMEOUT) == -1);

    printf("executing\n");
    return 0;
}

#define AVAILABLE_OPTIONS		"x:u:d:a:s:t:c:i:npqhrg"

int main(int argc, char *argv[])
{
    unsigned int address = 0x8c010000;
    unsigned int size = 0;
    unsigned int console = 1;
    unsigned int quiet = 0;
    unsigned char command = 0;
    unsigned int cdfs_redir = 0;
    int someopt;

    /* Dynamically allocated, so it should be freed */
    char *filename = 0;
    char *isofile = 0;
    char *path = 0;
    char *hostname = DREAMCAST_IP;
    char *cleanlist[4] = { 0, 0, 0, 0 };

    if (argc < 2) {
        usage();
        return 0;
    }

    wsa_initialize();

    someopt = getopt(argc, argv, "x:u:d:a:s:t:c:i:npqhrg");
    while (someopt > 0) {
        switch (someopt) {
        case 'x':
            if (command) {
                fprintf(stderr, "You can only specify one of -x, -u, -d, and -r\n");
                goto doclean;
            }
            command = 'x';
            filename = malloc(strlen(optarg) + 1);
            cleanlist[0] = filename;
            strcpy(filename, optarg);
            break;
        case 'u':
            if (command) {
                fprintf(stderr, "You can only specify one of -x, -u, -d, and -r\n");
                goto doclean;
            }
            command = 'u';
            filename = malloc(strlen(optarg) + 1);
            cleanlist[0] = filename;
            strcpy(filename, optarg);
            break;
        case 'd':
            if (command) {
                fprintf(stderr, "You can only specify one of -x, -u, -d, and -r\n");
                goto doclean;
            }
            command = 'd';
            filename = malloc(strlen(optarg) + 1);
            cleanlist[0] = filename;
            strcpy(filename, optarg);
            break;
        case 'c':
#ifdef __MINGW32__
            printf("chroot is not supported on MinGW");
            exit(EXIT_FAILURE);
#else
            path = malloc(strlen(optarg) + 1);
            cleanlist[1] = path;
            strcpy(path, optarg);
#endif
            break;
        case 'i':
            cdfs_redir = 1;
            isofile = malloc(strlen(optarg) + 1);
            cleanlist[2] = isofile;
            strcpy(isofile, optarg);
            break;
        case 'a':
            address = strtoul(optarg, NULL, 0);
            break;
        case 's':
            size = strtoul(optarg, NULL, 0);
            break;
        case 't':
            hostname = malloc(strlen(optarg) + 1);
            cleanlist[3] = hostname;
            strcpy(hostname, optarg);
            break;
        case 'n':
            console = 0;
            break;
        case 'q':
            quiet = 1;
            break;
        case 'h':
            usage();
            cleanup(cleanlist);
            return 0;
            break;
        case 'r':
            if (command) {
                fprintf(stderr, "You can only specify one of -x, -u, -d, and -r\n");
                goto doclean;
            }
            command = 'r';
            break;
        case 'g':
            printf("Starting a GDB server on port 2159\n");
            if (gdb_socket_open(2159) != 0) {
                exit(EXIT_FAILURE);
            }
            break;
        default:
        /* The user obviously mistyped something */
            usage();
            goto doclean;
            break;
        }
        someopt = getopt(argc, argv, AVAILABLE_OPTIONS);
    }

    if (quiet)
	    printf("Quiet download\n");

    if (cdfs_redir & (!console))
	    console = 1;

    if (console & (command=='x'))
	    printf("Console enabled\n");

#ifndef __MINGW32__
    if (path)
	    printf("Chroot enabled\n");
#endif

    if (cdfs_redir & (command=='x'))
	    printf("Cdfs redirection enabled\n");

    if (ip_xprt_initialize(hostname)<0)
    {
        fprintf(stderr, "Error opening socket\n");
        goto doclean;
    }

    switch (command) {
    case 'x':
        printf("Upload <%s>\n", filename);
        address = upload(filename, address, ip_xprt_send_data);

        if (address == -1)
            goto doclean;

        printf("Executing at <0x%x>\n", address);
        if(execute(address, console, cdfs_redir))
            goto doclean;
        if (console)
            do_console(path, isofile, ip_xprt_dispatch_commands);
        break;
    case 'u':
        printf("Upload <%s> at <0x%x>\n", filename, address);
        if(upload(filename, address, ip_xprt_send_data))
            goto doclean;
        break;
    case 'd':
        if (!size) {
            fprintf(stderr, "You must specify a size (-s <size>) with download (-d <filename>)\n");
            goto doclean;
        }
        printf("Download %d bytes at <0x%x> to <%s>\n", size, address, filename);
        if (download(filename, address, size, quiet ? ip_xprt_recv_data_quiet : ip_xprt_recv_data) == -1)
            goto doclean;
        break;
    case 'r':
        printf("Reseting...\n");
        if(ip_xprt_send_command(CMD_REBOOT, 0, 0, NULL, 0) == -1)
            goto doclean;
        break;
    default:
        usage();
        break;
    }

    cleanup(cleanlist);
    return 0;

/* Failed (I hate gotos...) */
doclean:
    cleanup(cleanlist);
    return -1;
}
