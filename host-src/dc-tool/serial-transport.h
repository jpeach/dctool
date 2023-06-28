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

#ifndef __SERIAL_TRANSPORT_H__
#define __SERIAL_TRANSPORT_H__

#include <sys/types.h>

int serial_xprt_send_data(void *data, size_t len, unsigned dcaddr);
int serial_xprt_recv_data(unsigned dcaddr, size_t len, void *dst);
int serial_xprt_recv_data_quiet(unsigned dcaddr, size_t len, void *dst);
int serial_xprt_dispatch_commands(int isofd);
int serial_xprt_execute(unsigned dcaddr, unsigned console, unsigned cdfsredir);

int serial_xprt_read_bytes(void *data, size_t len);
int serial_xprt_write_bytes(void *data, size_t len);
int serial_xprt_write_uint(unsigned value);
int serial_xprt_read_uint();

/* The _chunk() functions read and write len bytes of data over the serial
 * transport, but they do so chunks that may be compressed.
 */
int serial_xprt_read_chunk(void *data, size_t len);
int serial_xprt_write_chunk(void *data, size_t len);

#define SERIAL_XPRT_FLAG_SPEEDHACK  (1u << 0)
#define SERIAL_XPRT_FLAG_EXTCLOCK   (1u << 1)
#define SERIAL_XPRT_FLAG_DEBUG      (1u << 2)

int serial_xprt_initialize(const char *device, unsigned speed, unsigned flags);
void serial_xprt_cleanup(void);

#endif /* __SERIAL_TRANSPORT_H__ */
