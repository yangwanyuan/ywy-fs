#include <linux/buffer_head.h>

#include "ywy.h"

int ywy_get_block(struct inode *inode, sector_t iblock,struct buffer_head *bh, int create);

//释放磁盘上的ywy_inode信息
void ywy_free_inode(struct inode *inode){
	struct super_block *sb = inode->i_sb;
	struct ywy_super_block *ys = YWY_SB(sb)->s_ys;
	struct buffer_head *bh;
	unsigned long ino;
	ino = inode->i_ino;
	struct ywy_inode *raw_inode;
	printk(KERN_INFO "inode.c: ywy_free_inode begin inode->i_ino = %d", (int)inode->i_ino);
	
	if(ino < 1 || ino > ys->s_inodes_count ){
		printk("ywy_free_inode: inode 0 or nonexistent inode\n");
		return;
	}
	raw_inode = ywy_raw_inode(sb, ino, &bh);
	if(raw_inode){
		raw_inode->i_nlinks = 0;
		raw_inode->i_mode = 0; //其他值不由这里写入，由之前inode置脏写入
	}
	if(bh){
		mark_buffer_dirty(bh);  //fs下buffer.c文件
		brelse(bh);
	}
	clear_inode(inode);  //<linux/fs.h>  清除内存中inode
	printk(KERN_INFO "inode.c: ywy_free_inode end");
}

//同步磁盘上的ywy_inode信息，用内存中inode填充buffer_head，然后标记脏
struct buffer_head *ywy_update_inode(struct inode *inode){
	struct ywy_inode_info *yi = YWY_I(inode);
	struct super_block *sb = inode->i_sb;
	
	ino_t ino = inode->i_ino;
	
	struct buffer_head *bh;
	struct ywy_inode *raw_inode = ywy_raw_inode(sb, ino, &bh);//根据超级块和ino从磁盘读inode信息
	
	printk(KERN_INFO "inode.c : ywy_update_inode inode->ino = %d\n", (int)inode->i_ino);
	
	if(!raw_inode) 
		return NULL;
	
	raw_inode->i_mode = inode->i_mode;
	raw_inode->i_uid  = inode->i_uid;
	raw_inode->i_gid =  inode->i_gid;
	raw_inode->i_nlinks = inode->i_nlink;
	raw_inode->i_size = inode->i_size;
	raw_inode->i_atime = inode->i_atime.tv_sec;
	raw_inode->i_mtime = inode->i_mtime.tv_sec;
	raw_inode->i_ctime = inode->i_ctime.tv_sec;

	if(S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		raw_inode->i_dev = old_encode_dev(inode->i_rdev); //<linux/kdev_t.h> 给设备号进行处理
	else{
		raw_inode->i_start_block = yi->i_start_block;
		raw_inode->i_end_block = yi->i_end_block;
		raw_inode->i_blocks = yi->i_blocks;
		raw_inode->i_reserved = yi->i_reserved;
	}
	mark_buffer_dirty(bh);  //fs下buffer.c文件
	return bh;
}

//截断inode节点，全部置零,只针对文件夹
void ywy_truncate(struct inode *inode){
	if(!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode)))
		return;
	printk(KERN_INFO "inode.c: ywy_truncate begin inode->ino = %d", (int)inode->i_ino);

	struct ywy_inode_info *yi = YWY_I(inode);
	block_truncate_page(inode->i_mapping, inode->i_size, ywy_get_block);// <linux/buffer_head.h> 
	
	yi->i_reserved += yi->i_end_block - yi->i_start_block + 1; //设置预留块
	yi->i_end_block = yi->i_start_block;//清空
	yi->i_blocks = 0;
	
	inode->i_mtime = inode->i_ctime = CURRENT_TIME_SEC;
	mark_inode_dirty(inode);//<linux/fs.h>
	printk(KERN_INFO "inode.c: ywy_truncate end");
}

