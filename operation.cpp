#include"operation.h"

//全局变量定义
const int Superblock_StartAddr = 0;																//超级块 偏移地址,占一个磁盘块
const int InodeBitmap_StartAddr = 1*BLOCK_SIZE;													//inode位图 偏移地址，占两个磁盘块，最多监控 1024 个inode的状态（存在文件中的一个位（用的int存）就是一个字节，所以1024B只能存1024位）
const int BlockBitmap_StartAddr = InodeBitmap_StartAddr + 2*BLOCK_SIZE;							//block位图 偏移地址，占二十个磁盘块，最多监控 10240 个磁盘块（5120KB）的状态
const int Inode_StartAddr = BlockBitmap_StartAddr + 20*BLOCK_SIZE;								//inode节点区 偏移地址，占 INODE_NUM/(BLOCK_SIZE/INODE_SIZE) 个磁盘块
const int Block_StartAddr = Inode_StartAddr + INODE_NUM/(BLOCK_SIZE/INODE_SIZE) * BLOCK_SIZE;	//block数据区 偏移地址 ，占 INODE_NUM 个磁盘块

const int Sum_Size = Block_StartAddr + BLOCK_NUM * BLOCK_SIZE;									//虚拟磁盘文件大小

//单个文件最大大小
const int File_Max_Size =	10*BLOCK_SIZE +														//10个直接块
							BLOCK_SIZE/sizeof(int) * BLOCK_SIZE +								//一级间接块（一个地址是一个Int,用Int来存地址值）
							(BLOCK_SIZE/sizeof(int))*(BLOCK_SIZE/sizeof(int)) * BLOCK_SIZE;		//二级间接块

int RootDir_Inode_Addr;							//根目录inode地址
int CurDir_Inode_Addr;							//当前目录
char Cur_Dir_Name[310];						//当前目录名
char Cur_Host_Name[110];					//当前主机名
char Cur_User_Name[110];					//当前登陆用户名
char Cur_Group_Name[110];					//当前登陆用户组名
char Cur_User_Dir_Name[310];				//当前登陆用户目录名

int nextUID;								//下一个要分配的用户标识号
int nextGID;								//下一个要分配的用户组标识号

bool isLogin;								//是否有用户登陆

FILE* fw;									//虚拟磁盘文件 写文件指针
FILE* fr;									//虚拟磁盘文件 读文件指针
SuperBlock *superblock = new SuperBlock;	//超级块指针
bool inode_bitmap[INODE_NUM];				//inode位图
bool block_bitmap[BLOCK_NUM];				//磁盘块位图

char buffer[10000000] = {0};				//10M，缓存整个虚拟磁盘文件

void Ready()
{
    
    //初始化用户信息
    nextUID = 0;
    nextGID = 0;
    isLogin = false ;
    strcpy(Cur_User_Name,"root");
    strcpy(Cur_Group_Name,"root");

    //获取主机名字，这步可以不重要
    memset(Cur_Host_Name,0,sizeof(Cur_Host_Name));
    DWORD k = 100;
    GetComputerName(Cur_Host_Name,&k); 

    //获取根目录等inode地址信息
    RootDir_Inode_Addr = Inode_StartAddr;
    CurDir_Inode_Addr = RootDir_Inode_Addr ;
    strcpy(Cur_Dir_Name,"/");

    char c;
	printf("是否格式化?[y/n]");
	while(c = getch()){
		fflush(stdin);
		if(c=='y'){
			printf("\n");
			printf("文件系统正在格式化……\n");
			if(!Format()){
				printf("文件系统格式化失败\n");
				return ;
			}
			printf("格式化完成\n");
			break;
		}
		else if(c=='n'){
			printf("\n");
			break;
		}
	}

	//printf("载入文件系统……\n");
	if(!Loading()){			
		printf("安装文件系统失败\n");
		return ;
	}
	//printf("载入完成\n");
}

//第一次登陆的时候才需要格式化，之后登陆直接使用，按照格式化中的空间分配函数定义，根节点的空间都是位于inode区和block区的第一个
bool Format()
{
    //初始化超级块信息，因为这些信息都是固定的
    superblock->s_inode_num = INODE_NUM;
    superblock->s_block_num = BLOCK_NUM;
    superblock->s_free_inode_num  = INODE_NUM;
    superblock->s_free_block_num = BLOCK_NUM;
    superblock->s_superblock_size = sizeof(SuperBlock);
    superblock->s_block_size = BLOCK_SIZE;
    superblock->s_inode_size = INODE_SIZE;
    superblock->s_superblock_address = Superblock_StartAddr;
    superblock->s_inodemap_address = InodeBitmap_StartAddr;
    superblock->s_blockmap_address  = BlockBitmap_StartAddr;
    superblock->s_inode_address = Inode_StartAddr;
    superblock->s_block_address = Block_StartAddr;

    //初始化位图
    memset(inode_bitmap,0,sizeof(inode_bitmap));
    fseek(fw,InodeBitmap_StartAddr,SEEK_SET);//移动指针位置（int fseek(FILE *stream, long offset, int fromwhere);函数设置文件指针stream的位置。如果执行成功，stream将指向以fromwhere为基准，偏移offset（指针偏移量）个字 .）
	fwrite(inode_bitmap,sizeof(inode_bitmap),1,fw); //把内容写到file stream中去

    memset(block_bitmap,0,sizeof(block_bitmap));
    fseek(fw,BlockBitmap_StartAddr,SEEK_SET);
    fwrite(block_bitmap,sizeof(block_bitmap),1,fw);

    //把超级块内容写回去
    fseek(fw,Superblock_StartAddr,SEEK_SET);
    fwrite(superblock,sizeof(SuperBlock),1,fw);

    fflush(fw);
    //至此完成了虚拟磁盘文件的开启初始化，以下就进行其他变量初始化

    //为系统创建根目录
    Inode root ;
    
    //分配空间
    int root_inode_address = InodeDistrubute();
    int root_block_address = BlockDistribute();

    //为根目录添加.这个目录项，其他目录还会再有..这个项
    DirectoryItem root_DirectoryItem[16] = {0};
    strcpy(root_DirectoryItem[0].d_item_name,".");
    root_DirectoryItem[1].d_inode_address = root_inode_address;

    fseek(fw,root_block_address,SEEK_SET);
    fwrite(root_DirectoryItem,sizeof(root_DirectoryItem),1,fw);

    //为根目录Inode初始化
    root.i_inode_id = 0;
    root.i_mode  = MODE_DIR | DIR_DEF_PERMISSION;
    root.i_cnt = 1;

    //这里这两个当前变量在开始format之前就要初始化了
    strcpy(root.i_user_name,Cur_User_Name);
    strcpy(root.i_group_name,Cur_Group_Name);

    root.i_file_size = superblock->s_block_size;

    root.i_inodechange_time = time(NULL);
    root.i_filechange_time = time(NULL);
    root.i_fileopen_time  = time(NULL);

    root.i_dirBlock[0] = root_block_address ;//直接快地址数组
    root.i_indirBlock_1 = -1 ; //还没启用间接块，所以地址为空

    fseek(fw,Inode_StartAddr,SEEK_SET);
    fwrite(&root,sizeof(Inode),1,fw);
    fflush(fw);

    /*接下来就是创建home目录，然后Home目录下有很多和用户名字相同的文件夹，
             然后用户注册的时候就会生成两个文件，一个是etc下的passwd文件，记录了用户的名字和密码
             还有一个是shadow文件，记录了用户的属性之类的信息，
             然后登陆的时候，在etc/passwd中，直接按照这个名字匹配，cd到这个用户的文件夹 */

    //创建目录及配置文件
	mkdir(RootDir_Inode_Addr,"home");	//用户目录
	cd(RootDir_Inode_Addr,"home");
	mkdir(CurDir_Inode_Addr,"root");	

	cd(CurDir_Inode_Addr,"..");
	mkdir(CurDir_Inode_Addr,"etc");	//配置文件目录
	cd(CurDir_Inode_Addr,"etc");

	char buf[1000] = {0};
	
	sprintf(buf,"root:x:%d:%d\n",nextUID++,nextGID++);	//增加条目，用户名：加密密码：用户ID：用户组ID
	create(CurDir_Inode_Addr,"passwd",buf);	//创建用户信息文件

	sprintf(buf,"root:root\n");	//增加条目，用户名：密码
	create(CurDir_Inode_Addr,"shadow",buf);	//创建用户密码文件
	chmod(CurDir_Inode_Addr,"shadow",0660);	//修改权限，禁止其它用户读取该文件

	sprintf(buf,"root::0:root\n");	//增加管理员用户组，用户组名：口令（一般为空，这里没有使用）：组标识号：组内用户列表（用，分隔）
	sprintf(buf+strlen(buf),"user::1:\n");	//增加普通用户组，组内用户列表为空
	create(CurDir_Inode_Addr,"group",buf);	//创建用户组信息文件

	cd(CurDir_Inode_Addr,"..");	//回到根目录

	return true;
        
             



    
}

