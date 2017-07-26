#include"define.h"
#include<string.h>
int naluLength;
chunk_sample* chunkLink;
uint32_t * sampleSize;
uint32_t * sampleIndex;
uint32_t * chunkIndex;
uint32_t sampleCount;
uint32_t chunkCount;
uint32_t chunkLinkCount;

char* strFrameType[7]={"I帧","P帧","B帧","SEI帧","SI帧","SP帧","其它"};
int numFrameType[7]={0}; 

uint32_t decodeUint32(uint8_t head[4])
{
	uint32_t x;
	x = head[0]*256*256*256 + head[1]*256*256 + head[2]*256 + head[3];
	return x;
}

uint32_t decodeUint32_rev(uint8_t head[4])
{
	uint32_t x;
	x = head[3]*256*256*256 + head[2]*256*256 + head[1]*256 + head[0];
	return x;
}

uint32_t decodeUint32_length(uint8_t head[4],int length)   //规定长度的 
{
	uint32_t x;
	if(length == 4)
		x = head[0]*256*256*256 + head[1]*256*256 + head[2]*256 + head[3];
	else if(length == 3){
		x = head[0]*256*256 + head[1]*256 + head[2];
	}
	else if(length == 1){
		x = head[0];
	}
		
	return x;
}

uint32_t findBox(FILE* fp, uint32_t target)    //没考虑文件末尾 
{
	uint32_t boxSize, boxType;
	uint32_t offset = 0x0;
	uint8_t size[4],type[4];
	
	fread(size,1,4,fp);
    fread(type,1,4,fp);
    boxSize = decodeUint32(size);
    boxType = decodeUint32(type);
    
    while(boxType!=target){
    	offset += boxSize;
    	fseek(fp,boxSize-8,SEEK_CUR);
    	fread(size,1,4,fp);
    	fread(type,1,4,fp);
    	boxSize = decodeUint32(size);
    	boxType = decodeUint32(type);
	}
	offset += 0x00000008;
	return offset;
}

int addNode(uint32_t num)
{
	chunk_sample* newPtr;
	chunk_sample* temp;
	temp = chunkLink;
	while(temp->next != NULL)
		temp = temp->next;
	
	newPtr = (chunk_sample*)malloc(sizeof(chunk_sample));
	if(newPtr==NULL){
		printf("error in mallocing for newNode\n");
		return 0;
	}
	
	newPtr->num = num;
	newPtr->index = 0x0;
	newPtr->next = NULL;
	temp->next = newPtr;
	return 1;
}

void sampleIndexTable()
{
	int i,j;
	uint32_t offset,num,length;
	chunk_sample* tempLink;
	tempLink = chunkLink->next;
	if(chunkLinkCount != chunkCount)
		printf("error");
	for(i=0;i<chunkCount && tempLink != NULL;++i){
		tempLink->index = chunkIndex[i];
		tempLink = tempLink->next; 
	}
	
	tempLink = chunkLink;
	num = 0;
	sampleIndex = (uint32_t*)malloc(sizeof(uint32_t)*sampleCount);
//	offset = tempLink->index;
//	num = tempLink->num;
	for(i=0;i<sampleCount;++i){
		if(num == 0){
			tempLink = tempLink->next;
			offset = tempLink->index;
			num = tempLink->num;
		}else{
			offset += length;
		}
		--num;
		length = sampleSize[i];
		sampleIndex[i] =  offset;
		
		
		
	}
}

