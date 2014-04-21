#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/smp_lock.h>
#include <linux/buffer_head.h>
#include "ywy.h"

//解除映射，释放page
inline void ywy_put_page(struct page *page){
        kunmap(page);   //<linux/highmem.h>  接触映射关系，实际为空？
        page_cache_release(page);  //<linux/pagemap.h> 实际调用put_pagei(<linux.h/mm.h>)释放内存页
	//printk(KERN_INFO "dir.c: ywy_put_page");
}

//计算dir占用的page个数
static inline unsigned long ywy_dir_pages(struct inode *inode){
	//printk(KERN_INFO "dir.c: ywy_dir_pages");
	return (inode->i_size + PAGE_CACHE_SIZE - 1)>> PAGE_CACHE_SHIFT;  //<linux/pagemap>中声明的
}

//获取内存中inode的指定页地址
static struct page *ywy_get_page(struct inode *dir, unsigned long n){
	struct address_space *mapping = dir->i_mapping;
	struct page *page = read_mapping_page(mapping, n, NULL); //<linux/pagemap.h> 调用map的a_ops->readpage来读取页
	printk(KERN_INFO "dir.c: ywy_get_page call a_ops->readpage");
	if(!IS_ERR(page)){
		kmap(page);   //<linux/highmem.h> 如果是低端物理地址，返回对应虚拟内存，如果是高端物理内存则映射
		if(!PageUptodate(page))  //<linux/page-flags.h>  确保page中数据和磁盘中数据是一致的，新建的page不是uptodate的
			goto fail;
	}
	return page;

fail:
	ywy_put_page(page);
	return ERR_PTR(-EIO);
}

//返回inode在各个页中的最后一个字段，一般是PAGE_CACHE_SIZE，最后一页可能小于
static unsigned ywy_last_byte(struct inode *inode, unsigned long page_nr){
	unsigned last_byte = inode->i_size;

	//printk(KERN_INFO "dir.c: ywy_last_byte");
	
	last_byte -= page_nr << PAGE_CACHE_SHIFT;
	if (last_byte > PAGE_CACHE_SIZE)
		last_byte = PAGE_CACHE_SIZE;
	return last_byte;
	
}

//跳转到下一个entry地址
static inline void *ywy_next_entry(void *de){
	//printk(KERN_INFO "dir.c: ywy_next_entry");
	return (void *)((char *)de + sizeof(struct ywy_dir_entry));
}

static inline int namecompare(int len, int maxlen, const char *name, const char *buffer){
	//printk(KERN_INFO "dir.c: namecompare");
	if(len < maxlen && buffer[len])
		return 0;
	return !memcmp(name, buffer, len);  //<linux/string.h>
}


//在目录中找目录项，没有则生成一个
struct ywy_dir_entry *ywy_find_entry(struct dentry *dentry, struct page **res_page){
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	struct inode *dir = dentry->d_parent->d_inode;
	
	unsigned long n;
	unsigned long npages = ywy_dir_pages(dir);
	struct page *page = NULL;
	char *p;

	char *namx;
	__u32 inumber;
	*res_page = NULL;
	
	printk(KERN_INFO "dir.c: ywy_find_entry begin");

	for(n = 0; n < npages ; n++){
		char *kaddr, *limit;
		page = ywy_get_page(dir, n);  //获取到父目录代表文件在内存中第几页
		if(IS_ERR(page))
			continue;
		kaddr = (char *)page_address(page);  //
		limit = kaddr + ywy_last_byte(dir, n) - sizeof(struct ywy_dir_entry);
		for(p = kaddr; p <= limit; p=ywy_next_entry(p)){
			struct ywy_dir_entry *de = (struct ywy_dir_entry*)p;
			namx = de->name;
			inumber = de->ino;
			if(!inumber) //如果该目录项没有使用过跳过
				continue;
			if(namecompare(namelen, YWY_NAME_LEN, name, namx))// 判断是否匹配
				goto found;
		}
		ywy_put_page(page);
	}
	return NULL;

found:
	*res_page = page;
	printk(KERN_INFO "dir.c: ywy_find_entry end de->ino = %d", (int)((struct ywy_dir_entry *)p)->ino);
	return (struct ywy_dir_entry *)p;
	
}