//主要负责加载虚拟磁盘中的必要内容如：超级块，inode位图，block位图，主目录，etc等内容到内存中
bool Loading()
{
    fseek(fr,Superblock_StartAddr,SEEK_SET);
    fread(superblock,sizeof(SuperBlock),1,fr);

    fseek(fr,InodeBitmap_StartAddr,SEEK_SET);
    fread(inode_bitmap,sizeof(inode_bitmap),1,fr);

    fseek(fr,BlockBitmap_StartAddr,SEEK_SET);
    fread(block_bitmap,sizeof(block_bitmap),1,fr);

    return true;
}

void PrintSuperBlock()
{
    printf("\n");
	printf("空闲inode数 / 总inode数 ：%d / %d\n",superblock->s_free_inode_num,superblock->s_inode_num);
	printf("空闲block数 / 总block数 ：%d / %d\n",superblock->s_free_block_num,superblock->s_block_num);
	printf("本系统 block大小：%d 字节，每个inode占 %d 字节（真实大小：%d 字节）\n",superblock->s_block_size,superblock->s_inode_size,sizeof(Inode));
	printf("\t超级块占 %d 字节（真实大小：%d 字节）\n",superblock->s_block_size,superblock->s_superblock_size);
	printf("磁盘分布：\n");
	printf("\t超级块开始位置：%d B\n",superblock->s_superblock_address);
	printf("\tinode位图开始位置：%d B\n",superblock->s_inodemap_address);
	printf("\tblock位图开始位置：%d B\n",superblock->s_blockmap_address);
	printf("\tinode区开始位置：%d B\n",superblock->s_inode_address);
	printf("\tblock区开始位置：%d B\n",superblock->s_block_address);
	printf("\n");

	return ;
}

void PrintInodeBitmap()	//打印inode使用情况
{
	printf("\n");
	printf("inode使用表：[uesd:%d %d/%d]\n",superblock->s_inode_num-superblock->s_free_inode_num,superblock->s_free_inode_num,superblock->s_inode_num);
	int i;
	i = 0;
	printf("0 ");
	while(i<superblock->s_inode_num){
		if(inode_bitmap[i])
			printf("*");
		else
			printf(".");
		i++;
		if(i!=0 && i%32==0){
			printf("\n");
			if(i!=superblock->s_inode_num)
				printf("%d ",i/32);
		}
	}
	printf("\n");
	printf("\n");
	return ;
}

void PrintBlockBitmap(int num)	//打印前num个block使用情况
{
	printf("\n");
	printf("block（磁盘块）使用表：[used:%d %d/%d]\n",superblock->s_block_num-superblock->s_free_block_num,superblock->s_free_block_num,superblock->s_block_num);
	int i;
	i = 0;
	printf("0 ");
	while(i<num){
		if(block_bitmap[i])
			printf("*");
		else
			printf(".");
		i++;
		if(i!=0 && i%32==0){
			printf("\n");
			if(num==superblock->s_block_num)
				getchar();
			if(i!=superblock->s_block_num)
				printf("%d ",i/32);
		}
	}
	printf("\n");
	printf("\n");
	return ;
}

//分配block,返回地址
int BlockDistribute()
{
    if(superblock->s_free_block_num ==0 )
    {
        printf("没有空闲的Block可以分配\n");
        return -1;
    }
    else
    {
        int i;
        for(i = 0 ; superblock->s_block_num;i++)
        {
            if(block_bitmap[i] == 0) //空闲
                break;
        }
        
        block_bitmap[i] =1 ;
        //写回
        superblock->s_free_block_num--;
        fseek(fw,Superblock_StartAddr,SEEK_SET);
        fwrite(superblock,sizeof(superblock),1,fw);
        
        
        fseek(fw,BlockBitmap_StartAddr+i,SEEK_SET); //直接从指针当前位置重写就好
        fwrite(&block_bitmap[i],sizeof(bool),1,fw);
        fflush(fw);

        return Block_StartAddr+i*BLOCK_SIZE;
    }
    
}
//分配inode,返回地址
int InodeDistrubute()
{
    if(superblock->s_free_inode_num ==0 )
    {
        printf("没有空闲的Inode可以分配\n");
        return -1;
    }
    else
    {
        int i;
        for(i = 0 ; superblock->s_inode_num;i++)
        {
            if(inode_bitmap[i] == 0) //空闲
                break;
        }
        
        inode_bitmap[i] =1 ;
        //写回
        superblock->s_free_inode_num--;
        fseek(fw,Superblock_StartAddr,SEEK_SET);
        fwrite(superblock,sizeof(superblock),1,fw);
        
        
        fseek(fw,InodeBitmap_StartAddr+i,SEEK_SET); //直接从指针当前位置重写就好
        fwrite(&inode_bitmap[i],sizeof(bool),1,fw);
        fflush(fw);

        return Inode_StartAddr+i*INODE_SIZE;
    }

}
//释放block
bool BlockFree(int add)
{
    //判断当该地址是合法的正在使用的block地址才可以释放
    if((add-superblock->s_block_address)%superblock->s_block_size != 0) 
    {
        printf("输入的block地址有误，不是正确的block块正确起始地址\n");
        return false;
    }
    else
    {
        int block_index = (add-superblock->s_block_address)/superblock->s_block_size ;
        if(block_bitmap[block_index]==0)
        {
            printf("该磁盘块为空闲磁盘块，并不需要释放");
            return false;
        }
        else
        {
            //清空block内容
            char tmp[BLOCK_SIZE] = {0};
            fseek(fw,add,SEEK_SET);	
            fwrite(tmp,sizeof(tmp),1,fw);

            superblock->s_free_block_num++;
            fseek(fw,Superblock_StartAddr,SEEK_SET);
            fwrite(superblock,sizeof(superblock),1,fw);

            block_bitmap[block_index] = 0;
            fseek(fw,BlockBitmap_StartAddr+block_index,SEEK_SET); //直接从指针当前位置重写就好
            fwrite(&block_bitmap[block_index],sizeof(bool),1,fw);
            fflush(fw);
            return true ;

        }
    }

}
//释放inode
bool InodeFree(int add)
{
     //判断当该地址是合法的正在使用的inode地址才可以释放
    if((add-superblock->s_inode_address)%superblock->s_inode_size != 0) 
    {
        printf("输入的inode地址有误，不是正确的inode块正确起始地址\n");
        return false;
    }
    else
    {
        int inode_index = (add-superblock->s_inode_address)/superblock->s_inode_size ;
        if(block_bitmap[inode_index]==0)
        {
            printf("该inode块为空闲inode块，并不需要释放\n");
            return false;
        }
        else
        {
            //清空inode中的内容
            Inode tmp = {0};
        	fseek(fw,add,SEEK_SET);	
	        fwrite(&tmp,sizeof(tmp),1,fw);

            superblock->s_free_inode_num++;
            fseek(fw,Superblock_StartAddr,SEEK_SET);
            fwrite(superblock,sizeof(superblock),1,fw);

            inode_bitmap[inode_index] = 0;
            fseek(fw,InodeBitmap_StartAddr+inode_index,SEEK_SET); //直接从指针当前位置重写就好
            fwrite(&inode_bitmap[inode_index],sizeof(bool),1,fw);
            fflush(fw);
            return true ;

        }
    }
}

