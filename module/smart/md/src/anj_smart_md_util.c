#include "anj_smart_md_util.h"
#include "anj_config.h"
#include "anj_mw_comm.h"

typedef struct _ANJ_MD_REGION_BLOCK
{
    uint8_t bOpen;
    uint8_t sensitivity;

    uint16_t lt_x; // 区域左上角的像素坐标
    uint16_t lt_y;
    uint16_t rb_x; // 右下角
    uint16_t rb_y;
    uint8_t *block_buf;

    uint16_t cell_w; // cell像素宽，block再分cell
    uint16_t cell_h;
    uint32_t cell_w_count; // block宽度方向cell个数
    uint32_t cell_h_count;

    uint64_t lastUpdateTimestampInMs;
    uint32_t average_diff;

    uint32_t motion_cell;

    uint16_t col; // 列
    uint16_t row; // 行

} ANJ_MD_REGION_BLOCK;

typedef struct _ANJ_MD_HANDLE_st
{
    uint16_t width;
    uint16_t height;
    uint32_t yBufSize; // == width * height
    uint16_t w_div_num;
    uint16_t h_div_num;
    ANJ_MD_REGION_BLOCK regions[ANJ_MD_MAX_REGION_NUM];
    uint8_t *cell_change_mark_buf;
    uint32_t total_cell_num;
    uint32_t obj_cnt;
    ANJ_MD_ObjPos_t objs[0]; // TODO 统计移动侦测对象所在位置
    uint32_t pix_cnt_blink;
    uint32_t max_pix_luma_diff;
    uint32_t max_cell_luma_diff;
    uint32_t obj_in_scope;

    uint32_t md_region_cnt;
    ANJ_MOTION_REGION_S md_regions[ANJ_MD_MAX_REGION_NUM];

} ANJ_MD_HANDLE_st;

ANJ_MD_HANDLE ANJ_MD_Create(uint16_t width, uint16_t height, uint8_t w_div, uint8_t h_div)
{
    if (w_div == 0 || h_div == 0 || w_div > ANJ_MD_MAX_W_DIV_NUM || h_div > ANJ_MD_MAX_H_DIV_NUM)
        return NULL;

    ANJ_MD_HANDLE_st *h = (ANJ_MD_HANDLE_st *)calloc(1, sizeof(ANJ_MD_HANDLE_st));
    if (h == NULL)
        return NULL;

    h->width = width;
    h->height = height;
    h->yBufSize = width * height;
    h->w_div_num = w_div;
    h->h_div_num = h_div;

    h->cell_change_mark_buf = (uint8_t *)calloc(1, h->width / h->w_div_num * h->height / h->h_div_num + 32);

    return h;
}

void ANJ_MD_Destroy(ANJ_MD_HANDLE handle)
{
    int i;
    if (handle != NULL)
    {
        ANJ_MD_HANDLE_st *h = (ANJ_MD_HANDLE_st *)handle;
        ANJ_MD_REGION_BLOCK *b = NULL;
        for (i = 0; i < ANJ_MD_MAX_REGION_NUM; ++i)
        {
            b = &h->regions[i];
            if (b->block_buf != NULL)
            {
                free(b->block_buf);
                b->block_buf = NULL;
            }
        }
        if (h->cell_change_mark_buf != NULL)
        {
            free(h->cell_change_mark_buf);
            h->cell_change_mark_buf = NULL;
        }
        free(handle);
    }
}

void make_md_cell_size_ok(const uint16_t *total_width, uint16_t *cell_width)
{
    if (*cell_width >= *total_width)
        return;

    while ((*total_width) % (*cell_width) != 0)
    {
        (*cell_width)++;
    }
}

