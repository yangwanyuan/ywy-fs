#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "ywy.h"

MODULE_AUTHOR("YWY");
MODULE_DESCRIPTION("YWY Filesystem");
MODULE_LICENSE("GPL");

static struct kmem_cache *ywy_inode_cachep; //<linux/>


static void init_once(void *foo){
	struct ywy_inode_info *yi = (struct ywy_inode_info *)foo;
	//printk(KERN_INFO "super.c: init_once 0x%p", yi);
	inode_init_once(&yi->vfs_inode);   //<linux/fs.h>

}

static int init_inodecache(void){
	ywy_inode_cachep = kmem_cache_create("ywy_inode_cache", sizeof(struct ywy_inode_info), 0,
					(SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), init_once);   
	printk(KERN_INFO "super.c: init_inodecache");
	//<linux/slab.h>
	if (ywy_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static void destory_inodecache(void){
	kmem_cache_destroy(ywy_inode_cachep);
	printk(KERN_INFO "super.c: destory_inodecache");
}

static struct inode *ywy_alloc_inode(struct super_block *sb){
	struct ywy_inode_info *yi;
	
	yi = (struct ywy_inode_info *)kmem_cache_alloc(ywy_inode_cachep, GFP_KERNEL);
	if(!yi)
		return NULL;
	printk(KERN_INFO "super.c: ywy_alloc_inode");
	return &yi->vfs_inode;
}

static void ywy_destroy_inode(struct inode *inode){
	printk(KERN_INFO "super.c: ywy_destory_inode inode->i_ino = %d", (int)inode->i_ino);
	kmem_cache_free(ywy_inode_cachep, YWY_I(inode));
}

static int ywy_write_inode(struct inode *inode, int wait){
	printk(KERN_INFO "super.c: ywy_write_inode !!!!ino = %d", (int)inode->i_ino);
	brelse(ywy_update_inode(inode));
	return 0;
}

static void ywy_delete_inode(struct inode *inode){
	printk(KERN_INFO "super.c: ywy_delete_inode !!!ino = %d", (int)inode->i_ino);
	truncate_inode_pages(&inode->i_data, 0);//<linux/mm.h> 从0处开始截断所有页
	YWY_I(inode)->i_dtime = get_seconds();  //<linux/time.h>
	inode->i_size = 0;
	ywy_truncate(inode);
	ywy_free_inode(inode);
}

static int ywy_write_super(struct super_block *sb){
	printk(KERN_INFO "super.c: ywy_write_super sb->s_inodes_count = %d\n", (int)((struct ywy_sb_info *)sb->s_fs_info)->s_ys->s_inodes_count);
	return 0;
}

static void ywy_put_super(struct super_block *sb){
	struct ywy_sb_info *sbi = YWY_SB(sb);
	
	printk(KERN_INFO "super.c: ywy_put_super sb->s_inodes_count = %d\n", (int)((struct ywy_sb_info *)sb->s_fs_info)->s_ys->s_inodes_count);

	brelse(sbi->s_sbh);
	sb->s_fs_info = NULL;
	kfree(sbi);   //系统在创建sb是调用的是kmalloc

}

static int ywy_statfs(struct dentry *dentry, struct kstatfs *buf){
	printk(KERN_INFO "super.c: ywy_statfs");
	return 0;
}

static const struct super_operations ywy_sops={
	.alloc_inode	=ywy_alloc_inode,
	.destroy_inode  =ywy_destroy_inode,
	.write_inode    =ywy_write_inode,
	.delete_inode   =ywy_delete_inode,
	.write_super    =ywy_write_super,
	.put_super	=ywy_put_super,
	.statfs		=ywy_statfs,
};

static int ywy_fill_super(struct super_block *sb, void *data, int silent){
	struct buffer_head *bh;   //<linux/buffer_head.h>
	struct ywy_super_block *ys;
	struct ywy_sb_info *sbi;
	struct inode *root;

	unsigned long sb_block = 1;
	
	long ret = -EINVAL;

	int blocksize = BLOCK_SIZE;

	sbi = kzalloc(sizeof(struct ywy_sb_info), GFP_KERNEL); //<linux/slab.h> 申请内存并置零

	if(!sbi)
		return -ENOMEM;

	if(!sb_set_blocksize(sb, BLOCK_SIZE))  //<linux/fs.h>, 检查sb的block_size是否合法
		goto out_bad_hblock;
	if(!(bh = sb_bread(sb, sb_block))){
		printk(KERN_INFO "YWY-fs:unable to read superblock\n");
		goto failed_sbi;
	}

	ys = (struct ywy_super_block *)(bh->b_data);
	sbi->s_sbh = bh;
	sbi->s_ys = ys;
	sb->s_fs_info = sbi;
	sb->s_magic = ys->s_magic;

	if(sb->s_magic != YWY_SUPER_MAGIC)
		goto cantfind_ywy;

	blocksize = YWY_BLOCK_SIZE;

	sb->s_op = &ywy_sops;

	//TODO: load root 	

	root = ywy_iget(sb, YWY_ROOT_INO);   //"inode.c"
	if(IS_ERR(root)){    //<linux/err.h>
		ret = PTR_ERR(root);
		printk(KERN_ERR "YWY-fs: can't find root inode\n");
	goto failed_mount;
	}
	if (!S_ISDIR(root->i_mode) || !root->i_blocks || !root->i_size){  //<linux/stat.h>
		iput(root);  //<linux/fs.h> 删除inode
		printk(KERN_ERR "isdir?%d,root->i_blocks=%d,root->i_size=%d\n",\
			 (int)S_ISDIR(root->i_mode), (int)root->i_blocks, (int)root->i_size);
		printk(KERN_ERR "YWY-fs: corrupt root inode\n");
	goto failed_mount;
	}

	sb->s_root = d_alloc_root(root); //<linux/dcache.h>
	if (!sb->s_root){
		iput(root);
		printk(KERN_ERR "YWY: get root inode failed\n");
		ret = -ENOMEM;
		goto failed_mount;
	}

	//printk(KERN_INFO "ys->s_inodes_count=%d, ys->s_inode_size=%d, ys->s_blocks_count=%d, ys->s_free_blocks_count=%d, ys->s_free_inodes_count=%d, ys->s_first_data_block=%d, ys->s_first_ino=%d, ys->s_link_max=%d, ys->s_log_block_size=%d, ys->s_mtime=%d, ys->s_wtime=%d, ys->s_magic=%d", ys->s_inodes_count, ys->s_inode_size, ys->s_blocks_count, ys->s_free_blocks_count, ys->s_free_inodes_count, ys->s_first_data_block, ys->s_first_ino, ys->s_link_max, ys->s_log_block_size, ys->s_mtime, ys->s_wtime, ys->s_magic);
	return 0;

cantfind_ywy:
	printk(KERN_INFO "VFS: Can't find an ywy filesystem on dev %s.\n magic on dev is %d and magic of YWY is %d\n"\
		,sb->s_id,(int)sb->s_magic,(int)YWY_SUPER_MAGIC);
failed_mount:
	brelse(bh);  //<linux/buffer_head.h>  释放buffer_head
out_bad_hblock:
	printk("YWY-fs:blocksize too small for device\n");
failed_sbi:
	sb->s_fs_info = NULL;
	kfree(sbi);   //<linux/slab.h>  释放slab对象
	return ret;

}

static int ywy_get_sb(struct file_system_type *fs_type, 
	int flags, const char *dev_name, void *data, struct vfsmount *mnt){
	return get_sb_bdev(fs_type, flags, dev_name, data, ywy_fill_super, mnt);   
	//<linux/fs.h>提供接口,调用真实填写superblock函数
}

static struct file_system_type ywy_fs_type ={
	.owner	   = THIS_MODULE,
	.name	   = "ywy",
	.get_sb	   = ywy_get_sb,
	.kill_sb   = kill_block_super,   //  <linux/fs.h>
	.fs_flags  = FS_REQUIRES_DEV,     // <linux/fs.h>,这种类型的文件系统必须位于物理磁盘设备上
};

static int __init init_ywy_fs(void){
	printk(KERN_INFO "BEGIN!!");
	int err = init_inodecache();
	if (err)
		return err;
	err = register_filesystem(&ywy_fs_type);
	if (err)
		goto out;
	return 0;

out:
	destory_inodecache();
	return err;
}

static void __exit exit_ywy_fs(void){
	unregister_filesystem(&ywy_fs_type);
	destory_inodecache();
	printk(KERN_INFO "OVER!!");
}

module_init(init_ywy_fs)
module_exit(exit_ywy_fs)