uint32_t handleSTBL(FILE* fp)
{
	uint32_t boxSize, boxType;
	uint32_t boxSize_old, boxType_old;
	uint8_t size[4],type[4]; 
	uint32_t sumOffset, offset,naluResult, boxLength,tempOffset; 
	int count =0,num,i,seq;
	uint8_t buf[78];
	long res;
	int j;
	
	fread(size,1,4,fp);               //得stbl的size 和 type 
    fread(type,1,4,fp);
    boxSize = decodeUint32(size);
    boxType = decodeUint32(type);
    sumOffset = boxSize-8;
    if(boxType != stbl){
    	printf("error on index stbl box\n");
    	return 0;
	}
	
	offset = 0x0;
	while(offset< sumOffset && count < 4)
	{
		fread(size,1,4,fp);               //得一个box的size 和 type 
		fread(type,1,4,fp);
    	boxSize = decodeUint32(size);
    	boxType = decodeUint32(type);
    	
    	offset += boxSize;
    	boxLength = boxSize;
    	tempOffset = 0x0;
    	switch(boxType)
		{
			case stsd:
				fread(size,1,4,fp);             //找去掉版本号之类 
    			fread(type,1,4,fp);
				
				fread(size,1,4,fp);             //找avc1
    			fread(type,1,4,fp);
    			boxSize = decodeUint32(size);
    			boxType = decodeUint32(type);
    			if(boxType == avc1){
					fread(buf,1,78,fp); 
					tempOffset = findBox(fp,avcC);    //找avcC
					fread(size,1,4,fp);  //去掉版本号之类的
					fread(size,1,4,fp);
					naluResult = decodeUint32_rev(size);
					
					naluResult = naluResult&0x00000003 + 0x00000001;
					naluLength = (int)naluResult;
				}else{
					printf("不是标准的avc编码.\n");
					return 0;
				}
				tempOffset += 86;
				fseek(fp,boxLength-tempOffset,SEEK_CUR);
				++ count;
				break;
			case stsz:
				
				fread(size,1,4,fp);   //版本号+标识 
				fread(type,1,4,fp);
				
				fread(size,1,4,fp);    //个数
				num = decodeUint32(size);
				sampleCount = num;   //全局变量
				sampleSize = (uint32_t*)malloc(sizeof(uint32_t)*num);  //动态申请数组 
				tempOffset = 0x0;
				for(i=0 ;i<num ;++i){   //记录sample的大小 
					
					fread(size,1,4,fp);
					boxSize = decodeUint32(size);
					sampleSize[i] = boxSize;
					tempOffset += 4;
				}
				tempOffset +=  20;
				if(tempOffset != boxLength){
					if(tempOffset < boxLength)
						fseek(fp,boxLength-tempOffset,SEEK_CUR);
				}
				++ count;
				break;
			
			case stsc:
				fread(size,1,4,fp);   //取出版本号。。。 
    			fread(type,1,4,fp);
				num = decodeUint32(type); 
				
				fread(size,1,4,fp);
    			fread(type,1,4,fp);
    			boxSize_old = decodeUint32(size);  // boxSize -- firstChunk
    			boxType_old = decodeUint32(type);  // boxType -- samplePerChunk
    			fread(type,1,4,fp);
    			chunkLinkCount = 0x0;
    			for(i=0;i<num-1;++i){
    				fread(size,1,4,fp);
    				fread(type,1,4,fp);
    				boxSize = decodeUint32(size);
    				boxType = decodeUint32(type);
    				fread(type,1,4,fp);
    				
    				while(boxSize != boxSize_old)
			    	{
			    		addNode(boxType_old);
			    		boxSize_old ++;
			    		chunkLinkCount ++;
					}
					boxSize_old = boxSize;
					boxType_old = boxType;
					tempOffset += 12;
				}
				addNode(boxType_old);
				chunkLinkCount ++;
				tempOffset += 28;
				if(tempOffset != boxLength){
					if(tempOffset < boxLength)
						fseek(fp,boxLength-tempOffset,SEEK_CUR);
				}
				++ count;
				break;

				case stco:	
				fread(size,1,4,fp); //版本号+标识 
				fread(type,1,4,fp); //表的个数 
				num = decodeUint32(type);
				chunkCount = num;      //全局变量
				
				chunkIndex = (uint32_t*)malloc(sizeof(uint32_t)*num);  //动态申请数组 
				for(i=0;i<num;++i){
					fread(size,1,4,fp);
					boxSize = decodeUint32(size);
					chunkIndex[i] = boxSize;
					tempOffset += 4;
				}
					tempOffset +=  16;
					if(tempOffset != boxLength){
						if(tempOffset < boxLength)
						fseek(fp,boxLength-tempOffset,SEEK_CUR);
					}
				++ count;
				break;
			default:
				fseek(fp,boxLength-8,SEEK_CUR);
				
				break;
		} 
	}
	
	if(count != 4){
		printf("此文件缺少必要的box\n");
		return 0;
	}
	
	//整理，构造表格sample-index
	sampleIndexTable(); 
    return 1;
}