int32_t ANJ_MD_SetDetectRegion(ANJ_MD_HANDLE handle, uint32_t regIndex, ANJ_MDParamsIn_t *param)
{
    ANJ_MD_HANDLE_st *h = (ANJ_MD_HANDLE_st *)handle;

    if (h == NULL || regIndex >= h->w_div_num * h->h_div_num || param->sensitivity < 1 || param->sensitivity > 100 || param->size_percent_min > param->size_percent_max || param->size_percent_min > 100 || param->size_percent_max > 100)
    {
        __ERR("region param invalid(sensitivity=%d,size_percent_min=%d, size_percent_max=%d)\n",
              param->sensitivity, param->size_percent_min, param->size_percent_max);
        return ANJ_MD_ERR_PARAM;
    }

    //	__ERR("regIndex = %d, sensitivity=%d, enable=%d\n",
    //	regIndex, param->sensitivity, param->enable);

    ANJ_MD_REGION_BLOCK *block = &h->regions[regIndex];
    block->bOpen = param->enable > 0 ? 1 : 0;
    block->sensitivity = 100 - 100 * param->sensitivity / 100;
    block->lastUpdateTimestampInMs = 0;

    if (block->block_buf != NULL)
        free(block->block_buf);

    uint16_t each_block_width = h->width / h->w_div_num;
    uint16_t each_block_height = h->height / h->h_div_num;

    block->block_buf = (uint8_t *)malloc(each_block_width * each_block_height + 32);
    if (block->block_buf == NULL)
        return ANJ_MD_ERR_NO_MEM;

    block->col = regIndex % h->w_div_num;
    block->row = regIndex / h->w_div_num;

    block->lt_x = (regIndex % h->w_div_num) * each_block_width;
    block->lt_y = (regIndex / h->w_div_num) * each_block_height;
    block->rb_x = block->lt_x + each_block_width;
    block->rb_y = block->lt_y + each_block_height;

    // cell 大小固定为4 * 4
    block->cell_w = 4;
    block->cell_h = 4;

#if 0
    block->cell_w = each_block_width * param->size_percent_min/100/2;//让cell变成竖条形
    block->cell_h = each_block_height * param->size_percent_min/100;
#endif

    //	__ERR("size_percent_min=%d\n", param->size_percent_min);

    if (block->cell_w < 1)
        block->cell_w = 1;
    if (block->cell_h < 1)
        block->cell_h = 1;

    // 每个区域边缘几个像素无法检测
    // make_md_cell_size_ok(&each_block_width, &block->cell_w);
    // make_md_cell_size_ok(&each_block_height, &block->cell_h);

    block->cell_w_count = each_block_width / block->cell_w;
    block->cell_h_count = each_block_height / block->cell_h;

    //   __ERR("set md area %d: %d %d -> %d %d, cell px:%dx%d count:%dx%d \n",
    //     regIndex, block->lt_x, block->lt_y, block->rb_x, block->rb_y,
    //    block->cell_w, block->cell_h, block->cell_w_count, block->cell_h_count);

    int i;
    ANJ_MD_REGION_BLOCK *b;
    h->total_cell_num = 0;
    for (i = 0; i < ANJ_MD_MAX_REGION_NUM; ++i)
    {
        b = &h->regions[i];
        h->total_cell_num += (b->cell_w_count * b->cell_h_count);
#if 0
		
        if(b->bOpen)
        {
            h->total_cell_num += (b->cell_w_count * b->cell_h_count);
        }
#endif
    }

    //	__ERR("total_cell_num=%d\n", h->total_cell_num);

    return ANJ_MD_NO_ERR;
}

/**
 * @brief ANJ_MD_Detect 以阀值大小将block分成更小的cell，计算前后帧每个cell灰度平均变化量，高于灵敏度值 ，则认为该cell有移动，返回变化的cell数量
 * @param handle
 * @param imgBuf
 * @param bufSize
 * @param timestampInMs
 * @return
 */
