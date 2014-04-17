========================================
ywy-fs    一个实验性质的文件系统
========================================

AUTHOR
========================================
name： 杨万元  

email：ywy8861@126.com

github： https://github/yangwanyuan

DESCRIPTION
========================================
开发linux内核版本： 2.6.34.14

ywy-fs是一个实验性质文件系统，基本实现了一个文件系统的mount，创建删除文件夹，文件，读写文件等基本功能。

文件系统组织结构：硬盘分为super块，inode表块，数据块
文件顺序存储，每个文件只可以占用10个块太小，即1M大小，最后一个文件支持无限写入

HOW TO USE
========================================
build：

  1.拷贝ywy_fs.h文件到/usr/src/linux-*/include/linux下

  2.在/usr/src/linux-*/include/linux/magic.h中加入文件系统magic数
  
    #define YWY_SUPER_MAGIC    0x1214
  
  3.编译工程
  
    $make

use：

  1.创建img文件
  
    $dd if=/dev/zero of=/home/ywyfs.img bs=1M count=1024
   
  2.格式化镜像文件，使用工具https://github.com/yangwanyuan/mkfs-ywy中mkfs.ywy执行文件
    
    $mkfs.ywy /home/ywyfs.img
  
  3.挂载ywy-fs内核模块
    
    $insmod ./ywy.ko
    
  4.挂载设备
    
    $mount -t ywy -o loop /home/ywyfs.img /mnt
    
开始尽情的玩耍吧！！！：-）

欢迎提bug！！