uint32_t getParameter(FILE* fp)
{
	uint32_t boxSize, boxType;
	uint8_t size[4],type[4]; 
	uint8_t buf[78];
	int control=0;
	uint32_t tempLength, tempOffset,offset; //记录mdat的offset 
	uint32_t result; 
	
	fread(size,1,4,fp);
	while(!control && !feof(fp))
	{
    	fread(type,1,4,fp);
    	boxSize = decodeUint32(size);
    	boxType = decodeUint32(type);
    	if(boxType != trak){
    		fseek(fp,boxSize-8,SEEK_CUR);
		}else{
			tempLength = boxSize;
    		offset = 0x0;
			tempOffset = findBox(fp,mdia);  //mdia
			offset  +=  tempOffset;
			tempOffset = findBox(fp,minf);  //minf
			offset  +=  tempOffset;
			
			fread(size,1,4,fp);             //找vmhd
    		fread(type,1,4,fp);
    		boxSize = decodeUint32(size);
    		boxType = decodeUint32(type);
    		
    		if(boxType != vmhd){
    			tempOffset += 0x00000008;
				fseek(fp,tempLength-8-tempOffset,SEEK_CUR);	
			}else{
				control = 1;              
				
				fseek(fp,boxSize-8,SEEK_CUR);
    			findBox(fp,stbl);          //找stbl
    			
    			fseek(fp,-8,SEEK_CUR);     //处理stbl内各个box 
    			handleSTBL(fp);
    			
			}
		}
		fread(size,1,4,fp);
	}
}

void bs_init( bs_t *s, void *p_data, int i_data )  
{  
    s->p_start = (unsigned char *)p_data; //用传入的p_data首地址初始化p_start，只记下有效数据的首地址  
    s->p       = (unsigned char *)p_data;  
    s->p_end   = s->p + i_data;                   //尾地址，最后一个字节的首地址?  
    s->i_left  = 8;                              //还没有开始读写，当前字节剩余未读取的位是8  
}  


int bs_read( bs_t *s, int i_count )  
{  
     static int i_mask[33] ={0x00,  
                                  0x01,      0x03,      0x07,      0x0f,  
                                  0x1f,      0x3f,      0x7f,      0xff,  
                                  0x1ff,     0x3ff,     0x7ff,     0xfff,  
                                  0x1fff,    0x3fff,    0x7fff,    0xffff,  
                                  0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,  
                                  0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,  
                                  0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,  
                                  0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff};  
    int      i_shr;              
    int i_result = 0;           //用来存放读取到的的结果 typedef unsigned   uint32_t;  
  
    while( i_count > 0 )     //要读取的比特数  
    {  
        if( s->p >= s->p_end ) //字节流的当前位置>=流结尾，即代表此比特流s已经读完了。  
        {                        
            break;  
        }  
  
        if( ( i_shr = s->i_left - i_count ) >= 0 )     
        {                                           
            i_result |= ( *s->p >> i_shr )&i_mask[i_count];
            s->i_left -= i_count; //即i_left = i_left - i_count，当前字节剩余的未读位数，原来的减去这次读取的  
            if( s->i_left == 0 ) //如果当前字节剩余的未读位数正好是0，说明当前字节读完了，就要开始下一个字节  
            {  
                s->p++;              //移动指针，所以p好象是以字节为步长移动指针的  
                s->i_left = 8;       //新开始的这个字节来说，当前字节剩余的未读位数，就是8比特了  
            }  
            return( i_result );     //可能的返回值之一为：00000000 00000000 00000000 00000001 (4字节长)  
        }  
        else    /* i_shr < 0 ,跨字节的情况*/  
        {  
            i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;    
            i_count  -= s->i_left;   
            s->p++;  //定位到下一个新的字节  
            s->i_left = 8;   //对一个新字节来说，未读过的位数当然是8，即本字节所有位都没读取过  
        }  
    }  
  
    return( i_result );//可能的返回值之一为：00000000 00000000 00000000 00000001 (4字节长)  
}  

/*把要读的比特移到当前字节最右，然后与0x01:00000001进行逻辑与操作，
  因为要读的只是一个比特，这个比特不是0就是1，与0000 0001按位与就可以得知此情况  */ 
int bs_read1( bs_t *s )  
{  
    if( s->p < s->p_end )    
    {  
        unsigned int i_result;  
        s->i_left--;                           //当前字节未读取的位数少了1位  
        i_result = ( *s->p >> s->i_left )&0x01;
        if( s->i_left == 0 )                   //如果当前字节剩余未读位数是0，即是说当前字节全读过了  
        {  
            s->p++;                             //指针s->p 移到下一字节  
            s->i_left = 8;                     //新字节中，未读位数当然是8位  
        }  
        return i_result;                       //unsigned int  
    }  
    return 0;                                  //返回0应该是没有读到东西  
}  


