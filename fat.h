#ifndef FAT32_H
#define FAT32_H
#include <stdint.h>
// READ_ONLY=0x01 HIDDEN=0x02 SYSTEM=0x04 VOLUME_ID=0x08 DIRECTORY=0x10 ARCHIVE=0x20 LFN=READ_ONLY|HIDDEN|SYSTEM|VOLUME_ID
#define MBR_OFFSET	0x1BE /* this might not be named correctly */

/* dir_entry attributes */
#define ATTRB_READ_ONLY 0x1
#define ATTRB_HIDDEN	0x2
#define ATTRB_SYSTEM	0x4
#define ATTRB_VOLUME_ID	0x8
#define ATTRB_DIRECTORY	0x10
#define ATTRB_ARCHIVE	0x20	/* this is a file */
#define ATTRB_LFN		ATTRB_READ_ONLY | ATTRB_HIDDEN | ATTRB_SYSTEM | ATTRB_VOLUME_ID

/* first sector, LBA = 0 */
/* FAT_PART_16_SMALL is < 32MiB */
/* FAT_PART_32_SMALL is < 2GiB  */
/* think we'll only support FAT_PART_16/32_LBA */
#define FAT_TYPE_UNUSED			0
#define FAT_TYPE_FAT12			1
#define FAT_TYPE_FAT16_SMALL	4
#define FAT_TYPE_EXT			5
#define FAT_TYPE_16				6
#define FAT_TYPE_32_SMALL		11
#define FAT_TYPE_32				12
#define FAT_TYPE_16_LBA			14
#define FAT_TYPE_EXT_LBA		15
/**
 * @fat_bpb
 * 
 * File Allocation Bios Parameter Block
 * 
 * @jmp						jump instruction; can be ignored
 * @oem_id					oem identification; can be ignored
 * @bytes_per_sector		generally 512
 * @sectors_per_cluster		
 * @reserved_sector_cnt		
 * @fat_cnt					number of file allocation tables
 * @dir_entries_cnt			number of directory entries
 * @total_sectors			number of sectors on disk/partition; if 0, refer to large_total_sectors
 * @media_descriptor_type	
 * @sectors_per_fat_16_12	sectors per fat table; disregard if using fat32
 * @sectors_per_track
 * @heads_sides_cnt			number of heads or sides on media; can probably be ignored (BECAUSE IT IS 2016!)
 * @hidden_sector_cnt		number of hidden sectors
 * @large_total_sectors		refer here if total_sectors is 0
 **/	
struct fat_bpb {
	uint8_t		jmp[3];					/* jmp instruction, disregard */
	uint8_t		oem_id[8];				/* oem identifier, can be disregarded */
	uint16_t	bytes_per_sector;
	uint8_t		sectors_per_cluster;
	uint16_t	reserved_sector_cnt;
	uint8_t		table_cnt;				/* number of File Allocation Tables */
	uint16_t	dir_entries_cnt;		/* number of directory entries, disregard on fat32 */
	uint16_t	total_sector_cnt;		/* refer to larger entry if zero */
	uint8_t		media_descriptor_type;	/* whaddafuck? */
	uint16_t	sectors_per_fat16;		/* reserved for fat12/16 */
	uint16_t	sectors_per_track;		/* whaddafuck? */
	uint16_t	heads_sides_cnt;		/* number of heads/sides (dunno) */
	uint32_t	hidden_sector_cnt;
	uint32_t	large_total_sector_cnt;	/* refer here if total_sectors is 0. */
} __attribute__ ((packed));

/**
 * fat16_ebpb
 * 
 * Fat16 Extended Bios Parameter Block
 * 
 * @bpb					Bios Parameter Block
 * @drive_number		0x13 floppy, 0x80 hdd
 * @win_nt_flags		can be ignored
 * @signature			must be 0x28/0x29
 * @volumeid_serial		volume id 'serial' number; can be ignored
 * @volume_label		label of the volume/partition
 * @system_id			can be ignored?
 * @boot_code			boot code stuff
 * @boot_signature		must be 0xAA55
 **/
