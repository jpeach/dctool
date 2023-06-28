/*
 * dc-tool, a tool for use with the dcload loader
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
#include "commands.h"
#include "utils.h"
#include "gdb.h"

#include "ip-transport.h"
#include "serial-transport.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <minilzo.h>

#define DCTOOL_COMMON_OPTS      "x:u:d:a:s:t:c:i:npqh"
#define DCTOOL_IP_OPTS          DCTOOL_COMMON_OPTS "rg"
#define DCTOOL_SERIAL_OPTS	DCTOOL_COMMON_OPTS "b:eEg"

#define DCTOOL_GDB_SERVER_PORT  2159

extern char *optarg;

static int dctool_main_serial(int argc, const char *argv[]);
static int dctool_main_ip(int argc, const char *argv[]);

static void usage(void) __attribute__ ((noreturn));

/* dumb terminal mode
 * for programs that don't use dcload I/O functions
 * FIXME: should allow setting a different baud rate from what dcload uses
 * FIXME: should allow input
 */

static void do_dumbterm(void)
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

static void usage(void)
{
    printf("%s %s by Andrew \"ADK\" Kieschnick\n\n", PACKAGE, PACKAGE_VERSION);
    printf("Usage: %s ip|serial OPTIONS...\n", PACKAGE);

    printf("\nCommon options:\n");
    printf("    -x <filename> Upload and execute <filename>\n");
    printf("    -u <filename> Upload <filename>\n");
    printf("    -d <filename> Download to <filename>\n");
    printf("    -a <address>  Set address to <address> (default: 0x8c010000)\n");
    printf("    -s <size>     Set size to <size>\n");
#ifndef __MINGW32__
    printf("    -c <path>     Chroot to <path> (must be super-user)\n");
#endif
    printf("    -i <isofile>  Enable cdfs redirection using iso image <isofile>\n");
    printf("    -g            Start a GDB server\n");
    printf("    -n            Do not attach console and fileserver\n");
    printf("    -q            Do not clear screen before download\n");
    printf("    -h            Usage information (you\'re looking at it)\n\n");

    printf("\nIP options:\n");
    printf("    -t <ip>       Communicate with <ip> (default is: %s)\n", DREAMCAST_IP);
    printf("    -r            Reset (only works when dcload is in control)\n");

    printf("\nSerial options:\n");
    printf("    -t <device>   Use <device> to communicate with dc (default: %s)\n", SERIALDEVICE);
    printf("    -b <baudrate> Use <baudrate> (default: %d)\n", DEFAULT_SPEED);
    printf("    -e            Try alternate 115200 (must also use -b 115200)\n");
    printf("    -E            Use an external clock for the DC's serial port\n");
    printf("    -p            Use dumb terminal rather than console/fileserver\n");

    exit(EXIT_SUCCESS);
}

int main(int argc, const char *argv[])
{
    int (*subcommand)(int, const char *[]);

    if (argc < 2) {
        usage();
    }

    lzo_init();
    wsa_initialize();

    if (strcmp(argv[1], "ip") == 0) {
        subcommand = dctool_main_ip;
    } else if (strcmp(argv[1], "serial") == 0) {
        subcommand = dctool_main_serial;
    } else {
        usage();
    }

    argv[1] = argv[0];
    return subcommand(argc - 1, argv + 1);
}

