#include <stdio.h>
#include <stdlib.h>
#include <byteswap.h>
#include <stdbool.h>
#include "fat.h"
#define BYTES_PER_SECTOR 512	/* dun worry about this */
static FILE *fp = NULL;


struct fat_info {
	/* both relative_start_cluster/sector are relative to the start of the partition */
	uint8_t			type;						/* defined as FS_TYPE_XX */
	uint16_t		bytes_per_sector;	
	uint32_t		first_cluster;
	uint32_t		first_sector;
	uint32_t		first_fat_sector;
	uint32_t		root_dir_sector;
	uint32_t		sectors_per_fat;			/* can be pull directly from structure */
	uint32_t		sectors_per_cluster;		/* can be pull directly from structure */
};

/**
 * clust_to_sect
 * 
 * calculates the beginning sector to a cluster
 * 
 * @start_clust				starting cluster in a partition
 * @clust_num				requested cluster to sector number
 * @sect_per_clust			number of sectors per cluster in partition
 *
 * @return beginning sector to a cluster
 **/
uint32_t clust_to_sect(uint32_t start_clust, uint32_t clust_num, uint32_t sect_per_clust) {
	return start_clust + (clust_num - 2) * sect_per_clust;
}

static bool is_fat32(uint8_t type_code) {
	return (type_code == FAT_TYPE_32 || type_code == FAT_TYPE_32_SMALL);
}

static bool is_fat16(uint8_t type_code) {
	return (type_code == FAT_TYPE_16 || type_code == FAT_TYPE_16_LBA || 
		type_code == FAT_TYPE_FAT16_SMALL);
}

/**
 * is_supported_fat
 * 
 * @type_code	type received (generally) from MBR
 * 
 * @return true if supported, false otherwise
 **/
bool is_supported_version(uint8_t type_code) {
	return (is_fat32(type_code) || is_fat16(type_code));
}

/* offering up some "host" functionality */
int read_sectors(uint32_t sector_start, size_t cnt, uint8_t *buf) {
	int ret = 0;
	
	if (fp != NULL) {
		if (fseek(fp, sector_start * BYTES_PER_SECTOR, SEEK_SET) == 0) {
			if (fread(buf, BYTES_PER_SECTOR * cnt, 1, fp) != 1) {
				printf("read_sector() failed to read element!\n");
				ret = -1;
			}
		} else {
			printf("read_sector(): fseek failed!?\n");
			ret = -2;
		}
	} else {
		printf("fp == NULL\n");
		ret = -3;
	}
	
	return ret;
}

/* some functions */
int gen_fat_info(struct fat_info *fti, struct mbr_entry mbe) {
	int ret = 0;
	struct fat_bpb bpb;
	
	bpb.fat32.bootable_signature = 0xAA55;
	
	if ((ret = read_sectors(mbe.start_sector, 1, (uint8_t *)&bpb)) == 0) { // read the bpb 
		if (is_supported_version(mbe.type_code)) {
			fti->type 					= mbe.type_code;
			fti->bytes_per_sector		= bpb.bytes_per_sector;
			fti->sectors_per_cluster	= bpb.sectors_per_cluster;
			fti->first_sector			= mbe.start_sector;
			fti->first_fat_sector		= fti->first_sector + bpb.reserved_sector_cnt;
			fti->sectors_per_fat		= is_fat32(mbe.type_code) ? bpb.fat32.sectors_per_fat : bpb.sectors_per_fat16;
			fti->first_cluster			= fti->first_fat_sector + (bpb.table_cnt * fti->sectors_per_fat);
			if (is_fat32(mbe.type_code)) { /* too long to inline */
				fti->root_dir_sector	= clust_to_sect(fti->first_cluster, bpb.fat32.root_cluster, fti->sectors_per_cluster);
			} else {
				fti->root_dir_sector	= fti->first_fat_sector + (bpb.table_cnt * fti->sectors_per_fat);
			}
		} else {
			ret = mbe.type_code;
			printf("unsupported fat 0x%x\n", ret);
		}
	} else {
		printf("gen_fs_info() - unable to read in bpb, failed with %i\n", ret);
	}
	
	return ret;
}
		
/* 
 * Every sector of of the FAT holds 128 of these 32 bit integers, so looking up the next 
 * cluster of a file is relatively easy. Bits 7-31 of the current cluster tell you which sectors to read from the FAT, and bits 
 * 0-6 tell you which of the 128 integers in that sector contain is the number of the next cluster of your file (or if all ones, that the current cluster is the last).
 */
int main() {
	uint8_t *buf = NULL;
	int ret = 0;
	
	if ((fp = fopen("/dev/sdb", "rb")) != NULL) {
		if ((buf = malloc(MBR_SECTOR_SIZE)) != NULL) {
			struct mbr_entry *mbr = NULL;
			struct fat_info fti;
			
			/* read the mbr */
			if ((ret = read_sectors(0, 1, buf)) == 0) {
				mbr = (struct mbr_entry *)(buf + MBR_OFFSET);
				
				if ((ret = gen_fat_info(&fti, mbr)) == 0) {
					/* lets read a fukkin' directory! */
				} else {
					printf("gen_fat_info() failed with %i\n", ret);
				}
			} else {
				printf("read_sectors() failed with %i\n", ret);
			}
		} else {
			printf("Unable to allocate %i bytes!\n", MBR_SECTOR_SIZE);
		}
	} else {
		printf("Unable to open file!\n");
	}

	fclose(fp);
	free(buf);
	return 0;
}

