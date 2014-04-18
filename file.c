#include "ywy.h"

int ywy_sync_file(struct file *file, struct dentry *dentry, int datasync){
	printk(KERN_INFO "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	return 0;
}
const struct file_operations ywy_file_operations = {
	.llseek	   = generic_file_llseek,  //<linux/fs.h>
	.read	   = do_sync_read,
	.write	   = do_sync_write,
	.aio_read  = generic_file_aio_read,
	.aio_write = generic_file_aio_write,
	.mmap	   = generic_file_mmap,
	.open	   = generic_file_open,
	.fsync     = ywy_sync_file,
};

const struct inode_operations ywy_file_inode_operations = {
	.truncate  = ywy_truncate,
};
