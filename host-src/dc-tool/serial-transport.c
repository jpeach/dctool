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

#include "serial-transport.h"
#include "minilzo.h"
#include "syscalls.h"
#include "utils.h"
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

#ifdef __APPLE__
#include <IOKit/serial/ioss.h>
#include <sys/ioctl.h>
#endif

#define INITIAL_SPEED  57600

#define DCLOADBUFFER	16384 /* was 8192 */

#ifdef _WIN32
#define DATA_BITS	8
#define PARITY_SET	NOPARITY
#define STOP_BITS	ONESTOPBIT
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define HEAP_ALLOC(var,size) \
        long __LZO_MMODEL var [ ((size) + (sizeof(long) - 1)) / sizeof(long) ]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

#ifndef _WIN32
static int dcfd = -1;
static struct termios oldtio;
#else
static HANDLE hCommPort = INVALID_HANDLE_VALUE;
#endif

static int debug = 0;

#ifdef _WIN32
static int serial_read(void *buffer, int count)
{
    BOOL fSuccess;

    fSuccess = ReadFile(hCommPort, buffer, count, (DWORD *)&count, NULL);
    if( !fSuccess )
        return -1;

    return count;
}

static int serial_write(void *buffer, int count)
{
    BOOL fSuccess;

    fSuccess = WriteFile(hCommPort, buffer, count, (DWORD *)&count, NULL);
    if( !fSuccess )
        return -1;

    return count;
}

static int serial_putc(char ch)
{
    BOOL fSuccess;
    int count = 1;

    fSuccess = WriteFile(hCommPort, &ch, count, (DWORD *)&count, NULL);
    if( !fSuccess )
        return -1;
    return count;
}
#else
static int serial_read(void *buffer, int count)
{
    return read(dcfd,buffer,count);
}

static int serial_write(void *buffer, int count)
{
    return write(dcfd,buffer,count);
}

static int serial_putc(char ch)
{
    return write(dcfd,&ch,1);
}
#endif

/* read count bytes from dc into buf */
static int blread(void *buf, int count)
{
    int retval;
    unsigned char *tmp = buf;

    while (count) {
        retval = serial_read(tmp, count);
        if (retval == -1) {
            printf("blread: read error!\n");
            return -1;
        } else {
            tmp += retval;
            count -= retval;
        }
    }

    return 0;
}

int serial_xprt_read_bytes(void *data, size_t len)
{
    return blread(data, len);
}

int serial_xprt_write_bytes(void *data, size_t len)
{
    return serial_write(data, len);
}

static char serial_getc()
{
    int retval;
    char tmp;

    retval = serial_read(&tmp, 1);
    if (retval == -1) {
        printf("serial_getc: read error!\n");
        tmp = 0x00;
    }

    return tmp;
}

/* send 4 bytes */
static int send_uint(unsigned int value)
{
    unsigned int tmp = value;

    /* send little-endian */
    serial_putc((char)(tmp & 0xFF));
    serial_putc((char)((tmp >> 0x08) & 0xFF));
    serial_putc((char)((tmp >> 0x10) & 0xFF));
    serial_putc((char)((tmp >> 0x18) & 0xFF));

    /* get little-endian */
    tmp =  ((unsigned int) (serial_getc() & 0xFF));
    tmp |= ((unsigned int) (serial_getc() & 0xFF) << 0x08);
    tmp |= ((unsigned int) (serial_getc() & 0xFF) << 0x10);
    tmp |= ((unsigned int) (serial_getc() & 0xFF) << 0x18);

    if (tmp != value)
        return 0;

    return 1;
}

int serial_xprt_write_uint(unsigned value)
{
    return send_uint(value);
}

/* receive 4 bytes */
static unsigned int recv_uint(void)
{
    unsigned int tmp;

    /* get little-endian */
    tmp =  ((unsigned int) (serial_getc() & 0xFF));
    tmp |= ((unsigned int) (serial_getc() & 0xFF) << 0x08);
    tmp |= ((unsigned int) (serial_getc() & 0xFF) << 0x10);
    tmp |= ((unsigned int) (serial_getc() & 0xFF) << 0x18);

    return (tmp);
}

int serial_xprt_read_uint(void)
{
    return recv_uint();
}

