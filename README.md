# 二级文件系统
项目开发大概用了6天的时间，因为没有仔细debug，其中bug很多，拿来玩一玩还是很不错的。

> 代码量统计如下,大概是2000行所有：


## 该项目是实现了一个仿Linux 的二级文件系统，实现思路如下：
1. 首先使用系统的dd命令创建一个大小为1G的空文件进行模拟磁盘；
2. 创建超级块superBlock，数据结构如下（其作用是系统启动时，加载进内存对文件系统进行识别）：
```
//声明superblock 数据结构
struct SuperBlock {
	//inode 数量 最多65536
	unsigned int s_Inode_num;
	//block number最多4294967294 
	unsigned int s_block_num;

    //原本是准备用于成组链接法，但因时间不足，就没有使用，基本没有作用
	unsigned int s_free_Inode_num;
	unsigned int s_free_block_num;
	int s_free_data_block_addr;
	int s_free[K_BLOCKS_PER_GROUP];
	
    //一个块大小
	unsigned int s_block_size;
    // Inode 项大小
	unsigned int s_Inode_size;
    //超级块大小
	unsigned int s_superblock_size;
    //每组多少块，成组链接法
	unsigned int s_blocks_per_group;
	

	//磁盘分布
	int s_superblock_startAddr;
	int s_InodeBitmap_startAddr;
	int s_blockBitmap_startAddr;
	int s_Inode_startAddr;
	int s_data_block_startAddr;
};
```

3. 创建Inode结构：
```
struct Inode {
	unsigned short i_Inode_num;
	unsigned short i_mode;//，确定了文件的类型，以及它的所有者、它的group、其它用户访问此文件的权限。
	unsigned short i_counter; //总共有几个目录项
	char i_uname[20];
	char i_gname[20];
	unsigned int i_size;//该Inode指向的文件大小，如果是文件的话
	
	time_t i_ctime; //Inode创建时间
	time_t i_mtime; //上一次修改Inode指向的文件的时间
	time_t i_atime; //最后一次访问Inode指向的文件的时间
	int i_dirBlock[12]; //这是inode直接指向的12 物理块， 也就是12*4 = 48 KB
	int i_indirBlock_1; //这是一级间接块，也就是用一个block来记录该文件剩下存储的block地址，一个地址4bytes， 所以是： 4KB / 4 * 4KB = 4M
	//合起来单个文件最大为 4096 + 48KB
};
```
4. 声明目录项：
```
struct DirItem {
    //目录名
	char itemName[K_MAX_NAME_SIZE];
    //该目录项的Inode地址
	int inode_addr;
};
```
5. 磁盘中的空闲块，与空闲Inode 采用位图的形式来进行管理
6. 磁盘的基本分布情况如下：
superBlock的内容 | Inode Bitmap | Block Bitmap | Inode  | data block