int bs_read_ue( bs_t *s )  
{  
    int i = 0;  
  
  	//条件为：读到的当前比特=0，指针未越界，最多只能读32比特  
    while( bs_read1( s ) == 0 && s->p < s->p_end && i < 32 )    
    {  
        i++;  
    }  
    return( ( 1 << i) - 1 + bs_read( s, i ) );      
}  

void getFrameType(FILE* fp)
{
	uint32_t boxSize, boxType,len;
	uint8_t size[4],type[4]; 
	uint8_t* buf;
	bs_t s;
	int i,frame_type = 0,str;
	
	
	for(i=0;i<sampleCount;++i){
		fseek(fp,sampleIndex[i]+naluLength,SEEK_SET);
		fread(type,1,1,fp);
	    boxType = decodeUint32_length(type,1);
	    boxType = boxType&0x0000001F;
		if(boxType == NAL_SLICE || boxType == NAL_SLICE_IDR){
			len = sampleSize[i]- naluLength -1;
			buf = (uint8_t*)malloc(sizeof(uint8_t)*len);
			fread(buf,1,len,fp);
			bs_init(&s,buf,len);
    		bs_read_ue( &s );  /* i_first_mb */
    		frame_type =  bs_read_ue( &s );/* picture type */  
    		switch(frame_type)
			{
				case 0: case 5: /* P */  
		            str = 1;
		            numFrameType[1]++;
		            break;  
		        case 1: case 6: /* B */  
			    str = 2;
			    numFrameType[2]++;
		            break;  
		        case 3: case 8: /* SP */  
		            str = 5;
		            numFrameType[5]++;
		            break;  
		        case 2: case 7: /* I */   
		            str = 0;
		            numFrameType[0]++;
		            break;  
		        case 4: case 9: /* SI */  
		            str = 4;
		            numFrameType[4]++;
		            break;  
			} 
			free(buf);
		}
		else if(boxType == NAL_SEI){
			str = 3;
			numFrameType[3]++;	
		}else{
			str = 6;
			numFrameType[6]++;
		}
		printf("\033[37m%d.\033[34m%s \033[32m--from:%X \033[33m--size:%X\033[0m\n",i+1,strFrameType[str],sampleIndex[i],sampleSize[i]);
	}
	
	printf("\n总计：\n帧的总数：%d\n\n",sampleCount);
	for(i=0;i<7;++i)
		printf("%s：%d个\n",strFrameType[i],numFrameType[i]);
	
}

/* 0-不可分，1-可分，2-无此类型*/
int getBoxType(uint32_t target, char** type)
{
	int i;
	switch(target)
	{
	   case ftp:  *type = "ftp"; return 0; break;
	   case	mdat: *type = "mdat"; return 0; break;
	   case moov: *type = "moov"; return 1; break; 
	   case	free0:*type = "free"; return 0; break;
	   case mvhd: *type = "mvdh"; return 0; break;
	   case iods: *type = "iods"; return 0; break;
	   case	trak: *type = "trak"; return 1; break;
	   case	tkhd: *type = "tkhd"; return 0; break;
	   case	edts: *type = "edts"; return 1; break;
	   case	elst: *type = "elst"; return 0; break;
	   case	mdia: *type = "mdia"; return 1; break;
	   case	mdhd: *type = "mdhd"; return 0; break;
	   case	hdlr: *type = "hdlr"; return 0; break;
	   case	minf: *type = "minf"; return 1; break;
	   case	vmhd: *type = "vmhd"; return 0; break;
	   case	smhd: *type = "smhd"; return 0; break;
	   case	dinf: *type = "dinf"; return 1; break;
	   case	dref: *type = "dref"; return 0; break;
	   case	stbl: *type = "stbl"; return 1; break;
	   case	stsd: *type = "stsd"; return 1; break;
	   case	avc1: *type = "avc1"; return 0; break;
	   case	mp4a: *type = "mp4a"; return 0; break;
	   case	stts: *type = "stts"; return 0; break;
	   case	stss: *type = "stss"; return 0; break;
	   case	ctts: *type = "ctts"; return 0; break;
	   case	stsc: *type = "stsc"; return 0; break;
	   case	stsz: *type = "stsz"; return 0; break;
	   case	stco: *type = "stco"; return 0; break;
	   case	sgpd: *type = "sgpd"; return 0; break;
	   case	sbgp: *type = "sbgp"; return 0; break;
	   case	udta: *type = "udta"; return 1; break;
	   case	meta: *type = "meta"; return 0; break;
	   default:
	   		*type = "****"; return 2;break;
	} 
}

