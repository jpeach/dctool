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

#ifndef __UPLOAD_H__
#define __UPLOAD_H__

#include <sys/types.h>

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
#endif /* __UPLOAD_H__ */