//检查该目录底下是否有相同名字的对应flag属性的文件或者目录，flag 0代表文件，1代表目录 ，返回值-1就是拉闸，正常不冲突就是0
int CheckDir(int parinoAddr,char name[],int flag,int& posi,int& posj)//输入的是当前目录的inode的address
{
     DirectoryItem dirlist[16] ;

    //读取上一级目录的Inode
    Inode cur ;
    fseek(fr,parinoAddr,SEEK_SET);	
	fread(&cur,sizeof(Inode),1,fr);

    int i = 0;
    int tempposi =-1 ,tempposj = -1;
    //前160个目录项可以直接在一级直接目录项中进行寻找
    while(i<cur.i_cnt&&i<160)
    {
        int dno = i /16 ;

        //在一级目录顺序遍历Block
        if(cur.i_dirBlock[dno]==-1)
        {
            i+=16;
            continue;
        }

        //取出这个直接块，对这个块中的内容进行判断
		fseek(fr,cur.i_dirBlock[dno],SEEK_SET);	
		fread(dirlist,sizeof(dirlist),1,fr);
		fflush(fr);

        int j = 0;
        for(j;j<16;j++)
        {
            if(strcmp(dirlist[j].d_item_name,name)==0)  //该目录名与目录项中内容重名
            {
                Inode temp;
                fseek(fr,dirlist[j].d_inode_address,SEEK_SET);	
		        fread(&temp,sizeof(Inode),1,fr);
		        fflush(fr);

                if(flag == 1)
                {
                        if(((temp.i_mode >> 9)&1)==1)  //该文件是目录
                    {
          //             printf("该目录已经存在\n");
                        posi = dno;  //冲突的位置
                        posj = j ;
                        return -1;
                    }
                    continue;
                }
                else{
                        if(((temp.i_mode >> 9)&1) == 0)
                        {
          //                    printf("该文件已经存在\n");
                             posi = dno;  //冲突的位置
                              posj = j ;
                             return -1;
                        }
                        continue;
                }

                //如果没发生对应文件属性的冲突的话

               

            }
            else if( strcmp(dirlist[j].d_item_name,"")==0 )
            {
				//找到一个空闲记录，将新目录创建到这个位置 
				//记录这个位置
				if(tempposi==-1){
					tempposi = dno; //块的序号
					tempposj = j; //这个块中的第几个目录项
				}

			}
			i++;
        }
    }

    //遍历完所有目录项之后发现的确不冲突，就把记录下的空闲位置返回
    posi = tempposi;
    posj = tempposj;

    return 0;
}
//创建目录
bool mkdir(int parinoAddr,char name[])
{
    if(strlen(name)>=MAX_NAME_SIZE)
    {
        printf("文件名超过最大文件名长度\n");
        return false;
    }
    
    DirectoryItem dirlist[16] ;

    //读取上一级目录的Inode
    Inode cur ;
    fseek(fr,parinoAddr,SEEK_SET);	
	fread(&cur,sizeof(Inode),1,fr);

    int posi = -1,posj = -1;
    if(CheckDir(parinoAddr,name,1,posi,posj)==-1)
    {
        printf("该目录已经存在\n");
        return false;
    }
    
    //不冲突的话
    //已经找到该目录项中的空闲块位置 posi ,posj
    if(posi != -1)
    {
        //取出这个直接块，要加入的目录条目的位置
		fseek(fr,cur.i_dirBlock[posi],SEEK_SET);	
		fread(dirlist,sizeof(dirlist),1,fr);
		fflush(fr);

        int dir_node_address = InodeDistrubute();
        if(dir_node_address==-1)
        {
            printf("新建目录的inode申请失败\n");
            return false;
        }
        //添加目录项到目录中去
        strcpy(dirlist[posj].d_item_name,name) ;
        dirlist[posj].d_inode_address = dir_node_address ;

        //初始化该inode的信息
        Inode p;
        p.i_cnt =2 ;//.. .
        p.i_inode_id = (dir_node_address-Inode_StartAddr)/superblock->s_inode_size; //第几个inode
		p.i_inodechange_time = time(NULL);
		p.i_filechange_time = time(NULL);
		p.i_fileopen_time = time(NULL);	
        strcpy(p.i_user_name,Cur_User_Name);
		strcpy(p.i_group_name,Cur_Group_Name);

        //为该inode中的目录项的直接Block申请空间
        int dir_block_address = BlockDistribute();
         if(dir_block_address==-1)
        {
            printf("新建目录的block申请失败\n");
            return false;
        }
        DirectoryItem dirlist_temp[16] ;
        strcpy(dirlist_temp[0].d_item_name,".");
        strcpy(dirlist_temp[1].d_item_name,"..");

        dirlist_temp[0].d_inode_address = dir_node_address ;//当前目录inode节点地址
        dirlist_temp[1].d_inode_address = parinoAddr ;  //父目录inode地址

        //写回新建目录中的目录项
        fseek(fw,dir_block_address,SEEK_SET);
        fwrite(&dirlist_temp,sizeof(dirlist_temp),1,fw);
        fflush(fw);

        p.i_dirBlock[0] = dir_block_address ;
        for(int k = 1 ; k<10 ; ++k)
        {
            p.i_dirBlock[k] = -1;
        }
        p.i_indirBlock_1 =-1 ;//没使用
        p.i_file_size = superblock->s_block_size ;
        p.i_mode =  MODE_DIR | DIR_DEF_PERMISSION;

        //写回新建目录的inode
		fseek(fw,dir_node_address,SEEK_SET);	
		fwrite(&p,sizeof(Inode),1,fw);

        //写回当前目录的目录项内容
        fseek(fw,cur.i_dirBlock[posi],SEEK_SET);	
		fwrite(dirlist,sizeof(dirlist),1,fw);

        //写回当前目录的inode
        cur.i_cnt++;
        fseek(fw,parinoAddr,SEEK_SET);	
		fwrite(&cur,sizeof(Inode),1,fw);
		fflush(fw);

        return true;

    }


    

    
}
//创建文件
bool create(int parinoAddr,char name[],char buf[])
{
    if(strlen(name)>=MAX_NAME_SIZE)
    {
        printf("超过文件名最大长度\n");
        return false;
    }
    //取出当前目录的目录项
    Inode cur;
    DirectoryItem curdir[16];
    fseek(fr,parinoAddr,SEEK_SET);
    fread(&cur,sizeof(Inode),1,fr) ;

    //posi是这个block在dirblock中的index,j是第几个目录项
    int posi = -1,posj = -1;
    if(CheckDir(parinoAddr,name,0,posi,posj)==-1)
    {
        printf("该文件已经存在\n");
        return false;
    }
    if(posi!=-1)
    {
        fseek(fr,cur.i_dirBlock[posi],SEEK_SET);
        fread(curdir,sizeof(curdir),1,fr);
        fflush(fr);

        int file_inode_address = InodeDistrubute();
        if(file_inode_address == -1)
        {
            printf("分配Inode失败\n") ;
            return false;           
        }
        //添加目录项
        strcpy(curdir[posj].d_item_name,name);
        curdir[posj].d_inode_address  = file_inode_address ;
        
        
        //为新文件添加inode
        Inode p;
        p.i_cnt = 1;
        p.i_inode_id = (file_inode_address - superblock->s_inode_address)/superblock->s_block_size ;
        p.i_filechange_time = time(NULL);
	    p.i_fileopen_time = time(NULL);
        p.i_inodechange_time = time(NULL);
		strcpy(p.i_user_name,Cur_User_Name);
		strcpy(p.i_group_name,Cur_Group_Name);

        //申请分配磁盘块，然后把内容存到磁盘块中
        //将buf内容存到磁盘块 
				int k;
				int len = strlen(buf);	//文件长度，单位为字节
 	        if(len==0)
            {	//长度为0的话也分给它一个block
				int curblockAddr = BlockDistribute();
				if(curblockAddr==-1){
					printf("block分配失败\n");
					return false;
				}
				p.i_dirBlock[k/superblock->s_block_size] = curblockAddr;
				//写入到当前目录的磁盘块
				fseek(fw,curblockAddr,SEEK_SET);	
				fwrite(buf,superblock->s_block_size,1,fw);

			}
            else{
                for(k=0;k<len;k+=superblock->s_block_size)
                {	//最多10次，10个磁盘快，即最多5K
					//分配这个inode的磁盘块，从控制台读取内容
					int curblockAddr =BlockDistribute();
					if(curblockAddr==-1){
						printf("block分配失败\n");
						return false;
					}
					p.i_dirBlock[k/superblock->s_block_size] = curblockAddr;
					//写入到当前目录的磁盘块
					fseek(fw,curblockAddr,SEEK_SET);	
					fwrite(buf+k,superblock->s_block_size,1,fw);
				}
            }
				
			p.i_file_size = len;
			p.i_indirBlock_1 = -1;	//没使用一级间接块
			p.i_mode = 0;
			p.i_mode = MODE_FILE | FILE_DEF_PERMISSION;

            //写回当前文件的inode
            fseek(fw,file_inode_address,SEEK_SET);	
			fwrite(&p,sizeof(Inode),1,fw);   

            //写回当前目录的block
            fseek(fw,cur.i_dirBlock[posi],SEEK_SET);
             fwrite(curdir,sizeof(curdir),1,fw);

            //刷新目录inode
            cur.i_cnt++;
            cur.i_filechange_time = time(NULL);
            cur.i_fileopen_time  = time(NULL);
            cur.i_inodechange_time = time(NULL);
            fseek(fw,parinoAddr,SEEK_SET);
            fwrite(&cur,sizeof(cur),1,fw);
            fflush(fw);
            return true;
    }
    else return false;



}
void rmall(int parinoAddr)	//删除该节点下所有文件或目录
{
	//从这个地址取出inode
	Inode cur;
	fseek(fr,parinoAddr,SEEK_SET);	
	fread(&cur,sizeof(Inode),1,fr);

	//取出目录项数
	int cnt = cur.i_cnt;
	//连接数少，则直接删除
	if(cnt<=2){
		BlockFree(cur.i_dirBlock[0]);
		InodeFree(parinoAddr);
		return ;
	}

	//依次取出磁盘块
	int i = 0;
	while(i<160){	//小于160
		DirectoryItem dirlist[16] = {0};

		if(cur.i_dirBlock[i/16]==-1){
			i+=16;
			continue;
		}
		//取出磁盘块
		int parblockAddr = cur.i_dirBlock[i/16];
		fseek(fr,parblockAddr,SEEK_SET);	
		fread(&dirlist,sizeof(dirlist),1,fr);

		//从磁盘块中依次取出目录项，递归删除
		int j;
		bool f = false;
		for(j=0;j<16;j++){
			//Inode tmp;

			if( ! (strcmp(dirlist[j].d_item_name,".")==0 || 
						strcmp(dirlist[j].d_item_name,"..")==0 || 
						strcmp(dirlist[j].d_item_name,"")==0 ) ){
				f = true;
				rmall(dirlist[j].d_inode_address);	//递归删除
			}

			cnt = cur.i_cnt;
			i++;
		}

		//该磁盘块已空，回收
		if(f)
			BlockFree(parblockAddr);

	}
	//该inode已空，回收
	InodeFree(parinoAddr);
	return ;

}