//同步inode节点
int ywy_sync_inode(struct inode *inode){
	printk(KERN_INFO "!!!!!!!!!!inode.c: ywy_sync_inode begin inode->i_ino = %d\n",(int)inode->i_ino);
	int ret = 0;
	struct buffer_head *bh;
	bh = ywy_update_inode(bh);  //获取到磁盘索引节点所在的缓冲区
	if(bh && buffer_dirty(bh)){  //<linux/buffer_head.h>中的宏定义 如果为脏就同步
		sync_dirty_buffer(bh); //<linux/buffer_head.h>  
		if(buffer_req(bh) && !buffer_uptodate(bh)){  // <linux/buffer_head.h>中宏定义
			printk("IO error syncing ywy inode\n");
			ret = -1;
		}
	}else if(!bh)
		ret = -1;
	brelse(bh);
	printk(KERN_INFO "!!!!!!!!!!inode.c: ywy_sync_inode end");
	return ret;
}

//新申请一个inode并填充
struct inode *ywy_new_inode(struct inode *dir, int *err){
	struct super_block *sb;
	struct buffer_head *bh;
	
	ino_t ino = 0;
	int block;

	struct inode *inode;

	struct ywy_inode_info *yi;

	struct ywy_inode *raw_inode;
	char *p;

	printk(KERN_INFO "inode.c: ywy_new_inode begin");

	sb = dir->i_sb;
	inode = new_inode(sb); //<linux/fs.h> 调用vfs函数来，vfs函数会调用sb->alloc_inode来分配新inode
	if(!inode)
		return ERR_PTR(-ENOMEM);
	yi = YWY_I(inode);

	inode->i_uid = current->real_cred->fsuid;
	inode->i_gid = (dir->i_mode & S_ISGID)? dir->i_gid : current->real_cred->fsgid;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME_SEC; //<linux/time.h>
	
	block = YWY_INODE_BLOCK - 1; // inode表所在块号
	struct ywy_inode *prev = NULL;
	while(bh = sb_bread(sb, block)){
		p = bh->b_data;
		while(p <= (bh->b_data + YWY_BLOCK_SIZE - YWY_INODE_SIZE)){
			raw_inode = (struct ywy_inode *)p;
			ino ++;
			if(!raw_inode->i_nlinks && !raw_inode->i_start_block){//找到一个连接为0的节点，可用的inode
				if(!prev->i_reserved)
					prev->i_reserved = YWY_BLOCK_RESERVED;
				prev->i_blocks = prev->i_end_block - prev->i_start_block + 1;
			
				mark_buffer_dirty(bh); //fs下buffer.c文件
				goto find;
			}
			p += YWY_INODE_SIZE;
			prev = raw_inode;
		}
		brelse(bh);
		if(block > YWY_INODE_BLOCK_COUNT(sb))
			break;
		block++;
	}
	
	iput(inode);
	brelse(bh);
	*err = -ENOSPC;
	return NULL;

find:
	inode->i_ino = ino;
	
	raw_inode->i_start_block = prev->i_end_block + prev->i_reserved + 1;
	yi->i_reserved = raw_inode->i_reserved;
	yi->i_start_block = yi->i_end_block = raw_inode->i_start_block;
	raw_inode->i_end_block = raw_inode->i_start_block;

	//printk(KERN_INFO "inode.c: ywy_new_inode end raw_inode->i_start_block = %d, i_end_block = %d, i_reserved = %d",\
	//	(int)raw_inode->i_start_block,(int)raw_inode->i_end_block, (int)raw_inode->i_reserved);

	brelse(bh);
	
	insert_inode_hash(inode);  //<linux/fs.h>
	mark_inode_dirty(inode);  //<linux/fs.h>
	*err = 0;
	printk(KERN_INFO "inode.c: ywy_new_inode end raw_inode->i_start_block = %d, i_end_block = %d, i_reserved = %d",\
		(int)raw_inode->i_start_block,(int)raw_inode->i_end_block, (int)raw_inode->i_reserved);
	return inode;
}

static const struct inode_operations ywy_symlink_inode_operations={
	.readlink       = generic_readlink,   //<linux/fs.h>
	.follow_link    = page_follow_link_light,  //<linux/fs.h>
	.put_link       = page_put_link,     //<linux/fs.h>         
};