static int dctool_main_ip(int argc, const char *argv[])
{
    unsigned int address = 0x8c010000;
    unsigned int size = 0;
    unsigned int console = 1;
    unsigned int quiet = 0;
    unsigned char command = 0;
    unsigned int cdfs_redir = 0;
    int someopt;

    char *filename = 0;
    char *isofile = 0;
    char *path = 0;
    char *hostname = DREAMCAST_IP;

    if (argc < 2) {
        usage();
    }

    atexit(ip_xprt_cleanup);

    someopt = getopt(argc, argv, DCTOOL_IP_OPTS);
    while (someopt > 0) {
        switch (someopt) {
        case 'x':
            if (command) {
                fprintf(stderr, "You can only specify one of -x, -u, -d, and -r\n");
                return EXIT_FAILURE;
            }
            command = 'x';
            filename = strdup(optarg);
            break;
        case 'u':
            if (command) {
                fprintf(stderr, "You can only specify one of -x, -u, -d, and -r\n");
                return EXIT_FAILURE;
            }
            command = 'u';
            filename = strdup(optarg);
            break;
        case 'd':
            if (command) {
                fprintf(stderr, "You can only specify one of -x, -u, -d, and -r\n");
                return EXIT_FAILURE;
            }
            command = 'd';
            filename = strdup(optarg);
            break;
        case 'c':
#ifdef __MINGW32__
            printf("chroot is not supported on MinGW");
            return EXIT_FAILURE;
#else
            path = strdup(optarg);
#endif
            break;
        case 'i':
            cdfs_redir = 1;
            isofile = strdup(optarg);
            break;
        case 'a':
            address = strtoul(optarg, NULL, 0);
            break;
        case 's':
            size = strtoul(optarg, NULL, 0);
            break;
        case 't':
            hostname = strdup(optarg);
            break;
        case 'n':
            console = 0;
            break;
        case 'q':
            quiet = 1;
            break;
        case 'h':
            usage();
            break;
        case 'r':
            if (command) {
                fprintf(stderr, "You can only specify one of -x, -u, -d, and -r\n");
                return EXIT_FAILURE;
            }
            command = 'r';
            break;
        case 'g':
            printf("Starting a GDB server on port %d\n", DCTOOL_GDB_SERVER_PORT);
            if (gdb_socket_open(DCTOOL_GDB_SERVER_PORT) != 0) {
                exit(EXIT_FAILURE);
            }

            atexit(gdb_socket_close);
            break;
        default:
            /* The user obviously mistyped something */
            usage();
            break;
        }

        someopt = getopt(argc, argv, DCTOOL_IP_OPTS);
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

    if (ip_xprt_initialize(hostname) < 0) {
        fprintf(stderr, "Error opening socket\n");
        return EXIT_FAILURE;
    }

    switch (command) {
    case 'x':
        printf("Upload <%s>\n", filename);
        address = upload(filename, address, ip_xprt_send_data);
        if (address == -1)
            return EXIT_FAILURE;

        printf("Executing at <0x%x>\n", address);
        if (ip_xprt_execute(address, console, cdfs_redir))
            return EXIT_FAILURE;

        if (console)
            do_console(path, isofile, ip_xprt_dispatch_commands);

        break;
    case 'u':
        printf("Upload <%s> at <0x%x>\n", filename, address);
        if (upload(filename, address, ip_xprt_send_data))
            return EXIT_FAILURE;
        break;
    case 'd':
        if (!size) {
            fprintf(stderr, "You must specify a size (-s <size>) with download (-d <filename>)\n");
            return EXIT_FAILURE;
        }

        printf("Download %d bytes at <0x%x> to <%s>\n", size, address, filename);
        if (download(filename, address, size, quiet ? ip_xprt_recv_data_quiet : ip_xprt_recv_data) == -1)
            return EXIT_FAILURE;

        break;
    case 'r':
        printf("Resetting...\n");
        if (ip_xprt_send_command(CMD_REBOOT, 0, 0, NULL, 0) == -1)
            return EXIT_FAILURE;

        break;
    default:
        usage();
        break;
    }

    return EXIT_SUCCESS;
}

static int dctool_main_serial(int argc, const char *argv[])
{
    unsigned int address = 0x8c010000;
    unsigned int size = 0;
    unsigned int console = 1;
    unsigned int dumbterm = 0;
    unsigned int quiet = 0;
    unsigned char command = 0;
    unsigned int speed = DEFAULT_SPEED;
    unsigned int device_flags = 0;
    unsigned int cdfs_redir = 0;
    int someopt;

    char *filename = 0;
    char *isofile = 0;
    char *path = 0;
    char *device_name = SERIALDEVICE;

    if (argc < 2) {
        usage();
    }

    atexit(serial_xprt_cleanup);

    someopt = getopt(argc, argv, DCTOOL_SERIAL_OPTS);
    while (someopt > 0) {
        switch (someopt) {
            case 'x':
                if (command) {
                    printf("You can only specify one of -x, -u, and -d\n");
                    return EXIT_FAILURE;
                }
                command = 'x';
                filename = strdup(optarg);
                break;
            case 'u':
                if (command) {
                    printf("You can only specify one of -x, -u, and -d\n");
                    return EXIT_FAILURE;
                }
                command = 'u';
                filename = strdup(optarg);
                break;
            case 'd':
                if (command) {
                    printf("You can only specify one of -x, -u, and -d\n");
                    return EXIT_FAILURE;
                }
                command = 'd';
                filename = strdup(optarg);
                break;
            case 'c':
#ifdef __MINGW32__
                printf("chroot is not supported on MinGW");
                return EXIT_FAILURE;
#else
                path = strdup(optarg);
#endif
                break;
            case 'i':
                cdfs_redir = 1;
                isofile = strdup(optarg);
                break;
            case 'a':
                address = strtoul(optarg, NULL, 0);
                break;
            case 's':
                size = strtoul(optarg, NULL, 0);
                break;
            case 't':
                device_name = strdup(optarg);
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
                printf("Starting a GDB server on port %d\n", DCTOOL_GDB_SERVER_PORT);
                if (gdb_socket_open(DCTOOL_GDB_SERVER_PORT) != 0) {
                    return EXIT_FAILURE;
                }

                atexit(gdb_socket_close);
                break;
            default:
                break;
        }

        someopt = getopt(argc, argv, DCTOOL_SERIAL_OPTS);
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

    if (cdfs_redir & (command=='x'))
        printf("Cdfs redirection enabled\n");

    if (serial_xprt_initialize(device_name, speed, device_flags) != 0) {
        return EXIT_FAILURE;
    }

    switch (command) {
        case 'x':
            printf("Upload <%s>\n", filename);
            address = upload(filename, address, serial_xprt_send_data);

            printf("Executing at <0x%x>\n", address);
            serial_xprt_execute(address, console, cdfs_redir);

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
                return EXIT_FAILURE;
            }

            printf("Download %d bytes at <0x%x> to <%s>\n", size, address, filename);
            download(filename, address, size, quiet ? serial_xprt_recv_data_quiet : serial_xprt_recv_data);
            break;
        case 'r':
            // TODO(jpeach) support reboot over serial.
            /* fallthru */
        default:
            usage();
            break;
    }

    return EXIT_SUCCESS;
}