bool rmdir(int parinoAddr,char name[])	//目录删除函数
{
	if(strlen(name)>=MAX_NAME_SIZE){
		printf("超过最大目录名长度\n");
		return false;
	}

	if(strcmp(name,".")==0 || strcmp(name,"..")==0){
		printf("错误操作\n");
		return 0;
	}

	//从这个地址取出inode
	Inode cur;
	fseek(fr,parinoAddr,SEEK_SET);	
	fread(&cur,sizeof(Inode),1,fr);

	//取出目录项数
	int cnt = cur.i_cnt;

	//判断文件模式。6为owner，3为group，0为other
	int filemode;
	if(strcmp(Cur_User_Name,cur.i_user_name)==0 ) 
		filemode = 6;
	else if(strcmp(Cur_User_Name,cur.i_group_name)==0)
		filemode = 3;
	else 
		filemode = 0;

	if( (((cur.i_mode>>filemode>>1)&1)==0) && (strcmp(Cur_User_Name,"root")!=0) ){
		//没有写入权限
		printf("权限不足：无写入权限\n");
		return false;
	}


	//依次取出磁盘块
	int i = 0;
	while(i<160){	//小于160
		DirectoryItem dirlist[16] = {0};

		if(cur.i_dirBlock[i/16]==-1){
			i+=16;
			continue;
		}
		//取出磁盘块
		int parblockAddr = cur.i_dirBlock[i/16];
		fseek(fr,parblockAddr,SEEK_SET);	
		fread(&dirlist,sizeof(dirlist),1,fr);

		//找到要删除的目录
		int j;
		for(j=0;j<16;j++){
			Inode tmp;
			//取出该目录项的inode，判断该目录项是目录还是文件
			fseek(fr,dirlist[j].d_inode_address,SEEK_SET);	
			fread(&tmp,sizeof(Inode),1,fr);

			if( strcmp(dirlist[j].d_item_name,name)==0){
				if( ( (tmp.i_mode>>9) & 1 ) == 1 ){	//找到目录
					//是目录

					rmall(dirlist[j].d_inode_address);

					//删除该目录条目，写回磁盘
					strcpy(dirlist[j].d_item_name,"");
					dirlist[j].d_inode_address = -1;
					fseek(fw,parblockAddr,SEEK_SET);	
					fwrite(&dirlist,sizeof(dirlist),1,fw);
					cur.i_cnt--;
					fseek(fw,parinoAddr,SEEK_SET);	
					fwrite(&cur,sizeof(Inode),1,fw);

					fflush(fw);
					return true;
				}
				else{
					//不是目录，不管
				}
			}
			i++;
		}

	}
	
	printf("没有找到该目录\n");
	return false;
}

//删除当前目录下的文件，可以使用checkdir中的功能直接检测到碰撞的Posi,posj直接操作，简单很多
bool del(int parinoAddr,char name[])
{
    //读取当前目录的inode
    Inode cur;
    fseek(fr,parinoAddr,SEEK_SET);
    fread(&cur,sizeof(Inode),1,fr) ;

    DirectoryItem dirlist[16];
    int posi = -1 ,posj = -1 ;
    if(CheckDir(parinoAddr,name,0,posi,posj)== 0)
    {
        printf("本目录中不存在该文件\n");
        return false;
    } 
    else
    {
        //读出该文件所在的目录项所在的Block
        fseek(fr,cur.i_dirBlock[posi],SEEK_SET) ;
        fread(dirlist,sizeof(dirlist),1,fr) ;

        Inode p;
        //读出该文件的inode
        fseek(fr,dirlist[posj].d_inode_address,SEEK_SET) ;
        fread(&p,sizeof(Inode),1,fr) ;
        fflush(fr);
        //释放该文件的block
        for(int i = 0 ; i< 10 ;++i)
        {
            if(BlockFree(p.i_dirBlock[i])==-1)
                return false ;
        }
        //释放该文件的inode
        if(InodeFree(dirlist[posj].d_inode_address) ==-1)
        {
            return false ;
        }

        strcpy(dirlist[posj].d_item_name,"");
        dirlist[posj].d_inode_address=-1;

        //删除完该文件之后，准备写回当前目录的目录项
        fseek(fw,cur.i_dirBlock[posi],SEEK_SET) ;
        fwrite(dirlist,sizeof(dirlist),1,fw) ;
        
        //准备写回inode
        cur.i_cnt--;
        cur.i_filechange_time =time(NULL);
        cur.i_fileopen_time = time(NULL);
        cur.i_inodechange_time = time(NULL) ;
        
        fseek(fw,parinoAddr,SEEK_SET);
        fwrite(&cur,sizeof(Inode),1,fw);
        fflush(fw);

        return true;

    }
    

}

//列出当前目录下的所有目录项
void ls(int parinoAddr)
{
    Inode cur;
	//取出这个inode
	fseek(fr,parinoAddr,SEEK_SET);	
	fread(&cur,sizeof(Inode),1,fr);
	fflush(fr);

	//取出目录项数
	int cnt = cur.i_cnt;

	//判断文件模式。6为owner，3为group，0为other
	int filemode;
	if(strcmp(Cur_User_Name,cur.i_user_name)==0 ) 
		filemode = 6;
	else if(strcmp(Cur_User_Name,cur.i_group_name)==0)
		filemode = 3;
	else 
		filemode = 0;

	if( ((cur.i_mode>>filemode>>2)&1)==0 ){
		//没有读取权限
		printf("权限不足：无读取权限\n");
		return ;
	}

	//依次取出磁盘块
	int i = 0;
	while(i<cnt && i<160){
		DirectoryItem dirlist[16] = {0};
		if(cur.i_dirBlock[i/16]==-1){
			i+=16;
			continue;
		}
		//取出磁盘块
		int parblockAddr = cur.i_dirBlock[i/16];
		fseek(fr,parblockAddr,SEEK_SET);	
		fread(&dirlist,sizeof(dirlist),1,fr);
		fflush(fr);

		//输出该磁盘块中的所有目录项
		int j;
		for(j=0;j<16 && i<cnt;j++){
			Inode tmp;
			//取出该目录项的inode，判断该目录项是目录还是文件
			fseek(fr,dirlist[j].d_inode_address,SEEK_SET);	
			fread(&tmp,sizeof(Inode),1,fr);
			fflush(fr);

			if( strcmp(dirlist[j].d_item_name,"")==0 ){
				continue;
			}

			//输出信息
			if( ( (tmp.i_mode>>9) & 1 ) == 1 ){
				printf("d");
			}
			else{
				printf("-");
			}

			tm *ptr;	//存储时间
			ptr=gmtime(&tmp.i_inodechange_time);

			//输出权限信息
			int t = 8;
			while(t>=0){
				if( ((tmp.i_mode>>t)&1)==1){
					if(t%3==2)	printf("r");
					if(t%3==1)	printf("w");
					if(t%3==0)	printf("x");
				}
				else{
					printf("-");
				}
				t--;
			}
			printf("\t");

			//其它
			printf("%d\t",tmp.i_cnt);	//链接
			printf("%s\t",tmp.i_user_name);	//文件所属用户名
			printf("%s\t",tmp.i_group_name);	//文件所属用户名
			printf("%d B\t",tmp.i_file_size);	//文件大小
			printf("%d.%d.%d %02d:%02d:%02d  ",1900+ptr->tm_year,ptr->tm_mon+1,ptr->tm_mday,(8+ptr->tm_hour)%24,ptr->tm_min,ptr->tm_sec);	//上一次修改的时间
			printf("%s",dirlist[j].d_item_name);	//文件名
			printf("\n");
			i++;
		}

    }
}

void cd(int parinoAddr ,char name[])
{
    int posi =-1 ,posj =-1;
    if(CheckDir(parinoAddr,name,1,posi,posj)==0)
    {
        printf("当前目录下不存在该目录\n");
        return ;
    }
    else
    {
        //取出当前目录的inode
        Inode cur;
        fseek(fr,parinoAddr,SEEK_SET);	
	    fread(&cur,sizeof(Inode),1,fr);

        DirectoryItem dirlist[16];
        fseek(fr,cur.i_dirBlock[posi],SEEK_SET);
        fread(dirlist,sizeof(dirlist),1,fr) ;

                CurDir_Inode_Addr = dirlist[posj].d_inode_address ;
        	        if( strcmp(dirlist[posj].d_item_name,".")==0)
                    {
						//本目录，不动
					}
					else if(strcmp(dirlist[posj].d_item_name,"..")==0){
						//上一次目录
						int k;
						for(k=strlen(Cur_Dir_Name);k>=0;k--)
							if(Cur_Dir_Name[k]=='/')
								break;
						Cur_Dir_Name[k]='\0';
						if(strlen(Cur_Dir_Name)==0)
							Cur_Dir_Name[0]='/',Cur_Dir_Name[1]='\0';
					}
					else{
						if(Cur_Dir_Name[strlen(Cur_Dir_Name)-1]!='/')
							strcat(Cur_Dir_Name,"/");
						strcat(Cur_Dir_Name,dirlist[posj].d_item_name);
					}
            return ;
    }
}
void writefile(Inode fileInode,int filed_inode_address,char buf[])	//将buf内容写回文件的磁盘块
{
	//将buf内容写回磁盘块 
	int k;
	int len = strlen(buf);	//文件长度，单位为字节
	for(k=0;k<len;k+=superblock->s_block_size){	//最多10次，10个磁盘快，即最多5K
		//分配这个inode的磁盘块，从控制台读取内容
		int curblockAddr;
		if(fileInode.i_dirBlock[k/superblock->s_block_size]==-1){
			//缺少磁盘块，申请一个
			curblockAddr = BlockDistribute();
			if(curblockAddr==-1){
				printf("block分配失败\n");
				return ;
			}
			fileInode.i_dirBlock[k/superblock->s_block_size] = curblockAddr;
		}
		else{
			curblockAddr = fileInode.i_dirBlock[k/superblock->s_block_size];
		}
		//写入到当前目录的磁盘块
		fseek(fw,curblockAddr,SEEK_SET);	
		fwrite(buf+k,superblock->s_block_size,1,fw);
		fflush(fw);
	}
	//更新该文件大小
	fileInode.i_file_size = len;
	fileInode.i_filechange_time = time(NULL);
	fseek(fw,filed_inode_address,SEEK_SET);	
	fwrite(&fileInode,sizeof(Inode),1,fw);
	fflush(fw);
}