void ywy_set_inode(struct inode *inode,dev_t rdev){
	//TODO:各种文件的操作调用
	if (S_ISREG(inode->i_mode)){
		printk(KERN_INFO "inode.c: ywy_set_inode regular file: inode->i_ino = %d\n", (int)inode->i_ino);
		inode->i_op = &ywy_file_inode_operations;
		inode->i_fop = &ywy_file_operations;
		inode->i_mapping->a_ops = &ywy_aops;   //i_mapping 是地址空间
	}else if(S_ISDIR(inode->i_mode)){
		printk(KERN_INFO "inode.c: ywy_set_inode dir: inode->i_ino = %d\n", (int)inode->i_ino);
		inode->i_op = &ywy_dir_inode_operations;
		inode->i_fop = &ywy_dir_operations;
		inode->i_mapping->a_ops = &ywy_aops;
	}else if(S_ISLNK(inode->i_mode)){
		printk(KERN_INFO "inode.c: ywy_set_inode dir: inode->i_ino = %d\n", (int)inode->i_ino);
		inode->i_op = &ywy_symlink_inode_operations;
		inode->i_mapping->a_ops = &ywy_aops;
	}else
		init_special_inode(inode, inode->i_mode, rdev); //<linux/fs.h>  特殊文件：字符，块，fifo，sock
}

/*
*读取磁盘上的ywy_inode结构,放入buffer_head中
*/
struct ywy_inode *ywy_raw_inode(struct super_block *sb, ino_t ino, struct buffer_head **p){
	int block = 0;
	int offset = 0;
	struct buffer_head *bh;
	struct ywy_sb_info *sbi = YWY_SB(sb);
	struct ywy_inode *ywy = NULL;
	
	printk(KERN_INFO "inode.c: ywy_raw_inode begin");
	if (!ino || ino > sbi->s_ys->s_inodes_count){
		printk(KERN_INFO "Bad inode number on dev %s: %ld is out of range\n", \
			sb->s_id, (long)ino);
		return NULL;
	}
	
	block = ((YWY_INODE_BLOCK - 1) + (ino - 1) / YWY_INODE_PER_BLOCK );   //计算ino所在块号
	offset = (ino - 1) % YWY_INODE_PER_BLOCK ;

	if (!(bh = sb_bread(sb, block))){
		printk(KERN_INFO "Unable to read inode block\n");
		return NULL;
	}
	ywy = (struct ywy_inode *)(bh->b_data) + offset;
	*p = bh;

        printk(KERN_INFO "inode.c: ywy_raw_inode end ino = %d", (int)ino);
	return ywy;
}

/*
*从磁盘上读取ywy_inode,填充inode结构
*/
struct inode *ywy_iget(struct super_block *sb, unsigned long ino){
	struct ywy_inode_info *yi;
	struct buffe_head *bh;
	struct ywy_inode *raw_inode;

	long ret = -EIO;
	
	printk(KERN_INFO "inode.c: ywy_iget begin");

	struct inode *inode = iget_locked(sb, ino);  //<linux/fs.h>  获取inode节点，没有则申请新的

	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW)){
		return inode;    //此索引节点原来就有
	}
	yi = YWY_I(inode);

	raw_inode = ywy_raw_inode(inode->i_sb, ino, &bh);  //将磁盘上ywy_inode读入buffer_head
	if (!raw_inode){
		iput(inode);
		return NULL;	
	}

	inode->i_mode = raw_inode->i_mode;
	inode->i_uid = raw_inode->i_uid;
	inode->i_gid = raw_inode->i_gid;
	inode->i_size = raw_inode->i_size;
	inode->i_nlink = raw_inode->i_nlinks;
	inode->i_mtime.tv_sec = inode->i_atime.tv_sec = inode->i_ctime.tv_sec = raw_inode->i_ctime;
	inode->i_mtime.tv_nsec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_ctime.tv_nsec = 0;

	yi->i_dtime = raw_inode->i_dtime;

	if(inode->i_nlink == 0 && (inode->i_mode == 0 || yi->i_dtime)){
		brelse(bh);
		ret = -ESTALE;
		goto bad_inode;
	}
	
	inode->i_blocks = raw_inode->i_blocks;
	yi->i_dtime = 0;
	yi->i_start_block = raw_inode->i_start_block;
	yi->i_end_block = raw_inode->i_end_block;
	yi->i_blocks = raw_inode->i_blocks;
	yi->i_reserved = raw_inode->i_reserved;

	ywy_set_inode(inode, inode->i_rdev);  //设置inode的各种操作
	brelse(bh);         //释放bh，前面ywy_raw_inode中创建了bh
	unlock_new_inode(inode);  //<linux/fs.h> 新创建的inode，将其I_NEW状态解锁

	printk(KERN_INFO "inode.c: ywy_i_get end inode = %d", (int)inode->i_ino);
	return inode;

