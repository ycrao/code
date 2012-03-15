#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "ubi-media.h"

#define BLOCKSIZE 128*1024

#define be64_to_cpu(x) __swab64((x))

/*
    - flashing to file: args: input file, num_blocks, output_file
    - check input file for correctness
    - also check types (dynamic not static, blocks in order? etc)
    - check of aantal blocks genoeg is (reserveer 4 voor VOLUME_ID)
    - malloc totale grootte (output_map)
    - schrijf elke blok uit volume 0 naar zelfde blok in output file
    - creeer 2 nieuwe volumeID blocken met nieue reserved_pebs en crc
    - vul resterende blokken op met default block (hardcoded values including crc)
        en 0xff als data fill
    - open output file (deleting existing one
    - write map to file
    - close output file
*/

enum Mode { MODE_SHOW=0, MODE_FLASH };
enum Mode mode = MODE_SHOW;

int main(int argc, const char *argv[]) {
    const char* input_file = NULL;
    const char* output_file = 0;
    int num_blocks = 0;

    switch (argc) {
    case 2:
        mode = MODE_SHOW;
        input_file = argv[1];
        break;
    case 4:
        mode = MODE_FLASH;
        input_file = argv[1];
        num_blocks = atoi(argv[2]);
        output_file = argv[3];
        break;
    default:
        printf("Usage %s [input_file] <numblocks> <output_file>\n", argv[0]);
        return -1;
    }

    int fd = open(input_file, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    struct stat statbuf;
    if (fstat(fd, &statbuf) != 0) {
        perror("fstat");
        return -1;
    }

    int size = statbuf.st_size;
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE;
    void* input_map = mmap(NULL, size, prot, flags, fd, 0);
    if (input_map == (void*)-1) {
        perror("mmap");
        return -1;
    }
    close(fd);


    unsigned char* base = input_map;
    int offset = 0;
    int index = 0;
    int vid_count = 0;
    int vtbl_count = 0;
    while (offset < size) {
        struct ubi_ec_hdr* hdr = (struct ubi_ec_hdr*)(base + offset);
        if (ntohl(hdr->magic) != UBI_EC_HDR_MAGIC) {
            printf("[%03d] [%08d] invalid magic\n", index, offset);
            break;
        }
        unsigned long ec = be64_to_cpu(hdr->ec);
        unsigned int vid = ntohl(hdr->vid_hdr_offset);
        unsigned int data = ntohl(hdr->data_offset);
        unsigned int crc = ntohl(hdr->hdr_crc);
        printf("[%03d] [%08x]  vid=%u  daa=%u  crc=0x%08x  ec=%ld\n", index, offset, vid, data, crc, ec);

        struct ubi_vid_hdr* vid_hdr = (struct ubi_vid_hdr*)&hdr[1];
        if (ntohl(vid_hdr->magic) == UBI_VID_HDR_MAGIC) {
            unsigned int vol_id = ntohl(vid_hdr->vol_id);
            unsigned int lnum = ntohl(vid_hdr->lnum);
            unsigned int datasize = ntohl(vid_hdr->data_size);
            unsigned int used_ebs = ntohl(vid_hdr->used_ebs);
            unsigned int data_pad = ntohl(vid_hdr->data_pad);
            unsigned int data_crc = ntohl(vid_hdr->data_crc);
            unsigned int sqnum = ntohl(vid_hdr->sqnum);
            unsigned int hdr_crc2 = ntohl(vid_hdr->hdr_crc);
            printf("  VID_HDR:  type=%d  copy=%d  vol_id=0x%08x  lnum=%d  datasize=%d\n",
                vid_hdr->vol_type, vid_hdr->copy_flag, vol_id, lnum, datasize);
            printf("  used_ebs=%d  data_pad=%d  crc=0x%08x  sqnum=%d  hdr_crc=0x%08x\n",
                used_ebs, data_pad, data_crc, sqnum, hdr_crc2);
            vid_count++;

            if (vol_id == UBI_LAYOUT_VOLUME_ID) {
                struct ubi_vtbl_record* vtbl = (struct ubi_vtbl_record*)&vid_hdr[1];
                unsigned int res_pebs = ntohl(vtbl->reserved_pebs);
                unsigned int alignment = ntohl(vtbl->alignment);
                unsigned int data_pad = ntohl(vtbl->data_pad);
                unsigned int name_len = ntohs(vtbl->name_len);
                unsigned int crc = ntohl(vtbl->crc);
                printf("    VOLUME_ID: res_peds=%d  align=%d  data_pad=%d  type=%d  marker=%d\n",
                    res_pebs, alignment, data_pad, vtbl->vol_type, vtbl->upd_marker);
                printf("    len=%d  name=%s  flags=%d  crc=0x%08x\n", name_len, vtbl->name, vtbl->flags, crc);
                vtbl_count++;
            }
        }

        offset += BLOCKSIZE;
        index++;
    }
    printf("%d PEBs,  %d VIDs  %d VTBLs\n", index, vid_count, vtbl_count);
    printf("SIZEOF ec_hdr=%d  vid_hdr=%d  vtbl=%d\n",
        sizeof(struct ubi_ec_hdr), sizeof(struct ubi_vid_hdr), sizeof(struct ubi_vtbl_record));

    munmap(input_map, size);
    return 0;
}