/* receive total bytes from dc and store in data */
static void recv_data(void *data, unsigned int total, unsigned int verbose)
{
    unsigned char type, sum, ok;
    unsigned int size, newsize;
    unsigned char *tmp;

    if (verbose) {
        printf("recv_data: ");
        fflush(stdout);
    }

    while (total) {
        blread(&type, 1);

        size = recv_uint();

        switch (type) {
        case 'U':		// uncompressed
            if (verbose) {
                printf("U");
                fflush(stdout);
            }
            blread(data, size);
            blread(&sum, 1);
            ok = 'G';
            serial_write(&ok, 1);
            total -= size;
            data += size;
            break;
        case 'C':		// compressed
            if (verbose) {
                printf("C");
                fflush(stdout);
            }
            tmp = malloc(size);
            blread(tmp, size);
            blread(&sum, 1);
            if (lzo1x_decompress(tmp, size, data, &newsize, 0) == LZO_E_OK) {
                ok = 'G';
                serial_write(&ok, 1);
                total -= newsize;
                data += newsize;
            } else {
                ok = 'B';
                serial_write(&ok, 1);
                printf("\nrecv_data: decompression failed!\n");
            }
            free(tmp);
            break;
        default:
            break;
        }
    }

    if (verbose) {
        printf("\n");
        fflush(stdout);
    }
}

int serial_xprt_read_chunk(void *data, size_t len)
{
    recv_data(data, len, debug);
    return 0;
}

/* send size bytes to dc from addr */
static void send_data(unsigned char * addr, unsigned int size, unsigned int verbose)
{
    unsigned int i;
    unsigned char *location = (unsigned char *) addr;
    unsigned char sum = 0;
    unsigned char data;
    unsigned int csize;
    unsigned int sendsize;
    unsigned char c;
    unsigned char * buffer;

    buffer = malloc(DCLOADBUFFER + DCLOADBUFFER / 64 + 16 + 3);

    if (verbose) {
        printf("send_data: ");
        fflush(stdout);
    }

    while (size) {
        if (size > DCLOADBUFFER)
            sendsize = DCLOADBUFFER;
        else
            sendsize = size;

        lzo1x_1_compress((unsigned char *)addr, sendsize, buffer, &csize, wrkmem);

        if (csize < sendsize) {
            // send compressed
            if (verbose) {
                printf("C");
                fflush(stdout);
            }
            c = 'C';
            serial_write(&c, 1);
            send_uint(csize);
            data = 'B';
            while(data != 'G') {
                location = buffer;
                serial_write(location, csize);
                sum = 0;
                for (i = 0; i < csize; i++) {
                    data = *(location++);
                    sum ^= data;
                }
                serial_write(&sum, 1);
                blread(&data, 1);
            }
        } else {
            // send uncompressed
            if (verbose) {
                printf("U");
                fflush(stdout);
            }
            c = 'U';
            serial_write(&c, 1);
            send_uint(sendsize);
            serial_write((unsigned char *)addr, sendsize);
            sum = 0;
            for (i = 0; i < sendsize; i++) {
                sum ^= ((unsigned char *)addr)[i];
            }
            serial_write(&sum, 1);
            blread(&data, 1);
        }

        size -= sendsize;
        addr += sendsize;
    }

    if (verbose) {
        printf("\n");
        fflush(stdout);
    }
}

int serial_xprt_write_chunk(void *data, size_t len)
{
    send_data(data, len, debug);
    return 0;
}

#ifdef _WIN32
/* XXX(jpeach) this is almost the same as log_error(), except that function
 * examines WSAGetLastError() rather than GetLastError().
 */
static void output_error(void)
{
    char *lpMsgBuf;

    FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    GetLastError(),
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
    (LPTSTR) &lpMsgBuf, 0, NULL // Process any inserts in lpMsgBuf.
    );
    printf("%s\n",(char *)lpMsgBuf);
    LocalFree( (LPVOID) lpMsgBuf );
}
#endif