int32_t ANJ_MD_Detect(ANJ_MD_HANDLE handle, const uint8_t *imgBuf, int32_t bufSize, uint64_t timestampInMs)
{
    int i, j, k, s, t, m, n;
    ANJ_MD_REGION_BLOCK *block;
    ANJ_MD_HANDLE_st *h = (ANJ_MD_HANDLE_st *)handle;

    if (h == NULL)
    {
        __ERR("handle is NULL\n");
        return ANJ_MD_ERR_PARAM;
    }

    if (bufSize < h->yBufSize)
    {
        __ERR("buf size %d < h->yBufSize %u\n", bufSize, h->yBufSize);
        return ANJ_MD_ERR_PARAM;
    }

    h->obj_cnt = 0;
    h->pix_cnt_blink = 0;
    h->max_pix_luma_diff = 0;
    h->max_cell_luma_diff = 0;
    h->obj_in_scope = 0;
    h->md_region_cnt = 0;

    for (i = 0; i < ANJ_MD_MAX_REGION_NUM; ++i)
    {
        block = &h->regions[i];
        block->motion_cell = 0;

#if 1
        if (!block->bOpen || block->block_buf == NULL)
        {
            //__ERR("block->bOpen=%d\n", block->bOpen);
            continue;
        }
#else
        if (block->block_buf == NULL)
        {
            continue;
        }
#endif

        if (block->lastUpdateTimestampInMs == 0)
        {
            s = 0;
            for (j = block->lt_y; j < block->rb_y; j++)
            {
                for (k = block->lt_x; k < block->rb_x; k++)
                {
                    block->block_buf[s++] = imgBuf[j * h->width + k];
                }
            }
            block->lastUpdateTimestampInMs = timestampInMs;
            continue;
        }

        uint32_t region_y_diff_total = 0;

        for (m = 0; m < block->cell_h_count; m++)
        {
            for (n = 0; n < block->cell_w_count; n++)
            {
                uint32_t cell_y_total = 0, cell_y_average_diff;
                uint16_t start_x, start_y, end_x, end_y; // cell pos in global
                uint8_t *before;
                const uint8_t *after;
                start_x = block->lt_x + n * block->cell_w;
                start_y = block->lt_y + m * block->cell_h;
                end_x = start_x + block->cell_w;
                end_y = start_y + block->cell_h;

                s = 0;
                int cell_pix_calc = 0;
                // 间隔1个像素统计亮度差异， 减少计算量

                for (j = start_y; j < end_y; j += 2) // 遍历cell像素，统计与背景差的平均值
                {
                    t = 0;
                    for (k = start_x; k < end_x; k += 2)
                    {
                        before = &block->block_buf[(m * block->cell_h + s) * (block->cell_w_count * block->cell_w) + n * block->cell_w + t];
                        after = &imgBuf[j * h->width + k];
                        uint8_t a, b;
                        a = *before;
                        b = *after;

                        //   __ERR("region[%d, %dx%d, %dx%d, cell(%dx%d), diff:%d %d\n",
                        //	i, block->lt_x, block->lt_y, block->rb_x, block->rb_y, m, n,
                        //	  a, b);

                        int luma_diff = 0;

                        if (a > b)
                        {
                            cell_y_total += (a - b);
                            luma_diff = a - b;
                        }
                        else
                        {
                            cell_y_total += (b - a);
                            luma_diff = b - a;
                        }

                        if (luma_diff > h->max_pix_luma_diff)
                        {
                            h->max_pix_luma_diff = luma_diff;
                        }

                        //	__ERR("luma_diff = %d\n", luma_diff);

                        *before = *after; // 更新背景
                        if (luma_diff > block->sensitivity)
                        {
                            h->pix_cnt_blink++;
                        }

                        cell_pix_calc++;

                        t++;
                    }
                    s++;
                }

                // cell_y_average_diff = cell_y_total/(block->cell_w * block->cell_h);
                cell_y_average_diff = cell_y_total / (cell_pix_calc);
                //   __ERR("cell_pix_check=%d\n", cell_pix_check);

                if (cell_y_average_diff > h->max_cell_luma_diff)
                {
                    h->max_cell_luma_diff = cell_y_average_diff;
                }

                // __ERR("region[%d, %dx%d, %dx%d, cell(%dx%d),cell_y_average_diff=%d\n",
                //   i, block->lt_x, block->lt_y, block->rb_x, block->rb_y, m, n, cell_y_average_diff
                // 	 );

                if (cell_y_average_diff > block->sensitivity)
                {
                    // find a change cell
                    h->obj_cnt++;
                    block->motion_cell++;
                    if (block->bOpen)
                    {
                        h->obj_in_scope++;
                    }
                }

                //__ERR("cell_y_average_diff=%d\n", cell_y_average_diff);
                region_y_diff_total += cell_y_average_diff;

                //__ERR("cell_y_average_diff:%d\n", cell_y_average_diff);
            }
        }

        if (block->motion_cell > 0)
        {
            ANJ_MOTION_REGION_S *md_region = &h->md_regions[h->md_region_cnt];
            md_region->lt_x = block->lt_x;
            md_region->lt_y = block->lt_y;
            md_region->rb_x = block->rb_x;
            md_region->rb_y = block->rb_y;
            md_region->motion_cell = block->motion_cell;
            md_region->col = block->col;
            md_region->row = block->row;
            h->md_region_cnt++;
        }

        //		int cell_total = block->cell_h_count * block->cell_w_count;
        //	block->average_diff = region_y_diff_total/cell_total;
        block->average_diff = region_y_diff_total;
        //	__ERR("region=%d, average_diff=%d, region_y_diff_total=%d, cell_total=%d\n", i, block->average_diff, region_y_diff_total, cell_total);
    }

    //  return h->obj_cnt;
    return h->obj_in_scope;
}

int32_t ANJ_MD_GetTotalCellNum(ANJ_MD_HANDLE handle)
{
    ANJ_MD_HANDLE_st *h = (ANJ_MD_HANDLE_st *)handle;
    if (h == NULL)
        return 0;
    return h->total_cell_num;
}

int32_t ANJ_MD_GetPixNumBlink(ANJ_MD_HANDLE handle)
{
    ANJ_MD_HANDLE_st *h = (ANJ_MD_HANDLE_st *)handle;
    if (h == NULL)
        return 0;
    return h->pix_cnt_blink;
}

int32_t ANJ_MD_GetMaxPixLumaDiff(ANJ_MD_HANDLE handle)
{
    ANJ_MD_HANDLE_st *h = (ANJ_MD_HANDLE_st *)handle;
    if (h == NULL)
        return 0;
    return h->max_pix_luma_diff;
}

