/*
 * QUBIDE/QL-SD image tools
 *
 * Copyright (c) 2014 Graeme Gregory
 *
 * Based in part on qltools which is
 *
 *       Copyright (c)1992 by Giuseppe Zanetti
 *
 *       Giuseppe Zanetti
 *       via Vergani, 11 - 35031 Abano Terme (Padova) ITALY
 *       e-mail: beppe@sabrina.dei.unipd.it
 *
 *       This is copyrighted software, but freely distributable.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "qubide.h"

static time_t timeadjust;

uint16_t swapword(uint16_t val)
{
    return (uint16_t) (val << 8) + (val >> 8);
}

uint32_t swaplong (uint32_t val)
{
    return (uint32_t) (((uint32_t) swapword (val & 0xFFFF) << 16) |
                    (uint32_t) swapword (val >> 16));
}

int file_num(struct qdisk *disk, int block)
{
	uint16_t fn;

	fn = *(uint16_t *)(disk->map + 0x100 + (block * 4));

	return swapword(fn);	
}

int file_block(struct qdisk *disk, int block)
{
	uint16_t bl;

	bl = *(uint16_t *)(disk->map + 0x102 + (block * 4));

	return swapword(bl);
}

time_t GetTimeZone(void)
{
	struct timeval tv;
	struct timezone tz;

	gettimeofday (&tv, &tz);
	return  -60 * tz.tz_minuteswest;
}

int find_file(struct qdisk *disk, int file_no, struct qfile *file)
{
	int i, fn, bl;

	/*
	 * Go through the blocklist and build an array of blocks belonging
	 * to the file searched for.
	 */
	for (i = 0; i < disk->total_blocks; i++) {
		bl = file_block(disk, i);
		fn = file_num(disk, i);
		
		if (file_no == fn) {
			file->blocks[bl] = i;
			/*
			 * keep track of the maximum block number we find
			 * in theory there should be no holes in the file
			 */
			if (file->no_blocks <= bl)
				file->no_blocks = bl + 1;
		}
	}

	return file->no_blocks;
}

int print_entry (DIR_ENTRY *entry, int fnum, int flag)
{
	short j,k;
	int32_t flen;

	if (entry == NULL)
		return 0;

	flen = swaplong (entry->file_len);

	if (flen + swapword (entry->fn_len) == 0)
		return 0;

	j = k = swapword (entry->fn_len);
	if (flag == 0)
	{
		k = 36;
	}
	printf ("%-*.*s", k, j, entry->filename);

	if (entry->file_type == 255)
	{
		if (flag)
		{
			putc ('\n', stdout);
		}
		else
		{
			if (flag == 0)
			{
				printf ("(dir) %ld\n", flen - 64l);
			}
		}
		if(flag != 2)
		{
			//RecurseDir (fnum, flen, flag, print_entry);
		}

	}
	else if (flag == 0)
	{
		switch (entry->file_type)
		{
			case 0:
				fputs (" ", stdout);
				break;
			case 1:
				fputs ("E", stdout);
				break;
			case 2:
				fputs ("r", stdout);
				break;
			default:
				printf ("%3d", entry->file_type);
				break;
		}
		printf (" %7d", (int32_t) (flen - 64L));
		{
			struct tm *tm;
			time_t t = swaplong (entry->date_update) - TIME_DIFF
								- timeadjust;

			tm = localtime (&t);
			printf (" %02d/%02d/%04d %02d:%02d:%02d v%-5u",
					tm->tm_mday, tm->tm_mon + 1, tm->tm_year+1900,
					tm->tm_hour, tm->tm_min, tm->tm_sec,
					swapword (entry->version));
		}
		if (entry->file_type == 1 && entry->data_space)
		{
			printf (" (%d)", swaplong (entry->data_space));
		}
		putc ('\n', stdout);
	}
	else
	{
		putc ('\n', stdout);
	}
	return 0;
}

void list_directory(struct qdisk *disk, int flag, struct qfile *file)
{
	int dir_size, file_len, file_mode;
	int name_len, file_type, file_num;
	int data_space, block_num;
	DIR_ENTRY *dir_entry;
	uint8_t *block;
	char filename[37];
	int i, j;

	block = malloc(disk->sec_per_block * Q_SSIZE);

	if (!flag) {
		printf("%.10s\n", disk->header->med_name);
		printf ("%i/%i blocks.\n\n", disk->free_blocks,
				disk->total_blocks);
	}
	for (j = 0; j < file->no_blocks; j++) {
		block_num = file->blocks[j];

		fseek(disk->image, 0, disk->sec_per_block * Q_SSIZE
				* block_num);
		fread(block, disk->sec_per_block, Q_SSIZE, disk->image);

		dir_entry = (DIR_ENTRY *)block;
		dir_size = swaplong(dir_entry->file_len);

		for(i = DIR_ENTRY_SIZE; i < dir_size ; i += DIR_ENTRY_SIZE) {
			dir_entry = (DIR_ENTRY *)(block + i);

			print_entry(dir_entry, 0, flag);
		}
	}

	free(block);
}

