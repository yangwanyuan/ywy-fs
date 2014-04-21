#include  <linux/buffer_head.h>
#include "ywy.h"

int ywy_sync_file(struct file *file, struct dentry *dentry, int datasync){
	printk(KERN_INFO "!!!!!!!!file.c: ywy_sync_file begin dentry->name = %s", dentry->d_name.name );
	struct inode *inode = dentry->d_inode;
	int err,ret;
	ret = sync_mapping_buffers(inode->i_mapping); // <linux/buffer_head.h>  同步buffer和磁盘上数据
	if(!(inode->i_state & I_DIRTY))   //<linux/fs.h>
		return ret;
	if(datasync && !(inode->i_state & I_DIRTY))
		return ret;
	err = ywy_sync_inode(inode);  //inode.c
	if(ret == 0)
		ret = err;
	printk(KERN_INFO "!!!!!!!!file.c: ywy_sync_file end");
	return ret;
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
