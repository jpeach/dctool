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

#include "commands.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef WITH_BFD
#include <bfd.h>
#else
#include <libelf.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

int download(const char *filename, unsigned int address, unsigned int size, xprt_recv_data_t recv)
{
    int outputfd;

    unsigned char *data;
    struct timeval starttime, endtime;
    double stime, etime;

    outputfd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);

    if (outputfd < 0) {
        log_error(filename);
        return -1;
    }

    data = malloc(size);

    gettimeofday(&starttime, 0);

    recv(address, size, data);
    /* TODO: propagate and handle transport errors */

    gettimeofday(&endtime, 0);

    printf("Received %d bytes\n", size);

    stime = starttime.tv_sec + starttime.tv_usec / 1000000.0;
    etime = endtime.tv_sec + endtime.tv_usec / 1000000.0;

    printf("transferred at %f bytes / sec\n", (double) size / (etime - stime));
    fflush(stdout);

    write(outputfd, data, size);

    close(outputfd);
    free(data);

    return 0;
}

unsigned int upload(const char *filename, unsigned int address, xprt_send_data_t send)
{
    int inputfd;
    int size = 0;
    int sectsize;
    unsigned char *inbuf;
    struct timeval starttime, endtime;

    double stime, etime;
#ifdef WITH_BFD
    bfd *somebfd;
#else
    Elf *elf;
    Elf32_Ehdr *ehdr;
    Elf32_Shdr *shdr;
    Elf_Scn *section = NULL;
    Elf_Data *data;
    char *section_name;
    size_t index;
#endif

#ifdef WITH_BFD
    if ((somebfd = bfd_openr(filename, 0))) {
        if (bfd_check_format(somebfd, bfd_object)) {
            /* try bfd first */
            asection *section;

            printf("File format is %s, ", somebfd->xvec->name);
            address = somebfd->start_address;
            size = 0;
            printf("start address is 0x%x\n", address);

            gettimeofday(&starttime, 0);

            for (section = somebfd->sections; section != NULL; section = section->next) {
                if ((section->flags & SEC_HAS_CONTENTS) && (section->flags & SEC_LOAD)) {
                    sectsize = bfd_section_size(somebfd, section);
                    printf("Section %s, ",section->name);
                    printf("lma 0x%x, ", (unsigned int)section->lma);
                    printf("size %d\n",sectsize);

                    if (sectsize) {
                        size += sectsize;
                        inbuf = malloc(sectsize);
                        bfd_get_section_contents(somebfd, section, inbuf, 0, sectsize);

                        if (send(inbuf, sectsize, section->lma, sectsize) == -1) {
                            free(inbuf);
                            bfd_close(somebfd);
                            return -1;
                        }

                        free(inbuf);
                    }
                }
            }

            bfd_close(somebfd);
            goto done_transfer;
        }

        bfd_close(somebfd);
    }
#else /* !WITH_BFD -- use libelf */
    if(elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(stderr, "libelf initialization error: %s\n", elf_errmsg(-1));
        return -1;
    }

    if((inputfd = open(filename, O_RDONLY | O_BINARY)) < 0) {
        log_error(filename);
        return -1;
    }

    if(!(elf = elf_begin(inputfd, ELF_C_READ, NULL))) {
        fprintf(stderr, "Cannot read ELF file: %s\n", elf_errmsg(-1));
        return -1;
    }

    switch (elf_kind(elf)) {
    case ELF_K_ELF:
        if(!(ehdr = elf32_getehdr(elf))) {
            fprintf(stderr, "Unable to read ELF header: %s\n", elf_errmsg(-1));
            return -1;
        }

        address = ehdr->e_entry;
        printf("File format is ELF, start address is 0x%x\n", address);

        /* Retrieve the index of the ELF section containing the string table of
           section names */
        if(elf_getshdrstrndx(elf, &index)) {
            fprintf(stderr, "Unable to read section index: %s\n", elf_errmsg(-1));
            return -1;
        }

        gettimeofday(&starttime, 0);
        while((section = elf_nextscn(elf, section))) {
            if(!(shdr = elf32_getshdr(section))) {
                fprintf(stderr, "Unable to read section header: %s\n", elf_errmsg(-1));
                return -1;
            }

            if(!(section_name = elf_strptr(elf, index, shdr->sh_name))) {
                fprintf(stderr, "Unable to read section name: %s\n", elf_errmsg(-1));
                return -1;
            }

            if(!shdr->sh_addr)
                continue;

            /* Check if there's some data to upload. */
            data = elf_getdata(section, NULL);
            if(!data->d_buf || !data->d_size)
                continue;

            printf("Section %s, lma 0x%08x, size %d\n", section_name,
                   shdr->sh_addr, shdr->sh_size);
            size += shdr->sh_size;

            do {
                if (send(data->d_buf, data->d_size, shdr->sh_addr + data->d_off) == -1) {
                    return -1;
                }
            } while((data = elf_getdata(section, data)));
        }

        elf_end(elf);
        close(inputfd);
        goto done_transfer;
    default:
        elf_end(elf);
        close(inputfd);
    }
#endif /* WITH_BFD */
    /* if all else fails, send raw bin */
    inputfd = open(filename, O_RDONLY | O_BINARY);

    if (inputfd < 0) {
        log_error(filename);
        return -1;
    }

    printf("File format is raw binary, start address is 0x%x\n", address);

    size = lseek(inputfd, 0, SEEK_END);
    lseek(inputfd, 0, SEEK_SET);

    inbuf = malloc(size);
    read(inputfd, inbuf, size);
    close(inputfd);

    gettimeofday(&starttime, 0);

    if (send(inbuf, size, address) == -1) {
        return -1;
    }

done_transfer:
    gettimeofday(&endtime, 0);

    stime = starttime.tv_sec + starttime.tv_usec / 1000000.0;
    etime = endtime.tv_sec + endtime.tv_usec / 1000000.0;

    printf("transferred %d bytes at %f bytes / sec\n", size, (double) size / (etime - stime));
    printf("%.2f seconds to transfer %d bytes\n", (etime - stime), size);
    fflush(stdout);

    return address;
}
