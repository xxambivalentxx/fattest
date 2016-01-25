/* Copyright (C) 2016 Jacob Paulsen <jspaulse@ius.edu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdio.h>
#include "fat.h"

/* prototypes */
static int lfn_ucs_to_ascii(struct lfn_entry *lfn, char *out, size_t *size);
static uint8_t gen_chksum(struct dir_entry *de);

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

/**
 * is_fat_32
 * 
 * @type_code		type_code found in mbr
 * 
 * @return true if fat32, false otherwise
 */
bool is_fat_32(uint8_t type_code) {
	return (type_code == FAT_TYPE_32 || type_code == FAT_TYPE_32_SMALL);
}

/**
 * is_fat_16
 * 
 * @type_code		type_code found in mbr
 * 
 * @return true if fat16, false otherwise
 **/
bool is_fat_16(uint8_t type_code) {
	return (type_code == FAT_TYPE_16 || type_code == FAT_TYPE_16_LBA || 
		type_code == FAT_TYPE_FAT16_SMALL);
}

/**
 * is_supported_version
 * 
 * @type_code	type received (generally) from MBR
 * 
 * @return true if supported, false otherwise
 **/
bool is_supported_version(uint8_t type_code) {
	return (is_fat_32(type_code) || is_fat_16(type_code));
}

/**
 * gen_chksum
 * 
 * generates a checksum used in lfn_entries
 * 
 * @de		dir_entry
 * 
 * @return checksum
 **/
static uint8_t gen_chksum(struct dir_entry *de) {
	uint8_t ret = 0;
	
	for (int i = 0; i < sizeof(de->short_fname); i++) {
		ret = ((ret & 1) << 7) + (ret >> 1) + de->short_fname[i];
	}
	
	return ret;
}

//uint32_t get_next_cluster(uint32_t curr_cluster) {
	

int lfn_read_fname(struct dir_entry *de, char *str) {
	int ret = 0;
	size_t size = 0;
	struct lfn_entry *lfn = (struct lfn_entry *)de;
	uint8_t checksum = gen_chksum(de);
	
	do {
		lfn--;
		
		if (lfn->checksum != checksum) {
			ret = -1;
			break;
		} else if (lfn_ucs_to_ascii(lfn, str, &size) != 0) {
			ret = -2;
			break;
		} else {
			str += size;
			
			/* if we have added the last characters on, append null byte */
			if (lfn->seq_num & LFN_LAST_ENTRY) {
				*str = '\0';
			}
		}
	} while (!(lfn->seq_num & LFN_LAST_ENTRY));

	
	return ret;
}
/**
 * lfn_strlen
 * 
 * returns the string length of a long file name
 * 
 * @de		dir_entry
 * @size	length of string
 * 
 * @return zero on success, negative otherwise
 * 
 */
int lfn_strlen(struct dir_entry *de, size_t *size) {
	int ret = 0;
	int lfn_cnt = 0;
	int char_cnt = 0;
	uint16_t *ptr = NULL;
	struct lfn_entry *lfn = (struct lfn_entry *)de;
	
	do {
		lfn--;
		
		if (lfn->seq_num & LFN_LAST_ENTRY) {
			ptr = lfn->first_nchars;
			
			while (lfn_cnt < CHARS_PER_ENTRY) {
				if (lfn_cnt == 5) {
					ptr = lfn->next_nchars;
				} else if (lfn_cnt == 11) {
					ptr = lfn->last_nchars;
				} else if (*ptr == 0xFFFF) {
					break;
				}
				
				char_cnt++;
				lfn_cnt++;
				ptr++;
			}
		} else {
			char_cnt += CHARS_PER_ENTRY;
		}
	} while (!(lfn->seq_num & LFN_LAST_ENTRY));
	
	*size = char_cnt;
	
	return ret;
}

/**
 * lfn_ucs_to_ascii
 * 
 * LFN UCS-2 to Ascii
 * 
 * converts an lfn_entry to an ascii string
 * 
 * @lfn		lfn_entry
 * @out		output string
 * @size	resulting string size
 * 
 * @return zero on success, negative otherwise
 **/
static int lfn_ucs_to_ascii(struct lfn_entry *lfn, char *out, size_t *size) {
	int ret = 0;
	uint16_t *ptr = NULL;
	int lfn_cnt = 0, out_cnt = 0;
	bool cont = true;
	
	ptr 	= lfn->first_nchars;
	
	while (lfn_cnt < CHARS_PER_ENTRY && cont == true) {
		uint8_t *ch = NULL;
		
		// make sure we're pointing at the right place
		if (lfn_cnt == 5) {
			ptr = lfn->next_nchars;
		} else if (lfn_cnt == 11) {
			ptr = lfn->last_nchars;
		}
		
		
		ch = (uint8_t *)ptr;
		// make sure everythis is okay...

		if (*ptr == 0xFFFF) {
			cont = false;
		} else if (ch[1] != 0x0) { // not ascii!
			ret 	= -1;
			cont 	= false;
		} else {
			out[out_cnt] = ch[0];
			out_cnt++;
		}
		
		lfn_cnt++;
		ptr++;
	}
	
	*size = out_cnt;
	return ret;
}

