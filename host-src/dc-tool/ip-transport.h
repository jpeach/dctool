/*
 * This file is part of the dcload Dreamcast loader
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

#ifndef __IP_TRANSPORT_H__
#define __IP_TRANSPORT_H__

#include <sys/types.h>

struct _command_t {
	unsigned char id[4];
	unsigned int address;
	unsigned int size;
	unsigned char data[1];
} __attribute__ ((packed));

typedef struct _command_t command_t;

#define CMD_EXECUTE  "EXEC" /* execute */
#define CMD_LOADBIN  "LBIN" /* begin receiving binary */
#define CMD_PARTBIN  "PBIN" /* part of a binary */
#define CMD_DONEBIN  "DBIN" /* end receiving binary */
#define CMD_SENDBIN  "SBIN" /* send a binary */
#define CMD_SENDBINQ "SBIQ" /* send a binary, quiet */
#define CMD_VERSION  "VERS" /* send version info */

#define CMD_RETVAL   "RETV" /* return value */

#define CMD_REBOOT   "RBOT"  /* reboot */

#define COMMAND_LEN  12

#define CMD_EXIT     "DC00"
#define CMD_FSTAT    "DC01"
#define CMD_WRITE    "DD02"
#define CMD_READ     "DC03"
#define CMD_OPEN     "DC04"
#define CMD_CLOSE    "DC05"
#define CMD_CREAT    "DC06"
#define CMD_LINK     "DC07"
#define CMD_UNLINK   "DC08"
#define CMD_CHDIR    "DC09"
#define CMD_CHMOD    "DC10"
#define CMD_LSEEK    "DC11"
#define CMD_TIME     "DC12"
#define CMD_STAT     "DC13"
#define CMD_UTIME    "DC14"
#define CMD_BAD      "DC15"
#define CMD_OPENDIR  "DC16"
#define CMD_CLOSEDIR "DC17"
#define CMD_READDIR  "DC18"
#define CMD_CDFSREAD "DC19"
#define CMD_GDBPACKET "DC20"
#define CMD_REWINDDIR "DC21"

struct _command_3int_t {
	unsigned char id[4];
	unsigned int value0;
	unsigned int value1;
	unsigned int value2;
} __attribute__ ((__packed__));

struct _command_2int_string_t {
	unsigned char id[4];
	unsigned int value0;
	unsigned int value1;
	char string[1];
} __attribute__ ((__packed__));

struct _command_int_t {
	unsigned char id[4];
	unsigned int value0;
} __attribute__ ((__packed__));

struct _command_int_string_t {
	unsigned char id[4];
	unsigned int value0;
	char string[1];
} __attribute__ ((__packed__));

struct _command_string_t {
	unsigned char id[4];
	char string[1];
} __attribute__ ((__packed__));

struct _command_3int_string_t {
	unsigned char id[4];
	unsigned int value0;
	unsigned int value1;
	unsigned int value2;
	char string[1];
} __attribute__ ((__packed__));

typedef struct _command_3int_t command_3int_t;
typedef struct _command_2int_string_t command_2int_string_t;
typedef struct _command_int_t command_int_t;
typedef struct _command_int_string_t command_int_string_t;
typedef struct _command_string_t command_string_t;
typedef struct _command_3int_string_t command_3int_string_t;

/* fstat    fd, addr, size
 * write    fd, addr, size
 * read     fd, addr, size
 * open     flags, mode, string
 * close    fd
 * creat    mode, string
 * link     string1+string2
 * unlink   string
 * chdir    string
 * cdmod    mode, string
 * lseek    fd, offset, whence
 * time     -
 * stat     addr, size, string
 * utime    foo, actime, modtime, string
 * opendir  string
 * closedir dir
 * readdir  dir, addr, size
 * cdfsread sector, addr, size
 * gdb_packet count, size, string
 */

int ip_xprt_send_data(void *data, size_t len, unsigned dcaddr);
int ip_xprt_send_command(const char *command, unsigned int addr, unsigned int size, unsigned char *data, unsigned int dsize);
int ip_xprt_recv_data(unsigned dcaddr, size_t len, void * dst);
int ip_xprt_recv_data_quiet(unsigned dcaddr, size_t len, void * dst);
int ip_xprt_dispatch_commands(int isofd);

/* 250000 = 0.25 seconds */
#define IP_XPRT_PACKET_TIMEOUT 250000

int ip_xprt_recv_packet(unsigned char *buffer, int timeout);

int ip_xprt_initialize(const char *hostname);
void ip_xprt_cleanup(void);

#endif /* __IP_TRANSPORT_H__ */