bad_inode:
	iget_failed(inode);   //<linux/fs.h> 标记inode未创建死亡并释放
	return ERR_PTR(ret);
}

//逻辑块号映射物理块号
int ywy_get_block(struct inode *inode, sector_t iblock,struct buffer_head *bh, int create){
	int err = -EIO;
	
	//printk(KERN_INFO "get_block!!!!!!!!!!!!!");
	struct ywy_inode_info *yi = YWY_I(inode);

	printk(KERN_INFO "inode.c: ywy_get_block begin inode->ino = %d, sector_t iblock = %d, i_start_block = %d, i_block = %d, i_reserved = %d\n"\
		,(int)inode->i_ino, (int)iblock, (int)yi->i_start_block, (int)yi->i_blocks, (int)yi->i_reserved);
	
	if (iblock > (yi->i_blocks + yi->i_reserved)){
		printk("YWY-fs:function ywy_get_block block error");
		return err;
	}
	
	ywy_fsblk_t block = yi->i_start_block + iblock;

	if (block <= yi->i_end_block){
		map_bh(bh, inode->i_sb, le32_to_cpu(block));  //给bh和inode形成映射关系 <linux/buffer_head.h>
		printk(KERN_INFO "inode.c: ywy_get_block end block = %d\n", (int)block);
		return 0;
	}else if(!create){
		brelse(bh);
		return err;
	}else{
		set_buffer_new(bh);
		if(yi->i_reserved && yi->i_blocks){
			yi->i_reserved = yi->i_blocks + yi->i_reserved - iblock;
			yi->i_end_block = yi->i_start_block + iblock - 1;
			yi->i_blocks = iblock;
		}else
			yi->i_end_block = yi->i_start_block + iblock - 1;
		
		map_bh(bh, inode->i_sb, le32_to_cpu(block));
		mark_buffer_dirty_inode(bh, inode);  //<linux/buffer_head.h>
	}
	printk(KERN_INFO "inode.c: ywy_get_block end block = %d\n", (int)block);
	return 0; 
}

static int ywy_readpage(struct file *file, struct page *page){
	printk(KERN_INFO "inode.c: ywy_readpage");
	return block_read_full_page(page, ywy_get_block);
}

static int ywy_writepage(struct page *page, struct writeback_control *wbc){
	printk(KERN_INFO "inode.c: ywy_writepage");
	return block_write_full_page(page, ywy_get_block, wbc);	
}

int __ywy_write_begin(struct file *file, struct address_space *mapping, loff_t pos, unsigned len, unsigned flags, struct page *pagep, void **fsdata){
	//printk(KERN_INFO "inode.c: _ywy_write_begin , mapping = %p ,loff_t = %d,len = %d, flags = %d ,page = %p", \
		 mapping, (int)pos, (int)len, (int)flags, pagep);
	printk(KERN_INFO "inode.c: __ywy_write_begin");
	return block_write_begin(file, mapping, pos, len, flags, pagep, fsdata, ywy_get_block);
}

static int ywy_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags, struct page **pagep,
			void **fsdata){
	printk(KERN_INFO "inode.c: ywy_write_begin");
	*pagep = NULL;
	return __ywy_write_begin(file, mapping, pos, len, flags, pagep, fsdata);
}

static sector_t ywy_bmap(struct address_space *mapping, sector_t block){
	printk(KERN_INFO "inode.c: ywy_bmap");
	return generic_block_bmap(mapping, block, ywy_get_block);
}

const struct address_space_operations ywy_aops ={
	.readpage    = ywy_readpage,
	.writepage   = ywy_writepage,
	.sync_page   = block_sync_page,
	.write_begin = ywy_write_begin,  //负责准备绑定到此page的buffer_head结构
	.write_end   = generic_write_end,
	.bmap        = ywy_bmap,
};
