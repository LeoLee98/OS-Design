#include"operation.h"

void Prepare()
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
    gethostname(Cur_Host_Name,k);

    //获取根目录等inode地址信息
    RootDir_Inode_Addr = Inode_StartAddr;
    CurDir_Inode_Addr = RootDir_Inode_Addr ;
    strcpy(Cur_Dir_Name,"/");

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
    superblock->s_inodemap_adderss = InodeBitmap_StartAddr;
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
    DirectoryItem root_diritem[16] = {0};
    strcpy(root_diritem[0].d_item_name,".");
    root_diritem[1].d_inode_address = root_inode_address;

    fseek(fw,root_block_address,SEEK_SET);
    fwrite(root_diritem,sizeof(root_diritem),1,fw);

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
	printf("\tinode位图开始位置：%d B\n",superblock->s_inodemap_adderss);
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
            printf("该inode块为空闲inode块，并不需要释放");
            return false;
        }
        else
        {
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

    int i = 0;
    int posi = -1,posj = -1;
    //前160个目录项可以直接在一级直接目录项中进行寻找
    while(i<160)
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

                if(((temp.i_mode >> 9)&1)==1)  //该文件是目录
                {
                    printf("该目录已经存在\n");
                    return false;
                }

            }
            else if( strcmp(dirlist[j].d_item_name,"")==0 )
            {
				//找到一个空闲记录，将新目录创建到这个位置 
				//记录这个位置
				if(posi==-1){
					posi = dno; //块的序号
					posj = j; //这个块中的第几个目录项
				}

			}
			i++;
        }
    }

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

        dirlist_temp[0].d_inode_address = dir_node_address ;
        dirlist_temp[1].d_inode_address = parinoAddr ;

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
	if(strcmp(Cur_User_Name,cur.i_uname)==0 ) 
		filemode = 6;
	else if(strcmp(Cur_User_Name,cur.i_gname)==0)
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
		DirItem dirlist[16] = {0};

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
			fseek(fr,dirlist[j].inodeAddr,SEEK_SET);	
			fread(&tmp,sizeof(Inode),1,fr);

			if( strcmp(dirlist[j].itemName,name)==0){
				if( ( (tmp.i_mode>>9) & 1 ) == 1 ){	//找到目录
					//是目录

					rmall(dirlist[j].inodeAddr);

					//删除该目录条目，写回磁盘
					strcpy(dirlist[j].itemName,"");
					dirlist[j].inodeAddr = -1;
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