void inUsername(char username[])	//输入用户名
{
	printf("username:");
	scanf("%s",username);	//用户名
}

void inPasswd(char passwd[])	//输入密码
{
	int plen = 0;
	char c;
	fflush(stdin);	//清空缓冲区
	printf("passwd:");
	while(c=getch()){
		if(c=='\r'){	//输入回车，密码确定
			passwd[plen] = '\0';
			fflush(stdin);	//清缓冲区
			printf("\n");
			break;
		}
		else if(c=='\b'){	//退格，删除一个字符
			if(plen!=0){	//没有删到头
				plen--;
			}
		}
		else{	//密码字符
			passwd[plen++] = c;
		}
	}
}

bool login()	//登陆界面
{
	char username[100] = {0};
	char passwd[100] = {0};
	inUsername(username);	//输入用户名
	inPasswd(passwd);		//输入用户密码
	if(check(username,passwd)){	//核对用户名和密码
		isLogin = true;
		return true;
	}
	else{
		isLogin = false;
		return false;
	}
}

bool check(char username[],char passwd[])	//核对用户名，密码
{
	int passwd_Inode_Addr = -1;	//用户文件inode地址
	int shadow_Inode_Addr = -1;	//用户密码文件inode地址
	Inode passwd_Inode;		//用户文件的inode
	Inode shadow_Inode;		//用户密码文件的inode

	Inode cur_dir_inode;	//当前目录的inode
	int i,j;
	DirectoryItem dirlist[16];	//临时目录

	cd(CurDir_Inode_Addr,"etc");	//进入配置文件目录

	//找到passwd和shadow文件inode地址，并取出
	//取出当前目录的inode
	fseek(fr,CurDir_Inode_Addr,SEEK_SET);	
	fread(&cur_dir_inode,sizeof(Inode),1,fr);
	//依次取出磁盘块，查找passwd文件的inode地址，和shadow文件的inode地址
	for(i=0;i<10;i++){
		if(cur_dir_inode.i_dirBlock[i]==-1){
			continue;
		}
		//依次取出磁盘块
		fseek(fr,cur_dir_inode.i_dirBlock[i],SEEK_SET);	
		fread(&dirlist,sizeof(dirlist),1,fr);

		for(j=0;j<16;j++){	//遍历目录项
			if( strcmp(dirlist[j].d_item_name,"passwd")==0 ||	//找到passwd或者shadow条目
				strcmp(dirlist[j].d_item_name,"shadow")==0  ){
				Inode tmp;	//临时inode
				//取出inode，判断是否是文件
				fseek(fr,dirlist[j].d_inode_address,SEEK_SET);	
				fread(&tmp,sizeof(Inode),1,fr);

				if( ((tmp.i_mode>>9)&1)==0 ){	
					//是文件
					//判别是passwd文件还是shadow文件
					if( strcmp(dirlist[j].d_item_name,"passwd")==0 ){	
						passwd_Inode_Addr = dirlist[j].d_inode_address;
						passwd_Inode = tmp;
					}
					else if(strcmp(dirlist[j].d_item_name,"shadow")==0 ){
						shadow_Inode_Addr = dirlist[j].d_inode_address;
						shadow_Inode = tmp;
					}
				}
			}
		}
		if(passwd_Inode_Addr!=-1 && shadow_Inode_Addr!=-1)	//都找到了
			break;
	}

	//查找passwd文件，看是否存在用户username
	char buf[1000000];	//最大1M，暂存passwd的文件内容
	char buf2[600];		//暂存磁盘块内容
	j = 0;	//磁盘块指针
	//取出passwd文件内容
	for(i=0;i<passwd_Inode.i_file_size;i++){
		if(i%superblock->s_block_size==0){	//超出了
			//换新的磁盘块
			fseek(fr,passwd_Inode.i_dirBlock[i/superblock->s_block_size],SEEK_SET);	
			fread(&buf2,superblock->s_block_size,1,fr);
			j = 0;
		}
		buf[i] = buf2[j++];
	}
	buf[i] = '\0';
	if(strstr(buf,username)==NULL){
		//没找到该用户
		printf("用户不存在\n");
		cd(CurDir_Inode_Addr,"..");	//回到根目录
		return false;
	}

	//如果存在，查看shadow文件，取出密码，核对passwd是否正确
	//取出shadow文件内容
	j = 0;
	for(i=0;i<shadow_Inode.i_file_size;i++){
		if(i%superblock->s_block_size==0){	//超出了这个磁盘块
			//换新的磁盘块
			fseek(fr,shadow_Inode.i_dirBlock[i/superblock->s_block_size],SEEK_SET);	
			fread(&buf2,superblock->s_block_size,1,fr);
			j = 0;
		}
		buf[i] = buf2[j++];
	}
	buf[i] = '\0';

	char *p;	//字符指针
	if( (p = strstr(buf,username))==NULL){
		//没找到该用户
		printf("shadow文件中不存在该用户\n");
		cd(CurDir_Inode_Addr,"..");	//回到根目录
		return false;
	}
	//找到该用户，取出密码
	while((*p)!=':'){
		p++;
	}
	p++;
	j = 0;
	while((*p)!='\n'){
		buf2[j++] = *p;
		p++;
	}
	buf2[j] = '\0';

	//核对密码
	if(strcmp(buf2,passwd)==0){	//密码正确，登陆
		strcpy(Cur_User_Name,username);
		if(strcmp(username,"root")==0)
			strcpy(Cur_Group_Name,"root");	//当前登陆用户组名
		else
			strcpy(Cur_Group_Name,"user");	//当前登陆用户组名
		cd(CurDir_Inode_Addr,"..");
		cd(CurDir_Inode_Addr,"home");\
		cd(CurDir_Inode_Addr,username);	//进入到用户目录
		strcpy(Cur_User_Dir_Name,Cur_Dir_Name);	//复制当前登陆用户目录名
		return true;
	}
	else{
		printf("密码错误\n");
		cd(CurDir_Inode_Addr,"..");	//回到根目录
		return false;
	}
}

void gotoRoot()	//回到根目录
{
	memset(Cur_User_Name,0,sizeof(Cur_User_Name));		//清空当前用户名
	memset(Cur_User_Dir_Name,0,sizeof(Cur_User_Dir_Name));	//清空当前用户目录
	CurDir_Inode_Addr = RootDir_Inode_Addr;	//当前用户目录地址设为根目录地址
	strcpy(Cur_Dir_Name,"/");		//当前目录设为"/"
}

void logout()	//用户注销
{
	//回到根目录
	gotoRoot();

	isLogin = false;
	printf("用户注销\n");
	system("pause");
	system("cls");
}

