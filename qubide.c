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

int read_disk(struct qdisk *disk, int position, int bytes, uint8_t *buffer)
{
	int err;

	fseek(disk->image, position, SEEK_SET);
	err = fread(buffer, 1, bytes, disk->image);
	if (err != bytes) {
		fprintf(stderr, "ERROR: read %d, expected %d\n", err, bytes);
		return -1;
	}

	return 0;
}

int read_block(struct qdisk *disk, int block, uint8_t *buffer)
{
	int err;

	err = read_disk(disk, disk->blocksize * block, disk->blocksize, buffer);

	return err;
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
	int block_lim, dir_size = 0;
	int block_num, blockidx = 0, dir_idx = 0;
	DIR_ENTRY *dir_entry = NULL;
	uint8_t *block;
	int i;

	block = malloc(disk->blocksize);

	if (!flag) {
		printf("%.10s\n", disk->header->med_name);
		printf ("%i/%i blocks.\n\n", disk->free_blocks,
				disk->total_blocks);
	}

	while (blockidx < file->no_blocks) {

		if (!(dir_idx % disk->blocksize)) {
			block_num = file->blocks[blockidx];
			read_block(disk, block_num, block);
			dir_entry = (DIR_ENTRY *)block;
		}

		if (blockidx == 0) {
			dir_size = swaplong(dir_entry->file_len);
			dir_idx = DIR_ENTRY_SIZE;
		}

		block_lim = disk->blocksize * (blockidx + 1);

		for(i = dir_idx; (i < dir_size) && (i < block_lim) ;
				i += DIR_ENTRY_SIZE) {
			dir_entry = (DIR_ENTRY *)(block + (i % disk->blocksize));

			print_entry(dir_entry, 0, flag);

			dir_idx += DIR_ENTRY_SIZE;
		}

		blockidx++;
	}

	free(block);
}

int search_directory(struct qdisk *disk, char *name, struct qfile *dir,
		struct qfile *file)
{
	int dir_size;
	int fileno = -1;
	int block_num;
	DIR_ENTRY *dir_entry;
	uint8_t *block;
	int i, j;

	block = malloc(disk->blocksize);

	for (j = 0; j < dir->no_blocks; j++) {
		block_num = dir->blocks[j];

		read_block(disk, block_num, block);

		dir_entry = (DIR_ENTRY *)block;
		dir_size = swaplong(dir_entry->file_len);

		for(i = DIR_ENTRY_SIZE; i < dir_size ; i += DIR_ENTRY_SIZE) {
			dir_entry = (DIR_ENTRY *)(block + i);
			
			if (strncmp(name, dir_entry->filename,
						dir_entry->fn_len) == 0)
			{
				fileno = swapword(dir_entry->fileno);
				memcpy(&file->directory, dir_entry,
						sizeof(*dir_entry));
				goto out;
			}
		}
	}

out:
	free(block);
	return fileno;
}

void dump_file(struct qdisk *disk, struct qfile *file)
{
	int file_len;
	int blockno = 0, block_num;
	uint8_t *block;

	file_len = swaplong(file->directory.file_len);

	block = malloc(disk->blocksize);

	while(file_len > disk->blocksize ) {
		block_num = file->blocks[blockno];

		read_block(disk, block_num, block);

		if (blockno == 0) {
			fwrite(block + DIR_ENTRY_SIZE,
					disk->blocksize - DIR_ENTRY_SIZE, 1, 
					stdout);
		} else {
			fwrite(block, disk->blocksize, 1, stdout);
		}

		file_len -= disk->blocksize;
		blockno++;
	}

	if (file_len) {
		block_num = file->blocks[blockno];

		read_block(disk, block_num, block);

		if (blockno == 0) {
			fwrite(block + DIR_ENTRY_SIZE,
					file_len - DIR_ENTRY_SIZE, 1, stdout);
		} else {
			fwrite(block, file_len, 1, stdout);
		}
	}
}

void list_directory_cmd(struct qdisk *disk, int flag)
{
	int blks;
	struct qfile *file;

	file = calloc(1, sizeof(*file));

	blks = find_file(disk, 0, file);

	if (!blks) {
		printf("Failed to find root directory\n");
		free(file);
		return;
	}

	list_directory(disk, flag, file);

	free(file);
}

void dump_file_cmd(struct qdisk *disk, char *name)
{
	int dir_blks, file_blks, fileno;
	struct qfile *dir, *file;

	dir = calloc(1, sizeof(*dir));
	file = calloc(1, sizeof(*file));

	dir_blks = find_file(disk, 0, dir);

	if (!dir_blks) {
		printf("Failed to find root directory\n");
		free(dir);
		return;
	}

	fileno = search_directory(disk, name, dir, file);
	if (fileno < 0) {
		printf("Failed to find file\n");
		goto out;
	}

	file_blks = find_file(disk, fileno, file);
	if (!file_blks) {
		printf("Failed to find file\n");
		goto out;
	}

	dump_file(disk, file);
out:
	free(dir);
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

const char *options = "b:dsin:";

void usage(void)
{
	printf("qubide -b <imagefile> [-d|-s]\n");
	printf("  -b <imagefile>             the image to operate on\n");
	printf("  -d                         long directory listing\n");
	printf("  -s                         short directory listing\n");
	printf("  -i                         show device info\n");
	printf("  -n                         dump file to stdout\n");
}

int main(int argc, char **argv)
{
	struct qopts *qopts;
	struct qdisk *disk;
	int opt, err = 0;
	
	if (sizeof(DIR_ENTRY) != 64) {
		printf("Packing error DIR_ENTRY is %d bytes\n",
				(int)sizeof(DIR_ENTRY));
		return -1;
	}

	if (sizeof(DISK_HEADER) != 52) {
		printf("Packing error DISK_HEADER is %d bytes\n",
				(int)sizeof(DISK_HEADER));
		return -1;
	}

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
		case 'n':
			qopts->command = QUB_CMD_DUMP;
			qopts->cmd_arg = optarg;
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
	read_disk(disk, 0, Q_SSIZE, disk->header_buffer);

	disk->header = (DISK_HEADER *)disk->header_buffer;

	disk->total_blocks = swapword(disk->header->tot_blks);
	disk->free_blocks = swapword(disk->header->fre_blks);
	disk->sec_per_block = swapword(disk->header->sec_blk);
	disk->num_map_blocks = swapword(disk->header->part_1_q);

	disk->blocksize = disk->sec_per_block * Q_SSIZE;

	/* Allocate and read the block map */
	disk->map = malloc(disk->blocksize * disk->num_map_blocks);

	read_disk(disk, 0, disk->blocksize * disk->num_map_blocks, disk->map);

	timeadjust = GetTimeZone ();

	switch(qopts->command) {
	case QUB_CMD_LONGDIR:
		list_directory_cmd(disk, 0);
		break;
	case QUB_CMD_SHORTDIR:
		list_directory_cmd(disk, 1);
		break;
	case QUB_CMD_DUMP:
		dump_file_cmd(disk, qopts->cmd_arg);
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
