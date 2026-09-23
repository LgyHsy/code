#ifndef __ANJ_SMART_MD_H__
#define __ANJ_SMART_MD_H__

#if defined(__cplusplus)
extern "C"
{
#endif

#define MD_EVENT_DETECT_RESULT 1
#define MD_EVENT_ALARM 2
#define MD_EVENT_ALARM_DISAPPEAR 3

#define MD_MAX_W_DIV_NUM 22
#define MD_MAX_H_DIV_NUM 18
#define MD_MAX_REGION_NUM (MD_MAX_W_DIV_NUM * MD_MAX_H_DIV_NUM)

// 移动侦测消除时间，单位为妙
#define MD_ALM_DISAPPEAR_TIME 5000

typedef struct _MOTION_REGION_S
{
    unsigned short lt_x; // 区域左上角的像素坐标
    unsigned short lt_y;
    unsigned short rb_x; // 右下角
    unsigned short rb_y;
    unsigned short motion_cell; // 每个区域检测到移动的cell  个数
    unsigned short row;         // 区域对应的行，总的画面划分为18 行乘22 列
    unsigned short col;         // 区域对应的列， 总的画面划分为18 行乘22 列
} MOTION_REGION_S;

#define MAX_MD_REGION_NUM (MD_MAX_W_DIV_NUM * MD_MAX_H_DIV_NUM)

typedef struct _MD_RESULT_S
{
    int md_cell_num;                               // 检测到移动的cell 总数
    MOTION_REGION_S md_regions[MAX_MD_REGION_NUM]; // 检测到移动的区域信息
    int region_cnt;                                // 检测到移动的区域个数
} MD_RESULT_S;

typedef struct
{
    int block_x;
    int block_y;
    int block_config[MD_MAX_H_DIV_NUM][MD_MAX_W_DIV_NUM];
} MD_BLOCK_CFG;

int anj_md_cfg_set(int cameraIndex);
int anj_md_init(int pic_width, int pic_height);
int anj_md_uninit();
void anj_md_process(void *p_vir_addr, int len, int cameraIndex, MD_RESULT_S *pstMdResult);

#if defined(__cplusplus)
}
#endif

#endif
