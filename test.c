#include <stdio.h>
#include <stdlib.h>
#include "fat.h"
#define BYTES_PER_SECTOR 512	/* dun worry about this */
static FILE *fp = NULL;

/* offering up some "host" (lawl) functionality */
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
	
	if ((ret = read_sectors(mbe.start_sector, 1, (uint8_t *)&bpb)) == 0) { // read the bpb 
		if (is_supported_version(mbe.type_code)) {
			fti->type					= mbe.type_code;
			fti->bytes_per_sector		= bpb.bytes_per_sector;
			fti->sectors_per_cluster	= bpb.sectors_per_cluster;
			fti->first_sector			= mbe.start_sector;
			fti->first_fat_sector		= fti->first_sector + bpb.reserved_sector_cnt;
			fti->sectors_per_fat		= is_fat_32(mbe.type_code) ? bpb.fat32.sectors_per_fat : bpb.sectors_per_fat16;
			fti->first_cluster			= fti->first_fat_sector + (bpb.table_cnt * fti->sectors_per_fat);
			if (is_fat_32(mbe.type_code)) { /* too long to inline */
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

int read_directory(struct fat_info fti) {
	int ret = 0;
	uint8_t *buf = NULL;
	struct dir_entry *de = NULL;
	printf("entries per cluster: %lu\n", (fti.bytes_per_sector * fti.sectors_per_cluster) / sizeof(struct dir_entry));
	if (fti.bytes_per_sector > 0) { /* sanity check */
		if ((buf = malloc(fti.bytes_per_sector * fti.sectors_per_cluster)) != NULL) {
			if ((ret = read_sectors(fti.root_dir_sector, fti.sectors_per_cluster, buf)) == 0) {
				de = (struct dir_entry *)buf;
				int cnt = 0;
				
				while (de->short_fname[0] != 0) {
					printf("dir_entry[%i]\n", cnt);
					
					if (de->short_fname[0] != 0xE5) { /* delete or LFN */
						if (de->attr == ATTR_VOLUME_ID) {
							printf("VOLUME ID\n");
							printf("volume: %s\n", de->short_fname);
						} else if ((de->attr & ATTR_ARCHIVE) == ATTR_ARCHIVE) {
							size_t size = 0;
							char *fname = NULL;
							
							if (lfn_strlen(de, &size) == 0) {
								printf("strlen: %lu\n", size);
								if ((fname = malloc(size + 1)) != NULL) {
									if (lfn_read_fname(de, fname) == 0) {
										printf("file name: %s\n", fname);
									} else {
										printf("lfn_read_fname() failed!\n");
									}
								} else {
									printf("unable to allocate %lu bytes!\n", size);
									break;
								}
								
								free(fname);
							} else {
								printf("lfn_strlen() failed!\n");
								break;
							}
							
							printf("FILE ENTRY\n");
							printf("name: [%s]\n", de->short_fname);
							printf("attr: 0x%x\n", de->attr);
							printf("cluster_high: %i\n", de->cluster_high);
							printf("cluster_low: %i\n", de->cluster_low);
							printf("size: %i\n", de->size);

						} else if (de->attr == ATTR_LFN) {
							printf("LFN ENTRY\n");
							struct lfn_entry *lfn = (struct lfn_entry *)de;
							printf("seq_num: 0x%x\n", lfn->seq_num);
						}
					} else {
						printf("UNUSED/DELTED ENTRY\n");
					}
					
					printf("\n");
					de++;
					cnt++;
				}
						
				
			} else {
				printf("read_directory() - read_sectors returned %i\n", ret);
			}
		} else {
			ret = -1;
			printf("read_directory() - unable to allocate %i bytes!\n", fti.bytes_per_sector);
		}
	} else {
		ret = -2;
		printf("read_directory() - fti.bytes_per_sector is zero.\n");
	}
		
	free(buf);
	return 0;
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
				
				if ((ret = gen_fat_info(&fti, *mbr)) == 0) {
					if ((ret = read_directory(fti)) != 0) {
						printf("read_directory() failed with %i\n", ret);
					}
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

