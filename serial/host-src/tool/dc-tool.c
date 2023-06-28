/*
 * dc-tool, a tool for use with the dcload serial loader
 *
 * Copyright (C) 2023 Andrew Kieschnick <andrewk@napalm-x.com>
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

#include "config.h"

#include "serial-transport.h"
#include "minilzo.h"
#include "syscalls.h"
#include "utils.h"
#include "commands.h"
#include "gdb.h"

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
#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#endif
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>

#define VERSION PACKAGE_VERSION

#ifndef O_BINARY
#define O_BINARY 0
#endif

extern char *optarg;

void cleanup()
{
    gdb_socket_close();
    serial_xprt_cleanup();
}

void usage(void)
{
    printf("\n%s %s by Andrew \"ADK\" Kieschnick\n\n", PACKAGE, VERSION);
    printf("-x <filename> Upload and execute <filename>\n");
    printf("-u <filename> Upload <filename>\n");
    printf("-d <filename> Download to <filename>\n");
    printf("-a <address>  Set address to <address> (default: 0x8c010000)\n");
    printf("-s <size>     Set size to <size>\n");
    printf("-t <device>   Use <device> to communicate with dc (default: %s)\n", SERIALDEVICE);
    printf("-b <baudrate> Use <baudrate> (default: %d)\n", DEFAULT_SPEED);
    printf("-e            Try alternate 115200 (must also use -b 115200)\n");
    printf("-E            Use an external clock for the DC's serial port\n");
    printf("-n            Do not attach console and fileserver\n");
    printf("-p            Use dumb terminal rather than console/fileserver\n");
    printf("-q            Do not clear screen before download\n");
#ifndef __MINGW32__
    printf("-c <path>     Chroot to <path> (must be super-user)\n");
#endif
    printf("-i <isofile>  Enable cdfs redirection using iso image <isofile>\n");
    printf("-g            Start a GDB server\n");
    printf("-h            Usage information (you\'re looking at it)\n\n");
    cleanup();

    exit(0);
}

void execute(unsigned int address, unsigned int console)
{
    unsigned char c;

    printf("Sending execute command (0x%x, console=%d)...",address,console);

    serial_xprt_write_bytes("A", 1);
    serial_xprt_read_bytes(&c, 1);

    serial_xprt_write_uint(address);
    serial_xprt_write_uint(console);

    printf("executing\n");
}

/* dumb terminal mode
 * for programs that don't use dcload I/O functions
 * FIXME: should allow setting a different baud rate from what dcload uses
 * FIXME: should allow input
 */

void do_dumbterm(void)
{
    unsigned char c;

    printf("\nDumb terminal mode isn't implemented, so you get this half-assed one.\n\n");

    fflush(stdout);

    while (1) {
        serial_xprt_read_bytes(&c, 1);
        printf("%c", c);
        fflush(stdout);
    }
}

#define AVAILABLE_OPTIONS		"x:u:d:a:s:t:b:c:i:npqheEg"

int main(int argc, char *argv[])
{
    unsigned int address = 0x8c010000;
    unsigned int size = 0;
    char *filename = 0;
    char *path = 0;
    unsigned int console = 1;
    unsigned int dumbterm = 0;
    unsigned int quiet = 0;
    unsigned char command = 0;
    unsigned int speed = DEFAULT_SPEED;
    char *device_name = SERIALDEVICE;
    unsigned int device_flags = 0;
    unsigned int cdfs_redir = 0;
    char *isofile = 0;
    int someopt;

    if (argc < 2)
        usage();

    lzo_init();

    someopt = getopt(argc, argv, AVAILABLE_OPTIONS);
    while (someopt > 0) {
        switch (someopt) {
            case 'x':
                if (command) {
                    printf("You can only specify one of -x, -u, and -d\n");
                    exit(0);
                }
                command = 'x';
                filename = malloc(strlen(optarg) + 1);
                strcpy((char *)filename, optarg);
                break;
            case 'u':
                if (command) {
                    printf("You can only specify one of -x, -u, and -d\n");
                    exit(0);
                }
                command = 'u';
                filename = malloc(strlen(optarg) + 1);
                strcpy((char *)filename, optarg);
                break;
            case 'd':
                if (command) {
                    printf("You can only specify one of -x, -u, and -d\n");
                    exit(0);
                }
                command = 'd';
                filename = malloc(strlen(optarg) + 1);
                strcpy((char *)filename, optarg);
                break;
            case 'c':
#ifdef __MINGW32__
                printf("chroot is not supported on MinGW");
                exit(EXIT_FAILURE);
#else
                path = malloc(strlen(optarg) + 1);
                strcpy((char *)path, optarg);
#endif
                break;
            case 'i':
                cdfs_redir = 1;
                isofile = malloc(strlen(optarg) + 1);
                strcpy((char *)isofile, optarg);
                break;
            case 'a':
                address = strtoul(optarg, NULL, 0);
                break;
            case 's':
                size = strtoul(optarg, NULL, 0);
                break;
            case 't':
                device_name = malloc(strlen(optarg) + 1);
                strcpy(device_name, optarg);
                break;
            case 'b':
                speed = strtoul(optarg, NULL, 0);
                break;
            case 'n':
                console = 0;
                break;
            case 'p':
                console = 0;
                dumbterm = 1;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'h':
                usage();
                break;
            case 'e':
                device_flags |= SERIAL_XPRT_FLAG_SPEEDHACK;
                break;
            case 'E':
                device_flags |= SERIAL_XPRT_FLAG_EXTCLOCK;
                break;
            case 'g':
                printf("Starting a GDB server on port 2159\n");
                wsa_initialize();
                if (gdb_socket_open(2159) != 0) {
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                break;
        }

        someopt = getopt(argc, argv, AVAILABLE_OPTIONS);
    }

    if ((command == 'x') || (command == 'u')) {
        struct stat statbuf;
        if(stat((char *)filename, &statbuf)) {
            perror((char *)filename);
            exit(1);
        }
    }

    if (console)
        printf("Console enabled\n");

    if (dumbterm)
        printf("Dumb terminal enabled\n");

    if (quiet)
        printf("Quiet download\n");

#ifndef __MINGW32__
    if (path)
        printf("Chroot enabled\n");
#endif

    if (cdfs_redir)
        printf("Cdfs redirection enabled\n");

    if (serial_xprt_initialize(device_name, speed, device_flags) != 0) {
        return EXIT_FAILURE;
    }

    switch (command) {
        case 'x':
            if (cdfs_redir) {
                unsigned char c;
                c = 'H';
                serial_xprt_write_bytes(&c, 1);
                serial_xprt_read_bytes(&c, 1);
            }
            printf("Upload <%s>\n", filename);
            address = upload(filename, address, serial_xprt_send_data);
            printf("Executing at <0x%x>\n", address);
            execute(address, console);
            if (console)
                do_console(path, isofile, serial_xprt_dispatch_commands);
            else if (dumbterm)
                do_dumbterm();
            break;
        case 'u':
            printf("Upload <%s> at <0x%x>\n", filename, address);
            upload(filename, address, serial_xprt_send_data);
            break;
        case 'd':
            if (!size) {
                printf("You must specify a size (-s <size>) with download (-d <filename>)\n");
                cleanup();
                exit(0);
            }
            printf("Download %d bytes at <0x%x> to <%s>\n", size, address, filename);
            download(filename, address, size, quiet ? serial_xprt_recv_data_quiet : serial_xprt_recv_data);
            break;
        default:
            if (dumbterm)
                do_dumbterm();
            else
                usage();
            break;
    }

    cleanup();
    exit(0);
}
