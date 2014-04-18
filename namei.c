#include "ywy.h"'

static int ywy_mknod(struct inode *inode, struct dentry *dentry, int mode, dev_t rdev);

static int add_nondir(struct dentry *dentry, struct inode *inode){
	printk(KERN_INFO "namei.c: add_nondir begin fatherinode->i_ino = %d,inode->i_ino = %d",\
		(int)dentry->d_parent->d_inode->i_ino, (int)inode->i_ino);
	int err = ywy_add_link(dentry, inode);  //dir.c 将目录项写进父目录中
	//printk(KERN_INFO "namei.c: add_nondir err = %d", err);
	if(!err){
		printk(KERN_INFO "namei.c: add_nondir end");
		d_instantiate(dentry, inode);  //<linux/dcache.h>  将inode信息写进dentry中
		return 0;
	}
	printk(KERN_INFO "namei.c: add_nondir end error");
	inode_dec_link_count(inode);  //<linux/fs.h> 减少inode的link数
	iput(inode);  //<linux/fs.h>  释放inode
	return err;
}

static int ywy_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd){
	printk(KERN_INFO "namei.c: ywy_create");
	return ywy_mknod(dir, dentry, mode, 0);
}

//查找目录项，绑定dentry与相应inode，最后返回dentry
static struct dentry *ywy_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd){
	struct inode *inode = NULL;
	ino_t ino = 0;
	dentry->d_op = dir->i_sb->s_root->d_op;
	
	if(dentry->d_name.len > YWY_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);
	struct page *page;

	struct ywy_dir_entry *de = ywy_find_entry(dentry, &page);  //dir.c
	if(de){
		ino = de->ino;
		ywy_put_page(page);   //dir.c
	}
	if(ino){
		inode = ywy_iget(dir->i_sb, ino);   //inode.c
		if(IS_ERR(inode))
			return ERR_CAST(inode);
	}
	
	d_add(dentry, inode); //将inode与dentry联系起来 <linux/dcache.h>
	printk(KERN_INFO "namei.c: ywy_lookup");

	return NULL;
}

static int ywy_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry){
	printk(KERN_INFO "namei.c: ywy_link");
	return 0;
}

static int ywy_unlink(struct inode *dir, struct dentry *dentry){
	int err = -ENOENT;
	struct inode *inode = dentry->d_inode;
	struct page *page;
	struct ywy_dir_entry *de;
	printk(KERN_INFO "namei.c: ywy_unlink");
	de = ywy_find_entry(dentry, &page);
	if(!de)
		goto end_unlink;
	printk(KERN_INFO "aaaaaaa");
	err = ywy_delete_entry(de, page);  //dir.c
	if(err)
		goto end_unlink;
	printk(KERN_INFO "bbbbbb");
	inode->i_ctime = dir->i_ctime;
	inode_dec_link_count(inode);  //<linux/fs.h> inode的link数目减一
	err = 0;
end_unlink:
	printk(KERN_INFO "ccccc");
	return err;
}

static int ywy_symlink(struct inode *dir, struct dentry *dentry, const char* symname){
	printk(KERN_INFO "namei.c: ywy_symlink");
	return 0;
}

static int ywy_mkdir(struct inode *dir, struct dentry *dentry, int mode){
	struct inode *inode;
	struct ywy_super_block *ys = YWY_SB(dir->i_sb)->s_ys;
	int err = -EMLINK; //too many links

	printk(KERN_INFO "namei.c: ywy_mkdir begin");
	if(dir->i_nlink >= (ys->s_link_max))
		goto out;
	inode_inc_link_count(dir);  //<linux/fs.h> link数增加
	inode = ywy_new_inode(dir, &err);
	if(!inode)
		goto out_dir;
	inode->i_mode = S_IFDIR | mode;
	if(dir->i_mode & S_ISGID)
		dir->i_mode |= S_ISGID;
	ywy_set_inode(inode, 0);
	inode_inc_link_count(inode);

	err = ywy_make_empty(inode, dir);
	if(err)
		goto out_fail;
	err=ywy_add_link(dentry, inode);
	if(err)
		goto out_fail;
	d_instantiate(dentry, inode);  //<linux/dcache.h> 将inode写进dentry
	
	printk(KERN_INFO "namei.c: ywy_mkdir end inode->i_ino = %d\n", (int)inode->i_ino);
out:
	return err;
out_fail:
	// 需要减去两次link，因为在alloc_inode时候设置了一次inode->i_nlink = 1
	inode_dec_link_count(inode); //<linux/fs.h>  link数减少
	inode_dec_link_count(inode);
	iput(inode);  //<linux/fs.h>
out_dir:
	inode_dec_link_count(inode);
	goto out;
}

static int ywy_rmdir(struct inode *dir, struct dentry *dentry){
	printk(KERN_INFO "namei.c: ywy_rmdir");
	return 0;
}

static int ywy_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t rdev){
	int error;
	struct inode *inode;
	printk(KERN_INFO "namei.c: ywy_mknod begin");
	if(!old_valid_dev(rdev))   //<linux/kdev_t.h>  旧的文件系统主设备和从设备号小于256
		return -EINVAL;
	
	inode = ywy_new_inode(dir, &error);  //inode.c
	if(inode){
		inode->i_mode = mode;
		ywy_set_inode(inode, rdev); //inode.c

		mark_inode_dirty(inode); //<linux/fs.h> 讲inode放到super block的dirty表中，调用superblock中写inode？
		
		error = add_nondir(dentry, inode);
	}
	printk(KERN_INFO "namei.c: ywy_mknod end inode = %d", (int)inode->i_ino);
	return error;
}

static int ywy_rename(struct inode *old_dir, struct dentry *old_dentry,
			struct inode *new_dir, struct dentry *new_dentry){
	printk(KERN_INFO "namei.c: ywy_rename");
	return 0;
}

const struct inode_operations ywy_dir_inode_operations = {
	.create    = ywy_create,
	.lookup    = ywy_lookup,
	.link      = ywy_link,
	.unlink	   = ywy_unlink,
	.symlink   = ywy_symlink,
	.mkdir     = ywy_mkdir,
	.rmdir     = ywy_rmdir,
	.mknod     = ywy_mknod,
	.rename    = ywy_rename,
};
