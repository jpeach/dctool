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

#ifndef __GDB_H__
#define __GDB_H__ 1

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif

#define GDBBUFSIZE 1024

#ifndef __MINGW32__
extern int gdb_server_socket;
extern int gdb_client_socket;
#else
extern SOCKET gdb_server_socket;
extern SOCKET gdb_client_socket;
#endif

int gdb_socket_started(void);
void gdb_socket_close(void);
int gdb_socket_open(int port);

#endif /* __GDB_H__ */