static int ywy_readdir(struct file *filp, void *dirent, filldir_t filldir){
	loff_t pos = filp->f_pos;

	struct inode *inode = filp->f_path.dentry->d_inode;

	unsigned int offset = pos & ~PAGE_CACHE_MASK;  //offset 是在页中的偏移量
	unsigned long n = pos >> PAGE_CACHE_SHIFT;  //n指读到第几页
	unsigned long npages = ywy_dir_pages(inode); //指该文件在内存中一共占有几页
	unsigned chunk_size = sizeof(struct ywy_dir_entry);

	printk(KERN_INFO "dir.c: ywy_readdir begin");

	lock_kernel();   //<linux/smp_lock.h>锁内核
	pos = (pos + chunk_size - 1)& ~(chunk_size - 1);  //将目前读到的位置调整到目录项开始的位置
	if(pos >= inode->i_size)
		goto done;
	for(; n < npages; n++, offset = 0){
		char *kaddr, *limit;
		char *p;

		struct page *page = ywy_get_page(inode, n);  //获取第n页
		
		if(IS_ERR(page)){
			printk(KERN_INFO "page is error\n");
			continue;
		}
		
		kaddr = (char *)page_address(page);   //<linux/mm.h>
		p = (kaddr + offset);
		
		limit = kaddr +  ywy_last_byte(inode, n) - chunk_size;  //边界
		for(; p <= limit; p = ywy_next_entry(p)){
			struct ywy_dir_entry *de = (struct ywy_dir_entry *)p;

			if(de->ino){  //目录项有对应索引节点号
				offset = p - kaddr;  //页内偏移
				unsigned name_len = strnlen(de->name, YWY_NAME_LEN);
				unsigned char d_type = DT_UNKNOWN;  //<linux/fs.h>文件类型
				//调用vfs函数来填充dentry结构
				int over = filldir(dirent, de->name, name_len, (n<<PAGE_CACHE_SHIFT) | offset, le32_to_cpu(de->ino), d_type);
					if(over){
						ywy_put_page(page); //若错误释放该页，跳转到done
						goto done;
					}
			}
		}
		ywy_put_page(page);
	}
done:
	filp->f_pos = (n << PAGE_CACHE_SHIFT) | offset;  //调整文件偏移
	unlock_kernel();
	printk(KERN_INFO "dir.c: ywy_readdir end filp->name = %s \n", filp->f_path.dentry->d_name.name);
	return 0;
}

static int ywy_commit_chunk(struct page *page, loff_t pos, unsigned len){
	struct address_space *mapping = page->mapping;
	struct inode *dir = mapping->host;
	int err = 0;
	printk(KERN_INFO "dir.c: ywy_commit_chunk");
	block_write_end(NULL, mapping, pos, len, len, page, NULL);  //完成page绑定到buffer_head
	
	if(pos + len > dir->i_size){
		i_size_write(dir, pos + len);//如果大小改变了，重写inode大小
		mark_inode_dirty(dir);  //<linux/fs.h>
	}
	if(IS_DIRSYNC(dir)){
		err = write_one_page(page, 1); //<linux/mm.h>  等待page回写到磁盘，写完再解锁，这步完成了数据就写入磁盘了
		//printk(KERN_INFO "sasasasai err = %d",(int)err);
	}else{ 
		unlock_page(page);
		//printk(KERN_INFO "vdvdvd err = %d",(int)err);
	}
	return err;
}

//目录项写进父目录
int ywy_add_link(struct dentry *dentry, struct inode *inode){
	struct inode *dir = dentry->d_parent->d_inode;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;

	printk(KERN_INFO "dir.c: ywy_add_link begin");
	
	struct page *page = NULL;
	unsigned long npages = ywy_dir_pages(dir);
	unsigned long n;
	char *kaddr, *p;
	int ino;

	struct ywy_dir_entry *de;
	loff_t pos;
	int err;
	char *namx = NULL;
	__u32 inumber;
	for(n = 0; n<=npages; n++){
		char *limit, *dir_end;
		page = ywy_get_page(dir, n);
		err = PTR_ERR(page);
		if(IS_ERR(page))
			goto out;
		lock_page(page);
		kaddr = (char *)page_address(page); //<linux/mm.h>
		dir_end = kaddr + ywy_last_byte(dir ,n); //该页内未被使用的空间地址
		limit = kaddr + PAGE_CACHE_SIZE - sizeof(struct ywy_dir_entry);
		for(p = kaddr; p<= limit; p = ywy_next_entry(p)){
			de = (struct ywy_dir_entry *)p;
			namx = de->name;
			inumber = de->ino;
			if(p == dir_end) //如果走到最后一个页的目录项结尾，在此处插入新的目录项
				goto got_it;
			if(!inumber) //中途发现某个目录项未使用，对应索引节点号为0（被删除的）
				goto got_it;

			err = -EEXIST;  //新建文件在父目录中已存在
			if(namecompare(namelen, YWY_NAME_LEN, name, namx))
				goto out_unlock;
			ino = de->ino;
		}
		unlock_page(page);
		ywy_put_page(page);
	}	

	BUG();
	return -EINVAL;
got_it://准备开始写，pos为页内偏移，在pos处写
	pos = (page->index<<PAGE_CACHE_SHIFT) + p - (char *)page_address(page);
	err = __ywy_write_begin(NULL, page->mapping, pos, sizeof(struct ywy_dir_entry), AOP_FLAG_UNINTERRUPTIBLE, &page, NULL);
	if(err)
		goto out_unlock;
	memcpy(namx, name, namelen);
	memset(namx + namelen, 0, YWY_NAME_LEN - namelen); //多余空间清零
	de->ino = inode->i_ino;
	err = ywy_commit_chunk(page, pos, sizeof(struct ywy_dir_entry));
	dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;
	mark_inode_dirty(dir);  //<linux/fs.h>
	printk(KERN_INFO "dir.c: ywy_add_link end page->index = %d,p = %p, adress = %s, pos = %d", (int)page->index, p, (char*)page_address(page), (int)pos);
out_put:
	ywy_put_page(page);
out:
	return err;
out_unlock:
	unlock_page(page);
	goto out_put;
}

