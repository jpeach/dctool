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

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <sys/types.h>

/* xprt_recv_data_t is a pointer to a transport function that copies data from
 * the target system to the host.
 *
 * dcaddr is the address on the target to copy from
 * len is the number of bytes to copy
 * dst is the host buffer to copy into, which must be at least len bytes
 *
 * Returns -1 on failure.
 */
typedef int (*xprt_recv_data_t)(unsigned dcaddr, size_t len, void *dst);

int download(const char *filename, unsigned int address, unsigned int size, xprt_recv_data_t recv);

/* xprt_send_data_t is a pointer to a transport function that copies data from
 * the host to that target system.
 *
 * data points to a buffer of len bytes.
 * dcaddr is the address on the target.
 *
 * Returns -1 on failure.
 */
typedef int (*xprt_send_data_t)(void *data, size_t len, unsigned dcaddr);

unsigned int upload(const char *filename, unsigned int address, xprt_send_data_t send);

#endif /* __COMMANDS_H__ */
