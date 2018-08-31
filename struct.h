#ifndef _STRUCT_H_
#define _STRUCT_H_

#include "common.h"
struct SuperBlock  //38 Byte
{
    unsigned short s_inode_num ; //总inode 数目  65535  
    unsigned int   s_block_num ; //总的block数目 4294967294

    unsigned short s_free_inode_num; //空闲的inode数目
    unsigned int   s_free_block_num; //空闲block数目

    unsigned short s_superblock_size; //超级快大小
    unsigned short s_block_size;    //磁盘块大小
    unsigned short s_inode_size;    //inode 大小

    int s_superblock_address ; //偏移地址
    int s_inodemap_address ;
    int s_blockmap_address ; 
    int s_inode_address ;
    int s_block_address ;

};

struct Inode 
{
    unsigned short i_inode_id ; //id
    unsigned short i_mode ; //权限 rwx
    unsigned short i_cnt ;//硬连接数
    
    char i_user_name[20] ; //所属用户的id
    char i_group_name[20] ; //所属用户组的id

    unsigned int i_file_size ;             //文件大小
    time_t  i_inodechange_time;						//inode上一次变动的时间
	time_t  i_filechange_time;						//文件内容上一次变动的时间
	time_t  i_fileopen_time;						//文件上一次打开的时间
	int i_dirBlock[10];						//10个直接块。10*512B = 5120B = 5KB
	int i_indirBlock_1;						//一级间接块。512B/4 * 512B = 128 * 512B = 64KB （一个间接块，在32位（4字节）的系统下，512B的磁盘块大小能保存512/4个磁盘块的地址，所以，一个间接块能表示的空间大小）
};

struct DirectoryItem //32B  16 perblock
{
    char d_item_name [MAX_NAME_SIZE] ;
    int d_inode_address ;
};
#endif