int ywy_delete_entry(struct ywy_dir_entry *de, struct page *page){
	struct address_space *mapping = page->mapping;
	struct inode *inode =(struct inode*)mapping->host;
	char *kaddr = page_address(page); //<linux/mm.h> 页的虚拟地址
	loff_t pos = page_offset(page) + (char *)de - kaddr;  //<linux/pagemap.h> 计算page地址，语句是计算在inode中的偏移量
	unsigned len = sizeof(struct ywy_dir_entry);
	int err;
	
	printk(KERN_INFO "dir.c: ywy_delete_entry begin inode->i_ino = %d\n", (int)inode->i_ino);
	
	lock_page(page);
	err = __ywy_write_begin(NULL, mapping, pos, len, AOP_FLAG_UNINTERRUPTIBLE, &page, NULL); //<linux/fs.h> 0x001 not do a short write
	if(err == 0){
		de->ino = 0;  //不删除文件夹，只置零，和ywy_add_link对应
		err = ywy_commit_chunk(page, pos, len);
	}else
		unlock_page(page);
	ywy_put_page(page);
	inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
	mark_inode_dirty(inode); //<linux/fs.h>

	printk(KERN_INFO "dir.c: ywy_delete_entry end");

	return err;	
}

//判断文件夹是否空文件夹
int ywy_empty_dir(struct inode* inode){
	struct page *page = NULL;
	unsigned long i, npages = ywy_dir_pages(inode);
	char *name;
	__u32 inumber;
	printk(KERN_INFO "dir.c: ywy_empty_dir begin inode->i_ino = %d\n", (int)inode->i_ino);
	for(i = 0; i< npages; i++){
		char *p, *kaddr, *limit;

		page = ywy_get_page(inode, i);
		if(IS_ERR(page))
			continue;

		kaddr = (char*)page_address(page); //<linux/mm.h>
		limit = kaddr + ywy_last_byte(inode, i) - sizeof(struct ywy_dir_entry);
		for(p = kaddr; p <= limit; p = ywy_next_entry(p)){
			struct ywy_dir_entry *de = (struct ywy_dir_entry *)p;
			name = de->name;
			inumber = de->ino;
			if(inumber != 0 ){
				if(name[0] != '.'){
					goto not_empty;
				}
				if(!name[1]){
					if(inumber != inode->i_ino)
						goto not_empty;
				}else if(name[1] != '.'){
					goto not_empty;
				}else if(name[2]){
					goto not_empty;
				}
			}
		}
		ywy_put_page(page);
	}
	printk(KERN_INFO "dir.c: ywy_empty_dir end");
	return 1;

not_empty:
	printk(KERN_INFO "dir.c: ywy_empty_dir end");
	ywy_put_page(page);
	return 0;

}

//创建新的空文件夹，创建.和..文件
int ywy_make_empty(struct inode *inode, struct inode *dir)
{
	struct address_space *mapping = inode->i_mapping;
	struct page *page = grab_cache_page(mapping, 0);  //<linux/pagemap.h> 申请一些需要用的页
	
	printk(KERN_INFO "dir.c: ywy_make_empty begin inode->i_ino = %d \n", (int)inode->i_ino);

	char *kaddr;
	int err = -ENOMEM;
	
	if(!page)
		return err;
	//两个文件夹. 和 ..
	err = __ywy_write_begin(NULL, mapping, 0, 2*sizeof(struct ywy_dir_entry), AOP_FLAG_UNINTERRUPTIBLE, &page, NULL);
	if(err){
		unlock_page(page);   //<linux/pagemap.h>
		goto fail;
	}
	
	kaddr = kmap_atomic(page, KM_USER0); //<linux/highmem.h> 实现page到vaddr转换，相比kmap性能更高，不加锁
	memset(kaddr, 0, PAGE_CACHE_SIZE);
	
	struct ywy_dir_entry *de = (struct ywy_dir_entry *)kaddr;
	de->ino = inode->i_ino;
	strcpy(de->name, ".");
	de = ywy_next_entry(de);
	de->ino = dir->i_ino;
	strcpy(de->name, "..");

	kunmap_atomic(kaddr, KM_USER0);  //<linux/highmem.h>

	err = ywy_commit_chunk(page, 0, 2 * sizeof(struct ywy_dir_entry));
fail:
	printk(KERN_INFO "dir.c: ywy_make_empty end");
	page_cache_release(page);
	return err;
		
}

const struct file_operations ywy_dir_operations = {
	.llseek   = generic_file_llseek,
	.read     = generic_read_dir,
	.readdir  = ywy_readdir,
	.fsync	  = ywy_sync_file,  //file.c
};