// 打印文件中盒子的列表 
uint32_t getBoxList(FILE* fp, int floor, uint32_t length)
{
	uint32_t boxSize, boxType;
	uint32_t result, offset,abOffset;
	uint8_t size[4],type[4]; 
	int m,i,j;
	char* str;
	long startIndex;
	int color[7]= {33,34,32,35,31,36,37};
	
	str = (char*)malloc(sizeof(char)*4);
	
	startIndex = ftell(fp);
	i = fread(size,1,4,fp);
	j = fread(type,1,4,fp);
    boxSize = decodeUint32(size);
    boxType = decodeUint32(type);
    
    if(i==0 || j == 0)
    	return -1;
    
    
    if(floor == 0){
    	length = boxSize;
	}else{
		if(length<boxSize)
			return -1;
	}
    
    m = getBoxType(boxType, &str);
    
    for(i=0;i<floor;++i)
    	printf("  ");
	
	printf("\033[%dm%s --from: %X --size: %X\033[0m\n",color[floor],str,startIndex,boxSize);
	
	if(m == 0){
		fseek(fp,boxSize-8,SEEK_CUR);
		if(floor == 0 ){
			while(!feof(fp))
				getBoxList(fp,0,0);
		}else{
			return boxSize;
		}
	}
	else if(m == 1){
		offset = 0;
		abOffset = boxSize-8;
		if(boxType == stsd){
			fseek(fp,8,SEEK_CUR);
			abOffset = abOffset - 8;
		}
		
		while(offset < abOffset)
		{
			result = getBoxList(fp,floor+1,abOffset-offset);
			offset += result;
		}
		
		if(offset > abOffset)
			fseek(fp,abOffset-offset,SEEK_CUR);
			
		return boxSize;
	}else{
		return boxSize;
	}
}

int main(int argc,char *argv[])
{
	FILE* fp;
	uint32_t boxSize, boxType;
	uint32_t offset_mdat = 0x0, offset_temp; //记录mdat的offset 
	int control, naluLength,flag;
	uint8_t size[4],type[4]; 
	uint8_t* buf;
	uint8_t slice_type=0;
	int i,choice = 0;
	chunk_sample* tempLink;
	
	if(argc < 3){
	   printf("error:参数不完整\n");
	   return 0;
	}else{
	   if(strcmp(argv[2],"-a")==0){
		choice = 1;}
	   else if(strcmp(argv[2],"-b")==0){
		choice = 2;}
	   else{
		printf("error:[%s]-未定义的参数\n",argv[2]);
	   }
	}
	
	if((fp=fopen(argv[1],"rb"))==NULL){
        printf("error: Can not open the input file.\n");
        return 0;
        }

	
    
	//功能1：树型打印box列表
	if(choice == 1){ 
    	   getBoxList(fp,0,0);
	   return 0;
	}
        else if(choice == 0){
	   return 0;
	}

	//功能2：记录并打印每个sample的index和大小，判断帧类型 
        //初始化
	chunkLink = (chunk_sample*)malloc(sizeof(chunk_sample));
	if(chunkLink!=NULL){
		chunkLink->num = 0;
		chunkLink->index = 0x0;
		chunkLink->next = NULL;
	} else
		printf("Error in mallocing for chunkLink");
	
	sampleCount = 0;
    	control = 0;
    	flag =0;
    
	//找到moov的入口 
        fread(size,1,4,fp);
        while(!control && !feof(fp)){
    	   fread(type,1,4,fp);
    	   boxSize = decodeUint32(size);
    	   boxType = decodeUint32(type);
    	   if(boxType == moov){
    		control = 1;
    		//跳到子函数，处理里面的box，取pps，sps，和构造table 
    		naluLength=(int)getParameter(fp);   //naluLength个字节表示长度 
		}
		else if(boxType == mdat){
			flag = 1; 
			fseek(fp,boxSize-8,SEEK_CUR);
		}
		else{
			fseek(fp,boxSize-8,SEEK_CUR);
			if(!flag)  offset_mdat += boxSize;
		}
		if(!control) fread(size,1,4,fp);
	}
	
	//处理sample，判断其类型
	getFrameType(fp);  
	return 0;
		
} 











