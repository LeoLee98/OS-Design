#ifndef _OPERATION_H_
#define _OPERATION_H_

#include"common.h"
#include"struct.h"

//函数声明
void Prepare();													//登录系统前的准备工作,注册+安装
bool Format();													//格式化一个虚拟磁盘文件
bool Loading();													//安装文件系统，将虚拟磁盘文件中的关键信息如超级块读入到内存
void PrintSuperBlock();											//打印超级块信息
void PrintInodeBitmap();										//打印inode使用情况
void PrintBlockBitmap(int num = superblock->s_block_num);		//打印block使用情况
int	 BlockDistribute();													//磁盘块分配函数
bool BlockFree();													//磁盘块释放函数
int  InodeDistrubute();													//分配i节点区函数
bool InodeFree();													//释放i结点区函数
bool mkdir(int parinoAddr,char name[]);							//目录创建函数。参数：上一层目录文件inode地址 ,要创建的目录名
bool rmdir(int parinoAddr,char name[]);							//目录删除函数
bool create(int parinoAddr,char name[],char buf[]);				//创建文件函数
bool del(int parinoAddr,char name[]);							//删除文件函数 
void ls(int parinoaddr);										//显示当前目录下的所有文件和文件夹
void cd(int parinoaddr,char name[]);							//进入当前目录下的name目录
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
//void chmod(int parinoAddr,char name[],int pmode);				//修改文件或目录权限
//void touch(int parinoAddr,char name[],char buf[]);				//touch命令创建文件，读入字符
void help();													//显示所有命令清单

void cmd(char str[]);											//处理输入的命令
#endif