struct fat16_extbpb {
	struct fat_bpb 	bpb;
	uint8_t			drive_number;
	uint8_t			win_nt_flags;
	uint8_t			signature;
	uint32_t		volumeid_serial;
	uint8_t			volume_label[11];
	uint8_t			system_id[8];
	uint8_t			boot_code[448];
	uint16_t		bootable_signature;
} __attribute__ ((packed));
	

/**
 * fat32_ebpb
 * 
 * Fat32 Extended Bios Parameter Block
 * 
 * @bpb						Bios Parameter Block
 * @sectors_per_fat			size of file allocation table in sectors
 * @flags				
 * @version					high bytes is major, low byte is minor
 * @root_dir_cluster_number	cluster number of root directory
 * @fs_info_sector			sector number of fsinfo structure
 * @backup_boot_sector		sector number of backup boot sector
 * @reserved
 * @drive_number			0x13 floppy, 0x80 hdd
 * @win_nt_flags			can be ignored
 * @signature				must 0x28/0x29
 * @volumeid_serial			volume id 'serial' number; can be ignored
 * @volume_label			volume label string
 * @system_id				system_id, generally states "FAT32" can be ignored?
 * @boot_code				boot code stuff
 * @bootable_signature		always 0xAA55
 **/
struct fat32_extbpb {
	struct fat_bpb		bpb;
	uint32_t			sectors_per_fat;	
	uint16_t			flags;
	uint8_t				version[2];
	uint32_t			root_cluster;
	uint16_t			fs_info_sector;			
	uint16_t			backup_boot_sector;
	uint8_t				reserved[12];
	uint8_t				drive_number;
	uint8_t				win_nt_flags;			/* disregard */
	uint8_t				signature;				/* must be 0x28/0x29 */ 
	uint32_t			volumeid_serial;		
	uint8_t				volume_label[11];
	uint8_t				system_id[8];
	uint8_t				boot_code[420];
	uint16_t			bootable_signature;		/* 0xAA55 */
} __attribute__ ((packed));

/**
 * fat_partition
 * @boot_flags		can be ignored
 * @start_chs		can be ignored
 * @type			FAT_PART_XX, see above
 * @end_chs			can be ignored
 * @start_sector	starting sector of partition
 * @size			size (in sectors) of partition
 **/
struct fat_partition {
	uint8_t		boot_flag;
	uint8_t		start_chs[3];
	uint8_t		type;
	uint8_t		end_chs[3];
	uint32_t	start_sector;
	uint32_t	size;
} __attribute__ ((packed));

/**
 * dir_entry
 * 
 * entry within a directory
 * 
 * @entry_name				technically 11 bytes, split between ext.  padded if shorter than 11 bytes
 * @ext						file extension
 * @attrb					entry attributes
 * @win_nt_reserved			are we on windows nt? no? STFU
 * @creation_time_tenths	creation time for tenths of seconds (who the fuck knows why?)
 * @creation_time			entry creation time; format, 5:6:5, hours:minutes:seconds
 * @creation_date			entry creation date; format, 7:4:5, year:month:day
 * @last_access_date		same format as creation_date
 * @cluster_high			location 
 */
struct dir_entry {
	uint8_t		entry_name[8];
	uint8_t		ext[3];
	uint8_t		attrb;
	uint8_t		win_nt_reserved;
	uint8_t		creation_time_tenths;
	uint16_t	creation_time;			/* hour:minutes:seconds, 5b:6b:5b */
	uint16_t	creation_date;			/* year:month:day, 7b:4b:5b */
	uint16_t	last_access_date;		/* ^^^ this format */
	uint16_t	cluster_high;
	uint16_t	last_mod_time;			/* creation time format */
	uint16_t	last_mod_date;			/* creation date format */
	uint16_t	cluster_low;
	uint32_t	size;
} __attribute__ ((packed));

#endif