bool useradd(char username[])	//用户注册
{
	if(strcmp(Cur_User_Name,"root")!=0){
		printf("权限不足\n");
		return false;
	}
	int passwd_Inode_Addr = -1;	//用户文件inode地址
	int shadow_Inode_Addr = -1;	//用户密码文件inode地址
	int group_Inode_Addr = -1;	//用户组文件inode地址
	Inode passwd_Inode;		//用户文件的inode
	Inode shadow_Inode;		//用户密码文件的inode
	Inode group_Inode;		//用户组文件inode
	//原来的目录
	char bak_Cur_User_Name[110];
	char bak_Cur_User_Name_2[110];
	char bak_Cur_User_Dir_Name[310];
	int bak_CurDir_Inode_Addr;
	char bak_Cur_Dir_Name[310];
	char bak_Cur_Group_Name[310];

	Inode cur_dir_inode;	//当前目录的inode
	int i,j;
	DirectoryItem dirlist[16];	//临时目录

	//保存现场，回到根目录
	strcpy(bak_Cur_User_Name,Cur_User_Name);
	strcpy(bak_Cur_User_Dir_Name,Cur_User_Dir_Name);
	bak_CurDir_Inode_Addr = CurDir_Inode_Addr;
	strcpy(bak_Cur_Dir_Name,Cur_Dir_Name);

	//创建用户目录
	gotoRoot();
	cd(CurDir_Inode_Addr,"home");
	//保存现场
	strcpy( bak_Cur_User_Name_2 , Cur_User_Name);
	strcpy( bak_Cur_Group_Name , Cur_Group_Name);
	//更改
	strcpy(Cur_User_Name,username);
	strcpy(Cur_Group_Name,"user");
	if(!mkdir(CurDir_Inode_Addr,username)){
		strcpy( Cur_User_Name,bak_Cur_User_Name_2);
		strcpy( Cur_Group_Name,bak_Cur_Group_Name);
		//恢复现场，回到原来的目录
		strcpy(Cur_User_Name,bak_Cur_User_Name);
		strcpy(Cur_User_Dir_Name,bak_Cur_User_Dir_Name);
		CurDir_Inode_Addr = bak_CurDir_Inode_Addr;
		strcpy(Cur_Dir_Name,bak_Cur_Dir_Name);

		printf("用户注册失败!\n");
		return false;
	}
	//恢复现场
	strcpy( Cur_User_Name,bak_Cur_User_Name_2);
	strcpy( Cur_Group_Name,bak_Cur_Group_Name);

	//回到根目录
	gotoRoot();

	//进入用户目录
	cd(CurDir_Inode_Addr,"etc");

	//输入用户密码
	char passwd[100] = {0};
	inPasswd(passwd);	//输入密码

	//找到passwd和shadow文件inode地址，并取出，准备添加条目

	//取出当前目录的inode
	fseek(fr,CurDir_Inode_Addr,SEEK_SET);	
	fread(&cur_dir_inode,sizeof(Inode),1,fr);

	//依次取出磁盘块，查找passwd文件的inode地址，和shadow文件的inode地址
	for(i=0;i<10;i++){
		if(cur_dir_inode.i_dirBlock[i]==-1){
			continue;
		}
		//依次取出磁盘块
		fseek(fr,cur_dir_inode.i_dirBlock[i],SEEK_SET);	
		fread(&dirlist,sizeof(dirlist),1,fr);

		for(j=0;j<16;j++){	//遍历目录项
			if( strcmp(dirlist[j].d_item_name,"passwd")==0 ||	//找到passwd或者shadow条目
				strcmp(dirlist[j].d_item_name,"shadow")==0 ||
				strcmp(dirlist[j].d_item_name,"group")==0){
				Inode tmp;	//临时inode
				//取出inode，判断是否是文件
				fseek(fr,dirlist[j].d_inode_address,SEEK_SET);	
				fread(&tmp,sizeof(Inode),1,fr);

				if( ((tmp.i_mode>>9)&1)==0 ){	
					//是文件
					//判别是passwd文件还是shadow文件
					if( strcmp(dirlist[j].d_item_name,"passwd")==0 ){	
						passwd_Inode_Addr = dirlist[j].d_inode_address;
						passwd_Inode = tmp;
					}
					else if(strcmp(dirlist[j].d_item_name,"shadow")==0 ){
						shadow_Inode_Addr = dirlist[j].d_inode_address;
						shadow_Inode = tmp;
					}
					else if(strcmp(dirlist[j].d_item_name,"group")==0 ){
						group_Inode_Addr = dirlist[j].d_inode_address;
						group_Inode = tmp;
					}
				}
			}
		}
		if(passwd_Inode_Addr!=-1 && shadow_Inode_Addr!=-1)	//都找到了
			break;
	}

	//查找passwd文件，看是否存在用户username
	char buf[100000];	//最大100K，暂存passwd的文件内容
	char buf2[600];		//暂存磁盘块内容
	j = 0;	//磁盘块指针
	//取出passwd文件内容
	for(i=0;i<passwd_Inode.i_file_size;i++){
		if(i%superblock->s_block_size==0){	//超出了
			//换新的磁盘块
			fseek(fr,passwd_Inode.i_dirBlock[i/superblock->s_block_size],SEEK_SET);	
			fread(&buf2,superblock->s_block_size,1,fr);
			j = 0;
		}
		buf[i] = buf2[j++];
	}
	buf[i] = '\0';

	if(strstr(buf,username)!=NULL){
		//没找到该用户
		printf("用户已存在\n");

		//恢复现场，回到原来的目录
		strcpy(Cur_User_Name,bak_Cur_User_Name);
		strcpy(Cur_User_Dir_Name,bak_Cur_User_Dir_Name);
		CurDir_Inode_Addr = bak_CurDir_Inode_Addr;
		strcpy(Cur_Dir_Name,bak_Cur_Dir_Name);
		return false;
	}
	
	//如果不存在，在passwd中创建新用户条目,修改group文件
	sprintf(buf+strlen(buf),"%s:x:%d:%d\n",username,nextUID++,1);	//增加条目，用户名：加密密码：用户ID：用户组ID。用户组为普通用户组，值为1 
	passwd_Inode.i_file_size = strlen(buf);
	writefile(passwd_Inode,passwd_Inode_Addr,buf);	//将修改后的passwd写回文件中
	
	//取出shadow文件内容
	j = 0;
	for(i=0;i<shadow_Inode.i_file_size;i++){
		if(i%superblock->s_block_size==0){	//超出了这个磁盘块
			//换新的磁盘块
			fseek(fr,shadow_Inode.i_dirBlock[i/superblock->s_block_size],SEEK_SET);	
			fread(&buf2,superblock->s_block_size,1,fr);
			j = 0;
		}
		buf[i] = buf2[j++];
	}
	buf[i] = '\0';

	//增加shadow条目
	sprintf(buf+strlen(buf),"%s:%s\n",username,passwd);	//增加条目，用户名：密码
	shadow_Inode.i_file_size = strlen(buf);
	writefile(shadow_Inode,shadow_Inode_Addr,buf);	//将修改后的内容写回文件中


	//取出group文件内容
	j = 0;
	for(i=0;i<group_Inode.i_file_size;i++){
		if(i%superblock->s_block_size==0){	//超出了这个磁盘块
			//换新的磁盘块
			fseek(fr,group_Inode.i_dirBlock[i/superblock->s_block_size],SEEK_SET);	
			fread(&buf2,superblock->s_block_size,1,fr);
			j = 0;
		}
		buf[i] = buf2[j++];
	}
	buf[i] = '\0';

	//增加group中普通用户列表
	if(buf[strlen(buf)-2]==':')
		sprintf(buf+strlen(buf)-1,"%s\n",username);	//增加组内用户
	else 
		sprintf(buf+strlen(buf)-1,",%s\n",username);	//增加组内用户
	group_Inode.i_file_size = strlen(buf);
	writefile(group_Inode,group_Inode_Addr,buf);	//将修改后的内容写回文件中

	//恢复现场，回到原来的目录
	strcpy(Cur_User_Name,bak_Cur_User_Name);
	strcpy(Cur_User_Dir_Name,bak_Cur_User_Dir_Name);
	CurDir_Inode_Addr = bak_CurDir_Inode_Addr;
	strcpy(Cur_Dir_Name,bak_Cur_Dir_Name);

	printf("用户注册成功\n");
	return true;
}


