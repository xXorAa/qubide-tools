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

#ifndef _qubide_h
#define _qubide_h

#include <stdint.h>

#if defined __GNUC__
#define PACKED  __attribute__ ((packed))
#else
#define PACKED
#endif

struct DISK_HEADER {
	uint32_t	id;
	char		med_name[10];
	uint16_t	rand_num;
	uint32_t	n_updts;
	uint16_t	fre_blks;
	uint16_t	gd_blks;
	uint16_t	tot_blks;
	uint16_t	sec_trk;
	uint16_t	sec_cyl;
	uint16_t	n_tracks;
	uint16_t 	sec_blk;
	uint16_t	fat_size;
	uint16_t	spare1;
	uint8_t		fat_type;
	uint8_t		spare2[3];
	uint8_t		num_heads;
	uint8_t		num_parts;
	uint8_t		spare3[3];
	uint16_t	part_1_q;
	uint16_t	part_1_t;
};

struct DIR_ENTRY {
	uint32_t	file_len;
	uint8_t		file_mode;
	uint8_t		file_type;
	uint32_t	data_space PACKED;
	uint8_t		spare[4];
	uint16_t	fn_len;
	char		filename[36];
	uint32_t	date_update PACKED;
	uint16_t	version;
	uint16_t	fileno;
	uint32_t	date_backup;
};

#define Q_SSIZE		512
#define DIR_ENTRY_SIZE	0x40
#define TIME_DIFF	283996800

struct qopts {
	int image;
	char *image_name;
	int command;
};

enum qcommands {
	QUB_CMD_NONE,
	QUB_CMD_LONGDIR,
	QUB_CMD_SHORTDIR,
	QUB_CMD_INFO,
};

#endif /* _qubide.h */