int32_t ANJ_MD_GetMaxCellLumaDiff(ANJ_MD_HANDLE handle)
{
    ANJ_MD_HANDLE_st *h = (ANJ_MD_HANDLE_st *)handle;
    if (h == NULL)
        return 0;
    return h->max_cell_luma_diff;
}

int32_t ANJ_MD_GetMotionRegion(ANJ_MD_HANDLE handle, ANJ_MOTION_REGION_S **outMotionRegions)
{
    int ret;
    ANJ_MD_HANDLE_st *h = (ANJ_MD_HANDLE_st *)handle;
    if (h == NULL)
        return 0;

    //	__ERR("h->md_region_cnt=%d\n", h->md_region_cnt);
    ANJ_MOTION_REGION_S *pTmpRegions = NULL;

    if (h->md_region_cnt > 0)
    {
        pTmpRegions = (ANJ_MOTION_REGION_S *)malloc(sizeof(ANJ_MOTION_REGION_S) * h->md_region_cnt);
        memcpy(pTmpRegions, h->md_regions, sizeof(ANJ_MOTION_REGION_S) * h->md_region_cnt);
        ret = h->md_region_cnt;
        *outMotionRegions = pTmpRegions;
    }
    else
    {
        *outMotionRegions = NULL;
        ret = 0;
    }

#if 0

	char p[ANJ_MD_MAX_H_DIV_NUM][ANJ_MD_MAX_W_DIV_NUM];
	memset(p, '#', ANJ_MD_MAX_H_DIV_NUM * ANJ_MD_MAX_W_DIV_NUM);

	int i = 0;
	ANJ_MOTION_REGION_S *pRegions = *outMotionRegions;
	for (i = 0; i < h->md_region_cnt; i++)
	{
		ANJ_MOTION_REGION_S *region = &(pRegions[i]);
		p[region->row][region->col] = '@';
	}

	char line[256];
	for (i = 0; i < ANJ_MD_MAX_H_DIV_NUM; i++)
	{
		memset(line, 0, 256);
		memcpy(line, &p[i][0], ANJ_MD_MAX_W_DIV_NUM);
		__ERR("%s\n", line);
	}
#endif
    return ret;
}

void ANJ_MD_PrintCfg(ANJ_MD_HANDLE handle)
{
    ANJ_MD_HANDLE_st *h = (ANJ_MD_HANDLE_st *)handle;
    if (h == NULL)
        return;

    __INFO("total_cell_num=%d,width=%d,height=%d, w_div_num=%d, h_div_num=%d\n",
          h->total_cell_num, h->width, h->height, h->w_div_num, h->h_div_num);

#if 0
	char tmpbuf[128];
	int xIndex, yIndex, regIndex = 0;
	
	for(yIndex = 0; yIndex < h->h_div_num; yIndex++)
	{
		tmpbuf[0] = 0;
		for(xIndex = 0; xIndex < h->w_div_num ; xIndex++)
		{
			if (h->regions[yIndex * h->w_div_num + xIndex].bOpen)
			{
				strcat(tmpbuf, "# ");
			}
			else
			{
				strcat(tmpbuf, "@ ");
			}
			
			regIndex++;
		}	
		__ERR("row %02d: %s", yIndex, tmpbuf);
	}
#endif
}

void ANJ_MD_ShowResult(MD_RESULT_S *pResult)
{
    int iIndex = 0;
    __ERR("md_cell_num %d, region_cnt %d\n", pResult->md_cell_num, pResult->region_cnt);

    char p[ANJ_MD_MAX_H_DIV_NUM][ANJ_MD_MAX_W_DIV_NUM + 4];

    for (iIndex = 0; iIndex < ANJ_MD_MAX_H_DIV_NUM; iIndex++)
    {
        memset(p[iIndex], ' ', ANJ_MD_MAX_W_DIV_NUM);
        memset(p[iIndex] + ANJ_MD_MAX_W_DIV_NUM, 0, 4);
    }

    for (iIndex = 0; iIndex < MAX_MD_REGION_NUM && iIndex < pResult->region_cnt; iIndex++)
    {
        if (pResult->md_regions[iIndex].row < ANJ_MD_MAX_H_DIV_NUM && pResult->md_regions[iIndex].col < ANJ_MD_MAX_W_DIV_NUM)
        {
            p[pResult->md_regions[iIndex].row][pResult->md_regions[iIndex].col] = '#';
        }
    }

    for (iIndex = 0; iIndex < ANJ_MD_MAX_H_DIV_NUM; iIndex++)
    {
        __ERR("%s\n", p[iIndex]);
    }
}