bool userdel(char username[])	//用户删除
{
	if(strcmp(Cur_User_Name,"root")!=0){
		printf("权限不足:您需要root权限\n");
		return false;
	}
	if(strcmp(username,"root")==0){
		printf("无法删除root用户\n");
		return false;
	}
	int passwd_Inode_Addr = -1;	//用户文件inode地址
	int shadow_Inode_Addr = -1;	//用户密码文件inode地址
	int group_Inode_Addr = -1;	//用户组文件inode地址
	Inode passwd_Inode;		//用户文件的inode
	Inode shadow_Inode;		//用户密码文件的inode
	Inode group_Inode;		//用户组文件inode
	//原来的目录
	char bak_Cur_User_Name[110];
	char bak_Cur_User_Dir_Name[310];
	int bak_CurDir_Inode_Addr;
	char bak_Cur_Dir_Name[310];

	Inode cur_dir_inode;	//当前目录的inode
	int i,j;
	DirectoryItem dirlist[16];	//临时目录

	//保存现场，回到根目录
	strcpy(bak_Cur_User_Name,Cur_User_Name);
	strcpy(bak_Cur_User_Dir_Name,Cur_User_Dir_Name);
	bak_CurDir_Inode_Addr = CurDir_Inode_Addr;
	strcpy(bak_Cur_Dir_Name,Cur_Dir_Name);

	//回到根目录
	gotoRoot();

	//进入用户目录
	cd(CurDir_Inode_Addr,"etc");

	//输入用户密码
	//char passwd[100] = {0};
	//inPasswd(passwd);	//输入密码

	//找到passwd和shadow文件inode地址，并取出，准备添加条目

	//取出当前目录的inode
	fseek(fr,CurDir_Inode_Addr,SEEK_SET);	
	fread(&cur_dir_inode,sizeof(Inode),1,fr);

	//依次取出磁盘块，查找passwd文件的inode地址，和shadow文件的inode地址
	for(i=0;i<10;i++){
		if(cur_dir_inode.i_dirBlock[i]==-1){
			continue;
		}
		//依次取出磁盘块
		fseek(fr,cur_dir_inode.i_dirBlock[i],SEEK_SET);	
		fread(&dirlist,sizeof(dirlist),1,fr);

		for(j=0;j<16;j++){	//遍历目录项
			if( strcmp(dirlist[j].d_item_name,"passwd")==0 ||	//找到passwd或者shadow条目
				strcmp(dirlist[j].d_item_name,"shadow")==0 ||
				strcmp(dirlist[j].d_item_name,"group")==0){
				Inode tmp;	//临时inode
				//取出inode，判断是否是文件
				fseek(fr,dirlist[j].d_inode_address,SEEK_SET);	
				fread(&tmp,sizeof(Inode),1,fr);

				if( ((tmp.i_mode>>9)&1)==0 ){	
					//是文件
					//判别是passwd文件还是shadow文件
					if( strcmp(dirlist[j].d_item_name,"passwd")==0 ){	
						passwd_Inode_Addr = dirlist[j].d_inode_address;
						passwd_Inode = tmp;
					}
					else if(strcmp(dirlist[j].d_item_name,"shadow")==0 ){
						shadow_Inode_Addr = dirlist[j].d_inode_address;
						shadow_Inode = tmp;
					}
					else if(strcmp(dirlist[j].d_item_name,"group")==0 ){
						group_Inode_Addr = dirlist[j].d_inode_address;
						group_Inode = tmp;
					}
				}
			}
		}
		if(passwd_Inode_Addr!=-1 && shadow_Inode_Addr!=-1)	//都找到了
			break;
	}

	//查找passwd文件，看是否存在用户username
	char buf[100000];	//最大100K，暂存passwd的文件内容
	char buf2[600];		//暂存磁盘块内容
	j = 0;	//磁盘块指针
	//取出passwd文件内容
	for(i=0;i<passwd_Inode.i_file_size;i++){
		if(i%superblock->s_block_size==0){	//超出了
			//换新的磁盘块
			fseek(fr,passwd_Inode.i_dirBlock[i/superblock->s_block_size],SEEK_SET);	
			fread(&buf2,superblock->s_block_size,1,fr);
			j = 0;
		}
		buf[i] = buf2[j++];
	}
	buf[i] = '\0';

	if(strstr(buf,username)==NULL){
		//没找到该用户
		printf("用户不存在\n");

		//恢复现场，回到原来的目录
		strcpy(Cur_User_Name,bak_Cur_User_Name);
		strcpy(Cur_User_Dir_Name,bak_Cur_User_Dir_Name);
		CurDir_Inode_Addr = bak_CurDir_Inode_Addr;
		strcpy(Cur_Dir_Name,bak_Cur_Dir_Name);
		return false;
	}
	
	//如果存在，在passwd、shadow、group中删除该用户的条目
	//删除passwd条目
	char *p = strstr(buf,username);
	*p = '\0';
	while((*p)!='\n')	//空出中间的部分
		p++;
	p++;
	strcat(buf,p);	
	passwd_Inode.i_file_size = strlen(buf);	//更新文件大小
	writefile(passwd_Inode,passwd_Inode_Addr,buf);	//将修改后的passwd写回文件中
	
	//取出shadow文件内容
	j = 0;
	for(i=0;i<shadow_Inode.i_file_size;i++){
		if(i%superblock->s_block_size==0){	//超出了这个磁盘块
			//换新的磁盘块
			fseek(fr,shadow_Inode.i_dirBlock[i/superblock->s_block_size],SEEK_SET);	
			fread(&buf2,superblock->s_block_size,1,fr);
			j = 0;
		}
		buf[i] = buf2[j++];
	}
	buf[i] = '\0';

	//删除shadow条目
	p = strstr(buf,username);
	*p = '\0';
	while((*p)!='\n')	//空出中间的部分
		p++;
	p++;
	strcat(buf,p);	
	shadow_Inode.i_file_size = strlen(buf);	//更新文件大小
	writefile(shadow_Inode,shadow_Inode_Addr,buf);	//将修改后的内容写回文件中


	//取出group文件内容
	j = 0;
	for(i=0;i<group_Inode.i_file_size;i++){
		if(i%superblock->s_block_size==0){	//超出了这个磁盘块
			//换新的磁盘块
			fseek(fr,group_Inode.i_dirBlock[i/superblock->s_block_size],SEEK_SET);	
			fread(&buf2,superblock->s_block_size,1,fr);
			j = 0;
		}
		buf[i] = buf2[j++];
	}
	buf[i] = '\0';

	//增加group中普通用户列表
	p = strstr(buf,username);
	*p = '\0';
	while((*p)!='\n' && (*p)!=',')	//空出中间的部分
		p++;
	if((*p)==',')
		p++;
	strcat(buf,p);	
	group_Inode.i_file_size = strlen(buf);	//更新文件大小
	writefile(group_Inode,group_Inode_Addr,buf);	//将修改后的内容写回文件中

	//恢复现场，回到原来的目录
	strcpy(Cur_User_Name,bak_Cur_User_Name);
	strcpy(Cur_User_Dir_Name,bak_Cur_User_Dir_Name);
	CurDir_Inode_Addr = bak_CurDir_Inode_Addr;
	strcpy(Cur_Dir_Name,bak_Cur_Dir_Name);

	//删除用户目录	
	CurDir_Inode_Addr = RootDir_Inode_Addr;	//当前用户目录地址设为根目录地址
	strcpy(Cur_Dir_Name,"/");		//当前目录设为"/"
	cd(CurDir_Inode_Addr,"home");
	rmdir(CurDir_Inode_Addr,username);

	//恢复现场，回到原来的目录
	strcpy(Cur_User_Name,bak_Cur_User_Name);
	strcpy(Cur_User_Dir_Name,bak_Cur_User_Dir_Name);
	CurDir_Inode_Addr = bak_CurDir_Inode_Addr;
	strcpy(Cur_Dir_Name,bak_Cur_Dir_Name);

	printf("用户已删除\n");
	return true;
	
}

void chmod(int parinoAddr,char name[],int pmode)	//修改文件或目录权限
{
	if(strlen(name)>=MAX_NAME_SIZE){
		printf("超过最大目录名长度\n");
		return ;
	}
	if(strcmp(name,".")==0 || strcmp(name,"..")==0){
		printf("错误操作\n");
		return ;
	}
	//取出该文件或目录inode
	Inode cur,fileInode;
	fseek(fr,parinoAddr,SEEK_SET);	
	fread(&cur,sizeof(Inode),1,fr);

	//依次取出磁盘块
	int i = 0,j;
	DirectoryItem dirlist[16] = {0};
	while(i<160){
		if(cur.i_dirBlock[i/16]==-1){
			i+=16;
			continue;
		}
		//取出磁盘块
		int parblockAddr = cur.i_dirBlock[i/16];
		fseek(fr,parblockAddr,SEEK_SET);	
		fread(&dirlist,sizeof(dirlist),1,fr);
		fflush(fr);

		//输出该磁盘块中的所有目录项
		for(j=0;j<16;j++){
			if( strcmp(dirlist[j].d_item_name,name)==0 ){	//找到该目录或者文件
				//取出对应的inode
				fseek(fr,dirlist[j].d_inode_address,SEEK_SET);	
				fread(&fileInode,sizeof(Inode),1,fr);
				fflush(fr);
				goto label;
			}
		}
		i++;
	}
label:
	if(i>=160){
		printf("文件不存在\n");
		return ;
	}

	//判断是否是本用户
	if(strcmp(Cur_User_Name,fileInode.i_user_name)!=0 && strcmp(Cur_User_Name,"root")!=0){
		printf("权限不足\n");
		return ;
	}

	//对inode的mode属性进行修改
	fileInode.i_mode  = (fileInode.i_mode>>9<<9) | pmode;	//修改权限

	//将inode写回磁盘
	fseek(fw,dirlist[j].d_inode_address,SEEK_SET);	
	fwrite(&fileInode,sizeof(Inode),1,fw);
	fflush(fw);
}

void touch(int parinoAddr,char name[],char buf[])	//touch命令创建文件，读入字符
{
	//先判断文件是否已存在。如果存在，打开这个文件并编辑
	if(strlen(name)>=MAX_NAME_SIZE){
		printf("超过最大文件名长度\n");
		return ;
	}
	//查找有无同名文件，有的话提示错误，退出程序。没有的话，创建一个空文件
	DirectoryItem dirlist[16];	//临时目录清单

	//从这个地址取出inode
	Inode cur,fileInode;
	fseek(fr,parinoAddr,SEEK_SET);	
	fread(&cur,sizeof(Inode),1,fr);

	//判断文件模式。6为owner，3为group，0为other
	int filemode;
	if(strcmp(Cur_User_Name,cur.i_user_name)==0 ) 
		filemode = 6;
	else if(strcmp(Cur_User_Name,cur.i_group_name)==0)
		filemode = 3;
	else 
		filemode = 0;
	
	int i = 0,j;
	int dno;
	int filed_inode_address = -1;	//文件的inode地址
	while(i<160){
		//160个目录项之内，可以直接在直接块里找
		dno = i/16;	//在第几个直接块里

		if(cur.i_dirBlock[dno]==-1){
			i+=16;
			continue;
		}
		fseek(fr,cur.i_dirBlock[dno],SEEK_SET);	
		fread(dirlist,sizeof(dirlist),1,fr);
		fflush(fr);

		//输出该磁盘块中的所有目录项
		for(j=0;j<16;j++){
			if(strcmp(dirlist[j].d_item_name,name)==0 ){
				//重名，取出inode，判断是否是文件
				fseek(fr,dirlist[j].d_inode_address,SEEK_SET);	
				fread(&fileInode,sizeof(Inode),1,fr);
				if( ((fileInode.i_mode>>9)&1)==0 ){	//是文件且重名，提示错误，退出程序
					printf("文件已存在\n");
					return ;
				}
			}
			i++;
		}
	}

	//文件不存在，创建一个空文件
	if( ((cur.i_mode>>filemode>>1)&1)==1){
		//可写。可以创建文件
		buf[0] = '\0';
		create(parinoAddr,name,buf);	//创建文件
	}
	else{
		printf("权限不足：无写入权限\n");
		return ;
	}
	
}

