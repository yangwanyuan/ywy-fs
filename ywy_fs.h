#ifndef _LINUX_YWY_FS_H
#define _LINUX_YWY_FS_H

#include <linux/types.h>
#include <linux/magic.h>

#define YWY_DEFAULT_RESERVE_BLOCK  8

#define YWY_ROOT_INO  1
#define YWY_FIRST_INO  2

#define YWY_LINK_MAX  20

#define YWY_BLOCK_SIZE 1024
#define YWY_BLOCK_LOG_SIZE  10
#define YWY_BLOCKS(s) (YWY_SB(s)->s_ys->s_blocks_count)

#define YWY_ADDR_PER_BLOCK  (YWY_BLOCK_SIZE / sizeof(__u32))
#define YWY_INODE_SIZE  sizeof(struct ywy_inode)
#define YWY_INODE_BLOCK_COUNT(s)  ((YWY_INODE_COUNT(s) * YWY_INODE_SIZE - 1) / YWY_BLOCK_SIZE) 
#define YWY_INODE_COUNT(s)  (YWY_SB(s)->s_ys->s_inodes_count)
#define YWY_INODE_PER_BLOCK ((BLOCK_SIZE) / (sizeof (struct ywy_inode)))

#define YWY_INODE_BLOCK  3
#define YWY_BLOCK_RESERVED  10


struct ywy_inode{
	__le16  i_mode;
	__le16  i_uid;
	__le16  i_gid;
	__le32  i_size;
	__le32  i_atime;
	__le32  i_ctime;
	__le32  i_mtime;
	__le32  i_dtime;
	__le16  i_nlinks;
	__le32  i_flags;
	__le32  i_start_block;
	__le32  i_end_block;
	__le32  i_blocks;
	__le16  i_dev;
	__le32  i_reserved;
	__u8    i_nouse[10];
};

struct ywy_super_block{
	__le32  s_inodes_count;
	__le16  s_inode_size;
	__le32  s_blocks_count;
	__le32  s_free_blocks_count;
	__le32  s_free_inodes_count;
	__le32  s_first_data_block;
	__le32  s_first_ino;
	__le32  s_link_max;
	__le32  s_log_block_size;
	__le32  s_mtime;
	__le32  s_wtime;
	__le16  s_magic;
};

struct ywy_sb_info{
	struct ywy_super_block *s_ys;
	struct buffer_head *s_sbh;
};

typedef unsigned long ywy_fsblk_t;

#define YWY_NAME_LEN 60
struct ywy_dir_entry{
	__le32 ino;
	char name[YWY_NAME_LEN];
};
#endif
