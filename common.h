#ifndef _COMMON_H_
#define _COMMON_H_

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <conio.h>
#include <windows.h>
#include<unistd.h> 


#define BLOCK_SIZE	512						//块号大小为512B
#define INODE_SIZE	128						//inode节点大小为128B。注：sizeof(Inode)不能超过该值
#define MAX_NAME_SIZE 28					//最大名字长度

#define INODE_NUM	640						//inode节点数,最多64个文件
#define BLOCK_NUM	10240					//块号数，10240 * 512B = 5120KB
#define BLOCKS_PER_GROUP	64				//空闲块堆栈大小，一个空闲堆栈最多能存多少个磁盘块地址

#define MODE_DIR	01000					//目录标识
#define MODE_FILE	00000					//文件标识
#define OWNER_R	4<<6						//本用户读权限 （左移符号）         777（user group others）
#define OWNER_W	2<<6						//本用户写权限
#define OWNER_X	1<<6						//本用户执行权限
#define GROUP_R	4<<3						//组用户读权限
#define GROUP_W	2<<3						//组用户写权限
#define GROUP_X	1<<3						//组用户执行权限
#define OTHERS_R	4						//其它用户读权限
#define OTHERS_W	2						//其它用户写权限
#define OTHERS_X	1						//其它用户执行权限
#define FILE_DEF_PERMISSION 0664			//文件默认权限
#define DIR_DEF_PERMISSION	0755			//目录默认权限

#define FILESYSNAME	"OS.sys"			//虚拟磁盘文件名

//全局变量声明
extern SuperBlock *superblock;
extern const int Inode_StartAddr;
extern const int Superblock_StartAddr;		//超级块 偏移地址,占一个磁盘块
extern const int InodeBitmap_StartAddr;		//inode位图 偏移地址，占两个磁盘块，最多监控 1024 个inode的状态
extern const int BlockBitmap_StartAddr;		//block位图 偏移地址，占二十个磁盘块，最多监控 10240 个磁盘块（5120KB）的状态
extern const int Inode_StartAddr;			//inode节点区 偏移地址，占 INODE_NUM/(BLOCK_SIZE/INODE_SIZE) 个磁盘块
extern const int Block_StartAddr;			//block数据区 偏移地址 ，占 INODE_NUM 个磁盘块
extern const int File_Max_Size;				//单个文件最大大小
extern const int Sum_Size;					//虚拟磁盘文件大小


//全局变量声明
extern int RootDir_Inode_Addr;					//根目录inode地址
extern int CurDir_Inode_Addr;					//当前目录
extern char Cur_Dir_Name[310];				//当前目录名
extern char Cur_Host_Name[110];				//当前主机名
extern char Cur_User_Name[110];				//当前登陆用户名
extern char Cur_Group_Name[110];			//当前登陆用户组名
extern char Cur_User_Dir_Name[310];			//当前登陆用户目录名

extern int nextUID;							//下一个要分配的用户标识号
extern int nextGID;							//下一个要分配的用户组标识号

extern bool isLogin;						//是否有用户登陆

extern FILE* fw;							//虚拟磁盘文件 写文件指针
extern FILE* fr;							//虚拟磁盘文件 读文件指针
extern SuperBlock *superblock;				//超级块指针
extern bool inode_bitmap[INODE_NUM];		//inode位图
extern bool block_bitmap[BLOCK_NUM];		//磁盘块位图

extern char buffer[10000000];				//10M，缓存整个虚拟磁盘文件

#endif