void list_directory_cmd(struct qdisk *disk, int flag)
{
	int blks;
	struct qfile *file;

	file = calloc(1, sizeof(*file));

	blks = find_file(disk, 0, file);

	if (!blks) {
		printf("Failed to find root directory in this file\n");
		return;
	}

	list_directory(disk, flag, file);

	free(file);
}

void device_info(void)
{
	/*
	snprintf(diskname, 11, "%s", header->med_name);
	printf("Medium Name %s\n", diskname);
	printf("Rand Number %x\n", swapword(header->rand_num));
	printf("Number Updates %x\n", swaplong(header->n_updts));
	printf("Free Blocks %x\n", swapword(header->fre_blks));
	printf("Good Blocks %x\n", swapword(header->gd_blks));
	printf("Total Blocks %x\n", swapword(header->tot_blks));

	printf("----------------\n");

	printf("Sectors Per Track %x\n", swapword(header->sec_trk));
	printf("Sectors Per Cylinder %x\n", swapword(header->sec_cyl));
	printf("Number of Tracks %x\n", swapword(header->n_tracks));
	printf("Sectors Per Block %x\n", swapword(header->sec_blk));
	printf("Fat Size %x\n", swapword(header->fat_size));
	printf("Fat Type %x\n", header->fat_type);

	printf("----------------\n");

	printf("Number of Headers %x\n", header->num_heads);
	printf("Number of Partitions %x\n", header->num_parts);

	printf("----------------\n");

	printf("Part1: Number of Mapping blocks %x\n",
			swapword(header->part_1_q));
	printf("Part1: Start Track of Partition %x\n",
			swapword(header->part_1_t));
		*/
}

const char *options = "b:dsi";

void usage(void)
{
	printf("qubide -b <imagefile> [-d|-s]\n");
	printf("  -b <imagefile>             the image to operate on\n");
	printf("  -d                         long directory listing\n");
	printf("  -s                         short directory listing\n");
	printf("  -i                         show device info\n");
}

int main(int argc, char **argv)
{
	struct qopts *qopts;
	struct qdisk *disk;
	int opt, err;
	
	qopts = calloc(1, sizeof(*qopts));
	if (!qopts) {
		printf("Failed to allocate options\n");
		return -ENOMEM;
	}

	disk = calloc(1, sizeof(*disk));
	if (!disk) {
		printf("Failed to allocate disk\n");
		err = -ENOMEM;
		goto error1;
	}

	while((opt = getopt(argc, argv, options)) != EOF) {

		if (opt == 'b') {
			if (qopts->image) {
				printf("Only one image please\n");
				usage();
				err = -EINVAL;
				goto error1;
			}
			qopts->image = 1;
			qopts->image_name = optarg;
			continue;
		}

		if (qopts->command) {
			printf("Only one command please\n");
			usage();
			err = -EINVAL;
			goto error1;
		}

		switch (opt) {
		case 'd':
			qopts->command = QUB_CMD_LONGDIR;
			break;
		case 's':
			qopts->command = QUB_CMD_SHORTDIR;
			break;
		case 'i':
			qopts->command = QUB_CMD_INFO;
			break;
		default:
			usage();
			return -EINVAL;
		}
	}	

	if (!qopts->image) {
		usage();
		return -EINVAL;
	}

	disk->image = fopen(qopts->image_name, "rw");
	if (!disk->image) {
		perror("Failed to open file");
		err = errno;
		goto error2;
	}

	/* Read the info block so we can store some essential parameters */
	fread(disk->header_buffer, Q_SSIZE, 1, disk->image);
	disk->header = (DISK_HEADER *)disk->header_buffer;

	disk->total_blocks = swapword(disk->header->tot_blks);
	disk->free_blocks = swapword(disk->header->fre_blks);
	disk->sec_per_block = swapword(disk->header->sec_blk);
	disk->num_map_blocks = swapword(disk->header->part_1_q);

	/* Allocate and read the block map */
	disk->map = malloc(disk->sec_per_block * disk->num_map_blocks * Q_SSIZE);

	fseek(disk->image, 0, 0);
	fread(disk->map, disk->sec_per_block * disk->num_map_blocks, Q_SSIZE,
			disk->image);

	timeadjust = GetTimeZone ();

	switch(qopts->command) {
	case QUB_CMD_LONGDIR:
		list_directory_cmd(disk, 0);
		break;
	case QUB_CMD_SHORTDIR:
		list_directory_cmd(disk, 1);
		break;
	};

	/* clean up ready to leave */
	free(disk->map);
	fclose(disk->image);
error2:
	free(disk);
error1:
	free(qopts);
	return err;
}
