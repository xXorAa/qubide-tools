/*
 * Copyright (c) 2014 Graeme Gregory
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

uint16_t swapword(uint16_t val)
{
    return (uint16_t) (val << 8) + (val >> 8);
}

uint32_t swaplong (uint32_t val)
{
    return (uint32_t) (((uint32_t) swapword (val & 0xFFFF) << 16) |
                    (uint32_t) swapword (val >> 16));
}

static uint8_t *g_header_buffer[Q_SSIZE];
static struct DISK_HEADER *g_header=(struct DISK_HEADER *)g_header_buffer;

static int g_num_map_blocks;
static int g_sec_per_block;
static int g_total_blocks;
static int g_free_blocks;

static uint8_t *g_map;

static FILE *image;

static uint8_t g_cur_file[1024];
static uint8_t g_cur_file_blocks;

static time_t timeadjust;

int file_num(int block)
{
	uint16_t fn;

	fn = *(uint16_t *)(g_map + 0x100 + (block * 4));

	return swapword(fn);	
}

int file_block(int block)
{
	uint16_t bl;

	bl = *(uint16_t *)(g_map + 0x102 + (block * 4));

	return swapword(bl);
}

time_t GetTimeZone(void)
{
	struct timeval tv;
	struct timezone tz;

	gettimeofday (&tv, &tz);
	return  -60 * tz.tz_minuteswest;
}

int find_file(int file_no)
{
	int i, fn, bl;
	int cur_file_block = 0;

	/*
	 * Go through the blocklist and build an array of blocks belonging
	 * to the file searched for.
	 */
	for (i = 0; i < g_total_blocks; i++) {
		bl = file_block(i);
		fn = file_num(i);
		
		if (file_no == fn) {
			g_cur_file[bl] = i;
			/*
			 * keep track of the maximum block number we find
			 * in theory there should be no holes in the file
			 */
			if (cur_file_block <= bl)
				cur_file_block = bl + 1;
		}
	}

	g_cur_file_blocks = cur_file_block;

	return cur_file_block;
}

int print_entry (struct DIR_ENTRY *entry, int fnum, int flag)
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

void list_directory(int flag)
{
	int dir_size, file_len, file_mode;
	int name_len, file_type, file_num;
	int data_space, block_num;
	struct DIR_ENTRY *dir_entry;
	uint8_t *block;
	char filename[37];
	int i, j;

	block = malloc(g_sec_per_block * Q_SSIZE);

	if (!flag) {
		printf("%.10s\n", g_header->med_name);
		printf ("%i/%i blocks.\n\n", g_free_blocks, g_total_blocks);
	}
	for (j = 0; j < g_cur_file_blocks; j++) {
		block_num = g_cur_file[j];

		fread(block, g_sec_per_block, Q_SSIZE, image);

		dir_entry = (struct DIR_ENTRY *)block;
		dir_size = swaplong(dir_entry->file_len);

		for(i = DIR_ENTRY_SIZE; i < dir_size ; i += DIR_ENTRY_SIZE) {
			dir_entry = (struct DIR_ENTRY *)(block + i);

			print_entry(dir_entry, 0, flag);
		}
	}

	free(block);
}

void list_directory_cmd(flag)
{
	int blks;

	blks = find_file(0);

	if (!blks) {
		printf("Failed to find root directory in this file\n");
		return;
	}

	list_directory(flag);
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
	int opt, err;

	qopts = calloc(1, sizeof(*qopts));
	if (!qopts) {
		printf("Failed to allocate options\n");
		return -ENOMEM;
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

	image = fopen(qopts->image_name, "rw");
	if (!image) {
		perror("Failed to open file");
		err = errno;
		goto error1;
	}

	/* Read the info block so we can store some essential parameters */
	fread(g_header_buffer, Q_SSIZE, 1, image);

	g_total_blocks = swapword(g_header->tot_blks);
	g_free_blocks = swapword(g_header->fre_blks);
	g_sec_per_block = swapword(g_header->sec_blk);
	g_num_map_blocks = swapword(g_header->part_1_q);

	/* Allocate and read the block map */
	g_map = malloc(g_sec_per_block * g_num_map_blocks * Q_SSIZE);

	fseek(image, 0, 0);
	fread(g_map, g_sec_per_block * g_num_map_blocks, Q_SSIZE, image);

	timeadjust = GetTimeZone ();

	switch(qopts->command) {
	case QUB_CMD_LONGDIR:
		list_directory_cmd(0);
		break;
	case QUB_CMD_SHORTDIR:
		list_directory_cmd(1);
		break;
	};

	/* clean up ready to leave */
	free(g_map);
	fclose(image);
error1:
	free(qopts);
	return err;
}