/* setup serial port */
static int open_serial(const char *devicename, unsigned int speed, unsigned int *speedtest)
{
    *speedtest = speed;
#ifndef _WIN32
    struct termios newtio;
    speed_t speedsel;

    dcfd = open(devicename, O_RDWR | O_NOCTTY);
    if (dcfd < 0) {
        perror(devicename);
        exit(-1);
    }

    tcgetattr(dcfd, &oldtio);	// save current serial port settings
    memset(&newtio, 0, sizeof(newtio));	// clear struct for new port settings

    newtio.c_cflag = CRTSCTS | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;	// inter-character timer unused
    newtio.c_cc[VMIN] = 1;	// blocking read until 1 character arrives

    for (speedsel=0; !speedsel;) {
        switch(speed) {
#ifdef B1500000
        case 1500000:
            speedsel = B1500000;
            break;
#endif
#ifdef B500000
        case 500000:
            speedsel = B500000;
            break;
#endif
        case 115200:
            speedsel = B115200;
            break;
        case 57600:
            speedsel = B57600;
            break;
        case 38400:
            speedsel = B38400;
            break;
        case 19200:
            speedsel = B19200;
            break;
        case 9600:
            speedsel = B9600;
            break;
        default:
            printf("Unsupported baudrate (%d) - falling back to initial baudrate (%d)\n", speed, INITIAL_SPEED);
            *speedtest = speed = INITIAL_SPEED;
            break;
        }
    }

    cfsetispeed(&newtio, speedsel);
    cfsetospeed(&newtio, speedsel);

    // we don't error on these because it *may* still work
    if (tcflush(dcfd, TCIFLUSH) < 0) {
        perror("tcflush");
    }
    if (tcsetattr(dcfd, TCSANOW, &newtio) < 0) {
        perror("tcsetattr");
        printf("warning: your baud rate is likely set incorrectly\n");
    }

#ifdef __APPLE__
    if(speed > 115200) {
        /* Necessary to call ioctl to set non-standard speeds (aka higher than 115200) */
        if (ioctl(dcfd, IOSSIOSPEED, &speed) < 0) {
            perror("IOSSIOSPEED");
            printf("warning: your baud rate is likely set incorrectly\n");
        }
    }
#endif

#else
    BOOL fSuccess;
    COMMTIMEOUTS ctmoCommPort;
    DCB dcbCommPort;

    /* Setup the com port */
    hCommPort = CreateFile(devicename, GENERIC_READ | GENERIC_WRITE, 0,
               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if( hCommPort == INVALID_HANDLE_VALUE ) {
        printf( "*Error opening com port\n");
        output_error();
        return -1;
    }

    ctmoCommPort.ReadIntervalTimeout = MAXDWORD;
    ctmoCommPort.ReadTotalTimeoutMultiplier = MAXDWORD;
    ctmoCommPort.ReadTotalTimeoutConstant = MAXDWORD;
    ctmoCommPort.WriteTotalTimeoutMultiplier = 0;
    ctmoCommPort.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts( hCommPort, &ctmoCommPort );
    dcbCommPort.DCBlength = sizeof(DCB);

    fSuccess = GetCommState( hCommPort, &dcbCommPort );
    if( !fSuccess ) {
        printf( "*Error getting com port state\n" );
        output_error();
        return -1;
    }

    dcbCommPort.BaudRate = speed;
    dcbCommPort.ByteSize = DATA_BITS;
    dcbCommPort.Parity = PARITY_SET;
    dcbCommPort.StopBits = STOP_BITS;

    fSuccess = SetCommState( hCommPort, &dcbCommPort );
    if( !fSuccess ) {
        printf( "*Error setting com port state\n" );
        output_error();
        return -1;
    }
#endif
    return 0;
}

/* prepare for program exit */
static void finish_serial(void)
{
#ifdef _WIN32
    FlushFileBuffers(hCommPort);
#else
    tcflush(dcfd, TCIOFLUSH);
    tcsetattr(dcfd, TCSANOW, &oldtio);
#endif
}

/* close the host serial port */
static void close_serial(void)
{
#ifdef _WIN32
    FlushFileBuffers(hCommPort);
    CloseHandle(hCommPort);
    dcfd = INVALID_HANDLE_VALUE;
#else
    tcflush(dcfd, TCIOFLUSH);
    close(dcfd);
    dcfd = -1;
#endif
}

/* speedhack controls whether dcload will use N=12 (normal, 4.3% error) or
 * N=13 (alternate, -3.1% error) for 115200
 */
static int speedhack = 0;

/* use_extclk controls whether the DC's serial port will use an external clock */
static int use_extclk = 0;

static int change_speed(const char *device_name, unsigned int speed)
{
    unsigned char c;
    unsigned int dummy, rv = 0xdeadbeef;

    c = 'S';
    serial_write(&c, 1);
    blread(&c, 1);

    if (speedhack && (speed == 115200))
        send_uint(111600); /* get dcload to pick N=13 rather than N=12 */
    else if (use_extclk)
        send_uint(0);
    else
        send_uint(speed);

    printf("Changing speed to %d bps... ", speed);
    close_serial();

    if (open_serial(device_name, speed, &dummy)<0)
        return 1;

    send_uint(rv);
    rv = recv_uint();
    printf("done\n");

    return 0;
}

int serial_xprt_send_data(void *data, size_t len, unsigned dcaddr)
{
    char c = 'B';
    serial_write(&c, 1);
    blread(&c, 1);

    if (send_uint(dcaddr) == 0) {
        return -1;
    }

    if (send_uint(len) == 0) {
        return -1;
    }

    send_data(data, len, 1 /* verbose */);
    return 0;
}

int serial_xprt_recv_data(unsigned dcaddr, size_t len, void *dst)
{
    unsigned char c;
    const unsigned int wrkmem = 0x8cff0000;

    serial_write("F", 1);
    serial_read(&c, 1);
    send_uint(dcaddr);
    send_uint(len);
    send_uint(wrkmem);
    recv_data(dst, len, 1);

    return 0;
}

int serial_xprt_recv_data_quiet(unsigned dcaddr, size_t len, void *dst)
{
    unsigned char c;
    const unsigned int wrkmem = 0x8cff0000;

    serial_write("G", 1);
    serial_read(&c, 1);
    send_uint(dcaddr);
    send_uint(len);
    send_uint(wrkmem);
    recv_data(dst, len, 1);

    return 0;
}

int serial_xprt_initialize(const char *device, unsigned speed, unsigned flags)
{
    unsigned int dummy = DEFAULT_SPEED;

    speedhack = (flags & SERIAL_XPRT_FLAG_SPEEDHACK) ? 1 : 0;
    use_extclk = (flags & SERIAL_XPRT_FLAG_EXTCLOCK) ? 1 : 0;
    debug = (flags & SERIAL_XPRT_FLAG_DEBUG) ? 1 : 0;

    if (speedhack)
        printf("Alternate 115200 enabled\n");

    if (use_extclk)
        printf("External clock usage enabled\n");

    /* Test for a reasonable baud - this is for POSIX systems */
    if (speed != INITIAL_SPEED) {
        if (open_serial(device, speed, &speed) < 0) {
            return -1;
        }

        close_serial();
    }

    if (open_serial(device, INITIAL_SPEED, &dummy) < 0) {
        return -1;
    }

    if (speed != INITIAL_SPEED) {
        change_speed(device, speed);
    }

    return 0;
}

void serial_xprt_cleanup(void)
{
    finish_serial();
    close_serial();
}

int serial_xprt_dispatch_commands(int isofd)
{
    unsigned char command;

    serial_xprt_read_bytes(&command, 1);

    switch (command) {
        case 0:
            return 1;
        case 1:
            serial_xprt_system_calls.fstat(NULL);
            break;
        case 2:
            serial_xprt_system_calls.write(NULL);
            break;
        case 3:
            serial_xprt_system_calls.read(NULL);
            break;
        case 4:
            serial_xprt_system_calls.open(NULL);
            break;
        case 5:
            serial_xprt_system_calls.close(NULL);
            break;
        case 6:
            serial_xprt_system_calls.create(NULL);
            break;
        case 7:
            serial_xprt_system_calls.link(NULL);
            break;
        case 8:
            serial_xprt_system_calls.unlink(NULL);
            break;
        case 9:
            serial_xprt_system_calls.chdir(NULL);
            break;
        case 10:
            serial_xprt_system_calls.chmod(NULL);
            break;
        case 11:
            serial_xprt_system_calls.lseek(NULL);
            break;
        case 12:
            serial_xprt_system_calls.time(NULL);
            break;
        case 13:
            serial_xprt_system_calls.stat(NULL);
            break;
        case 14:
            serial_xprt_system_calls.utime(NULL);
            break;
        case 15:
            printf("command 15 should not happen... (but it did)\n");
            break;
        case 16:
            serial_xprt_system_calls.opendir(NULL);
            break;
        case 17:
            serial_xprt_system_calls.closedir(NULL);
            break;
        case 18:
            serial_xprt_system_calls.readdir(NULL);
            break;
        case 19:
            serial_xprt_system_calls.cdfs_redir_read_sectors(isofd, NULL);
            break;
        case 20:
            serial_xprt_system_calls.gdbpacket(NULL);
            break;
        case 21:
            serial_xprt_system_calls.rewinddir(NULL);
            break;
        default:
            printf("Unimplemented command (%d) \n", command);
            printf("Assuming program has exited, or something...\n");
            return -1;
    }

    return 0;
}