void help()	//显示所有命令清单 
{
	printf("ls - 显示当前目录清单\n");
	printf("cd - 进入目录\n");
	printf("mkdir - 创建目录\n");
	printf("rmdir - 删除目录\n");
	printf("super - 查看超级块\n");
	printf("inode - 查看inode位图\n");
	printf("block - 查看block位图\n");
	//printf("vi - vi编辑器\n");
	printf("create - 创建一个空文件\n");
	printf("rm - 删除文件\n");
	printf("cls - 清屏\n");
	printf("logout - 用户注销\n");
	printf("useradd - 添加用户\n");
	printf("userdel - 删除用户\n");
	printf("chmod - 修改文件或目录权限\n");
	printf("help - 显示命令清单\n");
	printf("exit - 退出系统\n");
	return ;
}

void cmd(char str[])	//处理输入的命令
{
	char p1[100];
	char p2[100];
	char p3[100];
	char buf[100000];	//最大100K
	int tmp = 0;
	int i;
	sscanf(str,"%s",p1);
	if(strcmp(p1,"ls")==0){
		ls(CurDir_Inode_Addr);	//显示当前目录
	}
	else if(strcmp(p1,"cd")==0){
		sscanf(str,"%s%s",p1,p2);
		cd(CurDir_Inode_Addr,p2);	
	}
	else if(strcmp(p1,"mkdir")==0){
		sscanf(str,"%s%s",p1,p2);
		mkdir(CurDir_Inode_Addr,p2);	
	}
	else if(strcmp(p1,"rmdir")==0){
		sscanf(str,"%s%s",p1,p2);
		rmdir(CurDir_Inode_Addr,p2);	
	}
	else if(strcmp(p1,"super")==0){
		PrintSuperBlock();
	}
	else if(strcmp(p1,"inode")==0){
		PrintInodeBitmap();
	}
	else if(strcmp(p1,"block")==0){
		sscanf(str,"%s%s",p1,p2);
		tmp = 0;
		if('0'<=p2[0] && p2[0]<='9'){
			for(i=0;p2[i];i++)
				tmp = tmp*10+p2[i]-'0';
			PrintBlockBitmap(tmp);
		}
		else
			PrintBlockBitmap();
	}
	// else if(strcmp(p1,"vi")==0){	//创建一个文件
	// 	sscanf(str,"%s%s",p1,p2);
	// 	vi(CurDir_Inode_Addr,p2,buf);	//读取内容到buf
	// }
	else if(strcmp(p1,"create")==0){
		sscanf(str,"%s%s",p1,p2);
		touch (CurDir_Inode_Addr,p2,buf);	//读取内容到buf
	}
	else if(strcmp(p1,"rm")==0){	//删除一个文件
		sscanf(str,"%s%s",p1,p2);
		del(CurDir_Inode_Addr,p2);
	}
	else if(strcmp(p1,"cls")==0){
		system("cls");
	}
	else if(strcmp(p1,"logout")==0){
		logout();
	}
	else if(strcmp(p1,"useradd")==0){
		p2[0] = '\0';
		sscanf(str,"%s%s",p1,p2);
		if(strlen(p2)==0){
			printf("参数错误\n");
		}
		else{
			useradd(p2);
		}
	}
	else if(strcmp(p1,"userdel")==0){
		p2[0] = '\0';
		sscanf(str,"%s%s",p1,p2);
		if(strlen(p2)==0){
			printf("参数错误\n");
		}
		else{
			userdel(p2);
		}
	}
	else if(strcmp(p1,"chmod")==0){
		p2[0] = '\0';
		p3[0] = '\0';
		sscanf(str,"%s%s%s",p1,p2,p3);
		if(strlen(p2)==0 || strlen(p3)==0){
			printf("参数错误\n");
		}
		else{
			tmp = 0;
			for(i=0;p3[i];i++)
				tmp = tmp*8+p3[i]-'0';
			chmod(CurDir_Inode_Addr,p2,tmp);
		}
	}
	else if(strcmp(p1,"help")==0){
		help();
	}
	else if(strcmp(p1,"format")==0){
		if(strcmp(Cur_User_Name,"root")!=0){
			printf("权限不足：您需要root权限\n");
			return ;
		}
		Ready();
        	char c;
	printf("是否格式化?[y/n]");
	while(c = getch()){
		fflush(stdin);
		if(c=='y'){
			printf("\n");
			printf("文件系统正在格式化……\n");
			if(!Format()){
				printf("文件系统格式化失败\n");
				return ;
			}
			printf("格式化完成\n");
			break;
		}
		else if(c=='n'){
			printf("\n");
			break;
		}
	}

	//printf("载入文件系统……\n");
	if(!Loading()){			
		printf("安装文件系统失败\n");
		return ;
	}
		logout();
	}
	else if(strcmp(p1,"exit")==0){
		printf("退出LeoOS\n");
		exit(0);
	}
	else{
		printf("抱歉，没有该命令\n");
	}
	return ;
}

int main()
{
	//打开虚拟磁盘文件 
	if( (fr = fopen(FILESYSNAME,"rb"))==NULL){	//只读打开虚拟磁盘文件（二进制文件）
		//虚拟磁盘文件不存在，创建一个
		fw = fopen(FILESYSNAME,"wb");	//只写打开虚拟磁盘文件（二进制文件）
		if(fw==NULL){
			printf("虚拟磁盘文件打开失败\n");
			return 0;	//打开文件失败
		}
		fr = fopen(FILESYSNAME,"rb");	//现在可以打开了

		//初始化变量
		nextUID = 0;
		nextGID = 0;
		isLogin = false;
		strcpy(Cur_User_Name,"root");
		strcpy(Cur_Group_Name,"root");

		//获取主机名
		memset(Cur_Host_Name,0,sizeof(Cur_Host_Name));  
		DWORD k= 100;  
		GetComputerName(Cur_Host_Name,&k);

		//根目录inode地址 ，当前目录地址和名字
		RootDir_Inode_Addr = Inode_StartAddr;	//第一个inode地址
		CurDir_Inode_Addr = RootDir_Inode_Addr;
		strcpy(Cur_Dir_Name,"/");

		printf("文件系统正在格式化……\n");
		if(!Format()){
			printf("文件系统格式化失败\n");
			return 0;
		}
		printf("格式化完成\n");
		printf("按任意键进行第一次登陆\n");
		system("pause");
		system("cls");


		if(!Loading()){			
			printf("安装文件系统失败\n");
			return 0;
		}
	}
	else{	//虚拟磁盘文件已存在
		fread(buffer,Sum_Size,1,fr);

		//取出文件内容暂存到内容中，以写方式打开文件之后再写回文件（写方式打开回清空文件）
		fw = fopen(FILESYSNAME,"wb");	//只写打开虚拟磁盘文件（二进制文件）
		if(fw==NULL){
			printf("虚拟磁盘文件打开失败\n");
			return false;	//打开文件失败
		}
		fwrite(buffer,Sum_Size,1,fw);

		/* 提示是否要格式化
		 * 因为不是第一次登陆，先略去这一步
		 * 下面需要手动设置变量
		Ready();
		system("pause");
		system("cls");
		*/
		
		//初始化变量
		nextUID = 0;
		nextGID = 0;
		isLogin = false;
		strcpy(Cur_User_Name,"root");
		strcpy(Cur_Group_Name,"root");

		//获取主机名
		memset(Cur_Host_Name,0,sizeof(Cur_Host_Name));  
		DWORD k= 100;  
		GetComputerName(Cur_Host_Name,&k);

		//根目录inode地址 ，当前目录地址和名字
		RootDir_Inode_Addr = Inode_StartAddr;	//第一个inode地址
		CurDir_Inode_Addr = RootDir_Inode_Addr;
		strcpy(Cur_Dir_Name,"/");

		if(!Loading()){			
			printf("安装文件系统失败\n");
			return 0;
		}

	}


	//testPrint();

	//登录
	while(1){	
		if(isLogin){	//登陆成功，才能进入shell
			char str[100];
			char *p;
			if( (p = strstr(Cur_Dir_Name,Cur_User_Dir_Name))==NULL)	//没找到
				printf("[%s@%s %s]# ",Cur_Host_Name,Cur_User_Name,Cur_Dir_Name);	
			else
				printf("[%s@%s ~%s]# ",Cur_Host_Name,Cur_User_Name,Cur_Dir_Name+strlen(Cur_User_Dir_Name));	
			gets(str);
			cmd(str);
		}
		else{	
			printf("欢迎来到LeoOS，请先登录\n");
			while(!login());	//登陆
			printf("登陆成功！\n");
			//system("pause");
			system("cls");
		}
	}

	fclose(fw);		//释放文件指针
	fclose(fr);		//释放文件指针

	return 0;
}


