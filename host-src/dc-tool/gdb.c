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

#include "gdb.h"
#include "utils.h"

#include <string.h>
#include <unistd.h>
#ifndef __MINGW32__
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#ifndef __MINGW32__
int gdb_server_socket = INVALID_SOCKET;
int gdb_client_socket = INVALID_SOCKET;
#else
SOCKET gdb_server_socket = INVALID_SOCKET;
SOCKET gdb_client_socket = INVALID_SOCKET;
#endif

int gdb_socket_started(void)
{
#ifndef __MINGW32__
    return gdb_server_socket != -1;
#else
    return gdb_server_socket != INVALID_SOCKET;
#endif
}

void gdb_socket_close(void)
{
    if (!gdb_socket_started()) {
        return;
    }

    // Send SIGTERM to the GDB Client, telling remote DC program has ended
    char gdb_buf[16];
    strcpy(gdb_buf, "+$X0f#ee\0");

#ifdef __MINGW32__
    send(gdb_client_socket, gdb_buf, strlen(gdb_buf), 0);
    sleep(1);
    closesocket(gdb_client_socket);
    closesocket(gdb_server_socket);
#else
    write(gdb_client_socket, gdb_buf, strlen(gdb_buf));
    sleep(1);
    close(gdb_client_socket);
    close(gdb_server_socket);
#endif
}

int gdb_socket_open(int port)
{
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( port );
    server_addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );

    gdb_server_socket = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
    if (gdb_server_socket == INVALID_SOCKET) {
        log_error( "error creating gdb server socket" );
        return -1;
    }

    int checkbind = bind(gdb_server_socket, (struct sockaddr*)&server_addr, sizeof( server_addr ));
#ifdef __MINGW32__
    if (checkbind == SOCKET_ERROR) {
#else
    if (checkbind < 0) {
#endif
        log_error( "error binding gdb server socket" );
        return -1;
    }

    int checklisten = listen( gdb_server_socket, 0 );
#ifdef __MINGW32__
    if (checklisten == SOCKET_ERROR) {
#else
    if (checklisten < 0) {
#endif
        log_error( "error listening to gdb server socket" );
        return -1;
    }

    return 0;
}
