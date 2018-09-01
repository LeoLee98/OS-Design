#ifndef _OPERATION_H_
#define _OPERATION_H_

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <conio.h>
#include <windows.h>


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


//函数声明
void Prepare();													//登录系统前的准备工作,注册+安装
bool Format();													//格式化一个虚拟磁盘文件
bool Loading();													//安装文件系统，将虚拟磁盘文件中的关键信息如超级块读入到内存
void PrintSuperBlock();											//打印超级块信息
void PrintInodeBitmap();										//打印inode使用情况
void PrintBlockBitmap(int num = superblock->s_block_num);		//打印block使用情况
int	 BlockDistribute();													//磁盘块分配函数
bool BlockFree(int add);													//磁盘块释放函数
int  InodeDistrubute();													//分配i节点区函数
bool InodeFree(int add);													//释放i结点区函数
bool mkdir(int parinoAddr,char name[]);							//目录创建函数。参数：上一层目录文件inode地址 ,要创建的目录名
bool rmdir(int parinoAddr,char name[]);							//目录删除函数
bool create(int parinoAddr,char name[],char buf[]);				//创建文件函数
bool del(int parinoAddr,char name[]);							//删除文件函数 
void ls(int parinoaddr);										//显示当前目录下的所有文件和文件夹
void cd(int parinoAddr,char name[]);							//进入当前目录下的name目录
void gotoxy(HANDLE hOut, int x, int y);							//移动光标到指定位置
void vi(int parinoaddr,char name[],char buf[]);					//模拟一个简单vi，输入文本
void writefile(Inode fileInode,int fileInodeAddr,char buf[]);	//将buf内容写回文件的磁盘块
void inUsername(char username[]);								//输入用户名
void inPasswd(char passwd[]);									//输入密码
bool login();													//登陆界面
bool check(char username[],char passwd[]);						//核对用户名，密码
void gotoRoot();												//回到根目录
void logout();													//用户注销
bool useradd(char username[]);									//用户注册
bool userdel(char username[]);									//用户删除
void chmod(int parinoAddr,char name[],int pmode);				//修改文件或目录权限
void touch(int parinoAddr,char name[],char buf[]);				//touch命令创建文件，读入字符
void help();													//显示所有命令清单
int CheckDir(int parinoAddr,char name[],int flag,int &posi,int &posj);   //检查该目录底下是否有相同名字的对应flag属性的文件或者目录，flag 0代表文件，1代表目录 ，返回值-1就是拉闸，正常不冲突就是0
void cmd(char str[]);											//处理输入的命令
#endif