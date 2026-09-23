#ifndef __UTIL_FONT__H__
#define __UTIL_FONT__H__


#define	FONT_W	16
#define	FONT_H	16
#define FONT_ALIGN_W(value) (((value + 8 - 1) / 8) * 8)
#define	REGION_ALIGN_VALUE		FONT_ALIGN_W(FONT_W)


//字体结构
typedef struct
{
	unsigned short  unicode ;			//unicode 编码
	unsigned char font_w;
	unsigned char font_h;	//字库的宽/高
	unsigned char	mask[FONT_ALIGN_W(FONT_W)*FONT_H/8];	//16*16
}CHAR_STRU;

typedef struct
{
	unsigned int  utf8 ;			//UTF8 编码
	unsigned int  unicode ;			//unicode 编码
	unsigned short	gbk;			//gbk 编码
	unsigned short	big5;			//big5 编码
}CHAR_CODEC_STRU;

//字体文件内容
typedef struct
{
	char	magic[32]; 
	unsigned int	chars_count;
}FONT_HEAD;

int GetMaskStatus(const unsigned char *pMask, int x, int y) ;
int GetUtf8Bytes1(unsigned char chHighByte) ;//根据UTF8第一个字符，获取该UTF8占用几个字节
int GetUtf8Bytes2(unsigned int utf8) ;//根据UTF8 U32值，获取该UTF8占用几个字节
unsigned int GetUtf8ValueU32(const unsigned char *ptr);//根据UTF8字符指针，获取该UTF8 U32值
int GetUtf8ValueU8(unsigned int utf8, unsigned char* pOut);//根据UTF8 U32值，获取该UTF8的每个字节值

void DrawFontMask(const CHAR_STRU *pCharData) ;
void DrawFontMaskInLineArray(const CHAR_STRU *pCharData, char szPrintLine[FONT_H][32]) ;

int codec_init();//用于只需要CODEC转换，不需要点阵的场合，减少内存使用。unicode->utf8/gbk/big5
int codec_deinit();
const CHAR_CODEC_STRU *codec_find(unsigned int data);

int font_init();//unicode点阵字库初始化
int font_deinit();
const CHAR_STRU *font_find(unsigned int data);

/*检查字符串中是否都已经包含在使用中的点阵map，没有的话就从字库文件中读取
bInThread=0用于OSD线程绘制线程开始前先初始化使用的文字，避免线程中异步加载文字的情况下，
第一次没有画出点阵，OSD文字没变化，后面也不再调用绘制，导致OSD没有划出来*/
/*如果需要频繁快速更新OSD，最好开始使用时一次性把所有需要的文字都check出来*/
void font_check_using(const char * pString, int bInThread);
int font_deinit_usingmap();//编码退出的时候销毁使用中的点阵map
const CHAR_STRU *font_find_using(unsigned int data);//从使用中点阵map中查找


unsigned int utf8_to_unicode(unsigned int utf8);
unsigned int unicode_to_utf8(unsigned int unicode);

const char * week_day(int index, int language);
const char * am_pm_str(int index, int language);


int str_gb2312_2_utf8(const char *pIn, char *pOut, int bBig5);
int str_utf8_2_gb2312(const char *pIn, char *pOut);

#endif


