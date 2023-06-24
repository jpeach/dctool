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
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <minilzo.h>

extern int dctool_main_serial(int argc, const char *argv[]);
extern int dctool_main_ip(int argc, const char *argv[]);

static void usage(void) __attribute__ ((noreturn));

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
    printf("    -t <ip>       Communicate with <ip> (default is: %s)\n",DREAMCAST_IP);
    printf("    -r            Reset (only works when dcload is in control)\n");

    printf("\nSerial options:\n");
    printf("    -t <device>   Use <device> to communicate with dc (default: %s)\n", SERIALDEVICE);
    printf("    -b <baudrate> Use <baudrate> (default: %d)\n", DEFAULT_SPEED);
    printf("    -e            Try alternate 115200 (must also use -b 115200)\n");
    printf("    -E            Use an external clock for the DC's serial port\n");
    printf("    -p            Use dumb terminal rather than console/fileserver\n");

    exit(EXIT_SUCCESS);
}

int dctool_main_serial(int argc, const char *argv[])
{
    fprintf(stderr, "%s: not implemented", PACKAGE);
    return EXIT_FAILURE;
}

int dctool_main_ip(int argc, const char *argv[])
{
    fprintf(stderr, "%s: not implemented", PACKAGE);
    return EXIT_FAILURE;
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
