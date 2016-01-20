#include <stdio.h>
#include <stdlib.h>
#include <byteswap.h>
#include "fat.h"
#define GENERIC_SECTOR_SIZE	512

/**
 * cluster_to_sector
 * 
 * calculates the beginning sector to a cluster
 * 
 * @rel_start_clust			relative_start_cluster, starting cluster in a partition
 * @clust_num				requested cluster to sector number
 * @sect_per_clust			number of sectors per cluster in partition
 *
 * @return beginning sector to a cluster
 **/
uint32_t cluster_to_sector(uint32_t rel_start_clust, uint32_t clust_num, uint32_t sect_per_clust) {
	return rel_start_clust + (clust_num - 2) * sect_per_clust;
}
/**
 * fs_info
 * 
 * File System Information
 * 
 * Used to hold relevant file system information
 * @test
 */
struct fs_info {
	/* both relative_start_cluster/sector are relative to the start of the partition */
	uint8_t			fs_type;					/* defined as FS_TYPE_XX */
	uint32_t		fat_start_sector;
	uint32_t		root_dir_cluster;
	uint32_t		sectors_per_fat;			/* can be pull directly from structure */
	uint32_t		sectors_per_cluster;		/* can be pull directly from structure */
	uint32_t		root_cluster;				/* can be pull directly from structure */
	uint32_t		bytes_per_sector;			/* can be pull directly from structure */
};
/*
 * if partition type is anything but 12 (or whatever fat32 is) throw a fucking fit.
 * 
 * "relative linear offset to the start of the partition. "
 */
//lba_addr = cluster_begin_lba + (cluster_number - 2) * sectors_per_cluster; ANY SECTOR
int main() {
	FILE *fp 	= NULL;// = fopen("rasp.img", "rb");
	/* can add pointers like (struct bullshit_t *)(buf + OFFSET); */
	
	if ((fp = fopen("/dev/sdb", "rb")) != NULL) {
		/* might end up in a structure */
		struct fat32_extbpb extbpb;
		struct fat_partition fpt;
		struct dir_entry de;
		struct fs_info fs;
		unsigned int root_directory = 0;
		unsigned int first_data_sector = 0;
		/* "fat_size" is either bpb.sectors_per_fat16 if fat16 or extbpb.sectors_per_fat */
		unsigned int fat_size = 0;
		
		fseek(fp, MBR_OFFSET, SEEK_SET);
		fread(&fpt, sizeof(struct fat_partition), 1, fp);
		
		
		/* it looks like you just slap high/low together to get the first cluster? hum-ho */
		
		
		/* calculate next offset */
		fseek(fp, GENERIC_SECTOR_SIZE * fpt.start_sector, SEEK_SET);
		fread(&extbpb, sizeof(struct fat32_extbpb), 1, fp);
		
		fs.fs_type					= fpt.type;
		fs.bytes_per_sector			= extbpb.bpb.bytes_per_sector;			/* this is mostly for testing purposes */
		fs.sectors_per_fat			= extbpb.sectors_per_fat;				/* this would be extbpb.bpb.sectors_per_fat16 if fat 16 */
		fs.sectors_per_cluster		= extbpb.bpb.sectors_per_cluster;
		fs.root_cluster				= extbpb.root_cluster;
		fs.fat_start_sector			= fpt.start_sector + extbpb.bpb.reserved_sector_cnt;
		fs.root_dir_cluster			= fpt.start_sector + extbpb.bpb.reserved_sector_cnt + (extbpb.bpb.table_cnt * fs.sectors_per_fat);
		
		/* relative_start_sector should be the fat sector? if this is the case, we should see a reference in slot #2 of nada (0xFFFFFFFF?) */
		fseek(fp, fs.bytes_per_sector * fs.fat_start_sector, SEEK_SET);
		unsigned int table[512];
		fread(&table, 4 * 512, 1, fp);
		for (int i = 0; i < 5; i++) {
			printf("[%i]: 0x%x\n", i, table[i]);
		}
		
		printf("[0x98]: 0x%x\n", table[0x98]);
		fseek(fp, cluster_to_sector(fs.root_dir_cluster, fs.root_cluster, fs.sectors_per_cluster) * fs.bytes_per_sector, SEEK_SET);
		
		for (int i = 0; i < 24; i++) {
			fread(&de, sizeof(struct dir_entry), 1, fp);
	
			printf("[%s].[%s]\n", de.entry_name, de.ext);
			printf("attb: 0%x\n", de.attrb);
			printf("size: %i\n", de.size);
			printf("high: 0x%x\n", de.cluster_high);
			printf("low: 0x%x\n", de.cluster_low);
			
			/* first fat sector is after reserved sectors? first_fat_sector = extbpb.bpb.reserved_sector_cnt 
			* Every sector of of the FAT holds 128 of these 32 bit integers, so looking up the next 
			 * cluster of a file is relatively easy. Bits 7-31 of the current cluster tell you which sectors to read from the FAT, and bits 
			 * 0-6 tell you which of the 128 integers in that sector contain is the number of the next cluster of your file (or if all ones, that the current cluster is the last).
			 */
		}
		
		printf("gonna try and read text\n");
		fseek(fp, cluster_to_sector(fs.root_dir_cluster, 0x98, fs.sectors_per_cluster) * fs.bytes_per_sector, SEEK_SET);
		char test[33];
		fread(&test, 32, 1, fp);
		test[32] = '\0';
		printf("test: %s\n", test);
	} else {
		printf("unable to open img!\n");
	}
	
	fclose(fp);
	
	return 0;
}

