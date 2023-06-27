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

#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__

struct dc_system_calls {
    int (*fstat)(unsigned char * buffer);
    int (*write)(unsigned char * buffer);
    int (*read)(unsigned char * buffer);
    int (*open)(unsigned char * buffer);
    int (*close)(unsigned char * buffer);
    int (*create)(unsigned char * buffer);
    int (*link)(unsigned char * buffer);
    int (*unlink)(unsigned char * buffer);
    int (*chdir)(unsigned char * buffer);
    int (*chmod)(unsigned char * buffer);
    int (*lseek)(unsigned char * buffer);
    int (*time)(unsigned char * buffer);
    int (*stat)(unsigned char * buffer);
    int (*utime)(unsigned char * buffer);

    int (*opendir)(unsigned char * buffer);
    int (*readdir)(unsigned char * buffer);
    int (*closedir)(unsigned char * buffer);
    int (*rewinddir)(unsigned char * buffer);

    int (*cdfs_redir_read_sectors)(int isofd, unsigned char * buffer);

    int (*gdbpacket)(unsigned char * buffer);
};

typedef struct dc_system_calls dc_system_calls_t;

extern const dc_system_calls_t ip_xprt_system_calls;
extern const dc_system_calls_t serial_xprt_system_calls;

#endif /* __SYSCALLS_H__ */
