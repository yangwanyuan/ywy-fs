#include <linux/fs.h>
#include <linux/types.h>
#include <linux/ywy_fs.h>

struct ywy_inode_info{
	__le32 i_start_block;
	__le32 i_end_block;
	__le32 i_blocks;
	__le32 i_reserved;
	__u32  i_flags;
	__u32  i_dtime;
	struct mutex truncate_mutex;
	struct inode vfs_inode;
};

static inline struct ywy_inode_info *YWY_I(struct inode *inode){
	return container_of(inode, struct ywy_inode_info, vfs_inode);
}

static inline struct ywy_sb_info *YWY_SB(struct super_block *sb){
        return sb->s_fs_info;
}

/*  inode.c  */
extern struct inode *ywy_iget(struct super_block *, unsigned long);
extern struct ywy_inode *ywy_raw_inode(struct super_block *, ino_t, struct buffer_head **);

extern struct inode *ywy_new_inode(struct inode *dir, int *err);

extern void ywy_set_inode(struct inode*, dev_t);

extern int __ywy_write_begin(struct file *file, struct address_space *mapping, loff_t pos, unsigned len, unsigned flags, struct page *pagep, void **fsdata);

extern struct buffer_head *ywy_update_inode(struct inode *inode);

extern void ywy_truncate(struct inode *inode);
extern void ywy_free_inode(struct inode *inode);

/*   dir.c   */
extern inline void ywy_put_page(struct page *page);
extern struct ywy_dir_entry *ywy_find_entry(struct dentry*, struct page **);

extern int ywy_add_link(struct dentry*, struct inode*);
extern int ywy_delete_entry(struct ywy_dir_entry *de, struct page *page);
extern int ywy_make_empty(struct inode *inode, struct inode *dir);
extern int ywy_empty_dir(struct inode* inode);

/*   inode.c */
extern const struct address_space_operations ywy_aops;

/*   file.c */
extern const struct inode_operations ywy_file_inode_operations;
extern const struct file_operations ywy_file_operations;

/*   namei.c */
extern const struct inode_operations ywy_dir_inode_operations;

/*   dir.c   */
extern const struct file_operations ywy_dir_operations;
