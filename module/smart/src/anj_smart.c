#include "anj_smart.h"
#include "anj_mw_media_video.h"
#include "anj_module.h"
#include "anj_config.h"
#include "anj_osd.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "anj_mw_comm.h"
#include "anj_mw_smart.h"
#include "anj_alarm.h"
#include "anj_ispctl.h"
#include "anj_smart_provider.h"

#include <math.h>
#include <stdint.h>
#include <pthread.h>

#define ANJ_SMART_DOUBLE_COMPARE(a, b) (fabs((a) - (b)) <= 1E-6 ? 0 : ((a) > (b) ? 1 : -1))
#define ANJ_SMART_DOT_PRODUCT(x1, y1, x2, y2) ((x1) * (x2) + (y1) * (y2))
#define ANJ_SMART_CROSS_PRODUCT(x1, y1, x2, y2) ((x1) * (y2) - (x2) * (y1))
#define ANJ_SMART_VECTOR_CROSS(a, b, c) \
    ANJ_SMART_CROSS_PRODUCT((b).x - (a).x, (b).y - (a).y, (c).x - (a).x, (c).y - (a).y)

#define ANJ_SMART_TRACK_EMA_NEW_WEIGHT (1.0f)  /* 防抖检测权重，越大越不防抖 */
#define ANJ_SMART_TRACK_HOLD_MS (350)          /* 丢检后仍输出平滑框，约 2~3 帧@10fps */
#define PTZ_TRACK_EMA_DEBUG_FILE "/tmp/ema"

static float s_stTrackEmaNewWeight = ANJ_SMART_TRACK_EMA_NEW_WEIGHT;
static AnjSmartAttr gstAnjSmartAttr[ANJ_CAMERA_MAX_NUMS] = {0};
static AnjSmartLuma s_stSmartLumaCtx = {
    .cache = {NULL},
    .width = 0,
    .height = 0,
    .valid = {0},
    .mutex = PTHREAD_MUTEX_INITIALIZER,
};

static void anj_smart_luma_cache_free(void)
{
    int camera = 0;

    pthread_mutex_lock(&s_stSmartLumaCtx.mutex);
    for (camera = 0; camera < ANJ_CAMERA_MAX_NUMS; camera++)
    {
        if (s_stSmartLumaCtx.cache[camera] != NULL)
        {
            anj_mw_free(s_stSmartLumaCtx.cache[camera]);
            s_stSmartLumaCtx.cache[camera] = NULL;
        }
        s_stSmartLumaCtx.valid[camera] = 0;
    }
    s_stSmartLumaCtx.width = 0;
    s_stSmartLumaCtx.height = 0;
    pthread_mutex_unlock(&s_stSmartLumaCtx.mutex);
}

static int anj_smart_luma_cache_alloc(unsigned int width, unsigned int height)
{
    int camera = 0;
    size_t y_size = 0;
    unsigned char *cache[ANJ_CAMERA_MAX_NUMS] = {NULL};

    if ((width == 0) || (height == 0))
    {
        return -1;
    }

    anj_smart_luma_cache_free();
    y_size = (size_t)width * (size_t)height;
    for (camera = 0; camera < ANJ_CAMERA_MAX_NUMS; camera++)
    {
        cache[camera] = (unsigned char *)anj_mw_malloc(y_size);
        if (cache[camera] == NULL)
        {
            __ERR("luma cache malloc fail, camera=%d\n", camera);
            for (camera = 0; camera < ANJ_CAMERA_MAX_NUMS; camera++)
            {
                if (cache[camera] != NULL)
                {
                    anj_mw_free(cache[camera]);
                    cache[camera] = NULL;
                }
            }
            return -1;
        }
        memset(cache[camera], 0, y_size);
    }

    pthread_mutex_lock(&s_stSmartLumaCtx.mutex);
    for (camera = 0; camera < ANJ_CAMERA_MAX_NUMS; camera++)
    {
        s_stSmartLumaCtx.cache[camera] = cache[camera];
        s_stSmartLumaCtx.valid[camera] = 0;
    }
    s_stSmartLumaCtx.width = (int)width;
    s_stSmartLumaCtx.height = (int)height;
    pthread_mutex_unlock(&s_stSmartLumaCtx.mutex);
    return 0;
}

static FILE *pFile = NULL;
static anj_thread_s s_stSmartRestartThread = {0};
static float s_fLastSensitivity[ANJ_CAMERA_MAX_NUMS] = {0};
static float s_fActualAlarmThreshold[ANJ_CAMERA_MAX_NUMS] = {0};

#define ANJ_SMART_HUMAN_MOVE_H_RATIO_PCT 4  /* 横向位移阈值：画面宽度 4% */
#define ANJ_SMART_HUMAN_MOVE_V_RATIO_PCT 5  /* 纵向位移阈值：画面高度 5% */
#define ANJ_SMART_HUMAN_MOVE_WINDOW_MS   2000

typedef struct
{
    unsigned long long record_ms; /* 上次记录中心的时间 */
    int record_cx;
    int record_cy;
} AnjSmartHumanMoveCtx;

static AnjSmartHumanMoveCtx s_stHumanMoveCtx[ANJ_CAMERA_MAX_NUMS] = {0};
static int s_stSmartInit = 0;
static pthread_mutex_t s_stSmartMutex = PTHREAD_MUTEX_INITIALIZER;

static inline double anj_smart_cross_product(double x1, double y1, double x2, double y2)
{
    return x1 * y2 - x2 * y1;
}

static inline double anj_smart_vector_cross(AJ_POINT_F a, AJ_POINT_F b, AJ_POINT_F c)
{
    return anj_smart_cross_product(b.fX - a.fX, b.fY - a.fY, c.fX - a.fX, c.fY - a.fY);
}

static int anj_smart_get_integer_bit(int data, int bit_position)
{
    return (data >> bit_position) & 1;
}

static int anj_smart_point_on_segment(AJ_POINT_F test_point, AJ_POINT_F segment_start, AJ_POINT_F segment_end)
{
    // 检查点是否在线段上
    const double min_x = segment_start.fX < segment_end.fX ? segment_start.fX : segment_end.fX;
    const double max_x = segment_start.fX > segment_end.fX ? segment_start.fX : segment_end.fX;
    const double min_y = segment_start.fY < segment_end.fY ? segment_start.fY : segment_end.fY;
    const double max_y = segment_start.fY > segment_end.fY ? segment_start.fY : segment_end.fY;

    if (test_point.fX < min_x || test_point.fX > max_x ||
        test_point.fY < min_y || test_point.fY > max_y)
    {
        return 0;
    }

    return ANJ_SMART_DOUBLE_COMPARE(anj_smart_vector_cross(segment_start, segment_end, test_point), 0.0) == 0;
}

static int anj_smart_line_segments_intersect(AJ_POINT_F line1_start, AJ_POINT_F line1_end,
                                             AJ_POINT_F line2_start, AJ_POINT_F line2_end)
{
    // 计算四条边的叉积
    const double cross1 = anj_smart_vector_cross(line1_start, line1_end, line2_start);
    const double cross2 = anj_smart_vector_cross(line1_start, line1_end, line2_end);
    const double cross3 = anj_smart_vector_cross(line2_start, line2_end, line1_start);
    const double cross4 = anj_smart_vector_cross(line2_start, line2_end, line1_end);

    // 检查规范相交（线段互相跨越）
    if ((cross1 * cross2 < 0) && (cross3 * cross4 < 0))
    {
        return 1;
    }

    // 检查非规范相交（端点在线段上）
    if (anj_smart_point_on_segment(line2_start, line1_start, line1_end) ||
        anj_smart_point_on_segment(line2_end, line1_start, line1_end) ||
        anj_smart_point_on_segment(line1_start, line2_start, line2_end) ||
        anj_smart_point_on_segment(line1_end, line2_start, line2_end))
    {
        return 1;
    }

    return 0;
}

static int anj_smart_line_rect_intersect(AJ_POINT_F line_start, AJ_POINT_F line_end, AnjSmartBoxInfo *pstAnjSmartBoxInfo)
{
    double scale_x = 100.0 / SMART_PD_WIDTH;
    double scale_y = 100.0 / SMART_PD_HEIGHT;

    int rect_x = pstAnjSmartBoxInfo->x * scale_x;
    int rect_y = pstAnjSmartBoxInfo->y * scale_y;
    int rect_width = pstAnjSmartBoxInfo->width * scale_x;
    int rect_height = pstAnjSmartBoxInfo->height * scale_y;

    // 定义矩形的四个顶点
    AJ_POINT_F rect_points[4] = {0};
    rect_points[0].fX = rect_x;
    rect_points[0].fY = rect_y;
    rect_points[1].fX = rect_x + rect_width;
    rect_points[1].fY = rect_y;
    rect_points[2].fX = rect_x + rect_width;
    rect_points[2].fY = rect_y + rect_height;
    rect_points[3].fX = rect_x;
    rect_points[3].fY = rect_y + rect_height;

    // 检查线段与四条边的相交情况
    for (int i = 0; i < 4; i++)
    {
        if (anj_smart_line_segments_intersect(
                line_start, line_end,
                rect_points[i],
                rect_points[(i + 1) % 4]))
        {
            return 1;
        }
    }

    return 0;
}
// 判断点是否在多边形内
static int anj_smart_polygon_contains_point(const Polygon *polygon, const AJ_POINT_F *point)
{
    int cross_count = 0;

    for (int i = 0; i < polygon->count; i++)
    {
        AJ_POINT_F p1 = {0};
        p1.fX = polygon->points[i].x;
        p1.fY = polygon->points[i].y;
        AJ_POINT_F p2 = {0};
        p2.fX = polygon->points[(i + 1) % polygon->count].x;
        p2.fY = polygon->points[(i + 1) % polygon->count].y;

        // 跳过水平边
        if (ANJ_SMART_DOUBLE_COMPARE(p1.fY, p2.fY) == 0)
        {
            continue;
        }

        // 点在边的下方或上方，跳过
        double min_y = fmin(p1.fY, p2.fY);
        double max_y = fmax(p1.fY, p2.fY);
        if (point->fY < min_y || point->fY >= max_y)
        {
            continue;
        }

        // 计算水平线与边的交点X坐标
        double x_intersect = p1.fX + (point->fY - p1.fY) * (p2.fX - p1.fX) / (p2.fY - p1.fY);

        // 点在交点的左侧，计数增加
        if (x_intersect > point->fX)
        {
            cross_count++;
        }
    }

    // 奇数次相交点在多边形内
    return (cross_count % 2 == 1);
}

static int anj_smart_target_threshold_compare(float target_score, int camera)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    float alarm_threshold = s_fActualAlarmThreshold[camera];
    if (alarm_threshold <= 0.0f)
    {
        // 灵敏度越高 满足的阈值越低
        alarm_threshold = (10 - pstAlarmCfg->aiAlarm.pdAlarm[camera].sensitivity) / 10.0;
    }
    CHECK_VALUE_LIMIT_RANGE(alarm_threshold, 0.01, 0.6);

    if (target_score >= alarm_threshold)
    {
        return 1;
    }

    return 0;
}

// 判断矩形是否与多边形相交
static int anj_smart_rect_polygon_intersect(AnjSmartBoxInfo *pstAnjSmartBoxInfo, int camera)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    Polygon *pstPolygon = &pstAlarmCfg->aiAlarm.pdAlarm[camera].polygonArea;

    // 归一化矩形框
    double scale_x = 100.0 / SMART_PD_WIDTH;
    double scale_y = 100.0 / SMART_PD_HEIGHT;

    int rect_x = pstAnjSmartBoxInfo->x * scale_x;
    int rect_y = pstAnjSmartBoxInfo->y * scale_y;
    int rect_width = pstAnjSmartBoxInfo->width * scale_x;
    int rect_height = pstAnjSmartBoxInfo->height * scale_y;

    // 检查矩形四个顶点是否在多边形内
    AJ_POINT_F corners[4] = {0};
    corners[0].fX = rect_x;
    corners[0].fY = rect_y;
    corners[1].fX = rect_x + rect_width;
    corners[1].fY = rect_y;
    corners[2].fX = rect_x + rect_width;
    corners[2].fY = rect_y + rect_height;
    corners[3].fX = rect_x;
    corners[3].fY = rect_y + rect_height;

    for (int i = 0; i < 4; i++)
    {
        if (anj_smart_polygon_contains_point(pstPolygon, &corners[i]))
        {
            return 1;
        }
    }

    // 检查矩形中心点是否在多边形内
    AJ_POINT_F center = {0};
    center.fX = rect_x + rect_width / 2.0;
    center.fY = rect_y + rect_height / 2.0;

    if (anj_smart_polygon_contains_point(pstPolygon, &center))
    {
        return 1;
    }

    // 检查多边形顶点是否在矩形内
    for (int i = 0; i < pstPolygon->count; i++)
    {
        AJ_POINT_S *vertex = &pstPolygon->points[i];
        if (vertex->x >= rect_x &&
            vertex->x <= rect_x + rect_width &&
            vertex->y >= rect_y &&
            vertex->y <= rect_y + rect_height)
        {
            return 1;
        }
    }

    // 检查矩形边与多边形边是否相交
    AJ_POINT_F rect_edges[4][2] = {
        {corners[0], corners[1]}, // 上边
        {corners[1], corners[2]}, // 右边
        {corners[2], corners[3]}, // 下边
        {corners[3], corners[0]}  // 左边
    };

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < pstPolygon->count; j++)
        {
            AJ_POINT_F poly_p1 = {0};
            poly_p1.fX = pstPolygon->points[j].x;
            poly_p1.fY = pstPolygon->points[j].y;
            AJ_POINT_F poly_p2 = {0};
            poly_p2.fX = pstPolygon->points[(j + 1) % pstPolygon->count].x;
            poly_p2.fY = pstPolygon->points[(j + 1) % pstPolygon->count].y;

            if (anj_smart_line_segments_intersect(rect_edges[i][0], rect_edges[i][1], poly_p1, poly_p2))
            {
                return 1;
            }
        }
    }

    return 0;
}

int anj_smart_detect_rect_line_cross(AnjSmartBoxInfo *pstAnjSmartBoxInfo, int *pCrossAlarm, int camera)
{
    if (s_stSmartInit == 0 || pstAnjSmartBoxInfo == NULL)
    {
        __ERR("input invalid!\n");
        return -1;
    }

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoGateAlarm *pstVideoGate = &pstAlarmCfg->aiAlarm.vgAlarm[camera];
    if (pstVideoGate->enable != 1)
    {
        return 0;
    }

    AjAiBits target_type;
    switch (pstAnjSmartBoxInfo->class_id)
    {
    case 0:
        target_type = AI_TYPE_BIT_HUMAN;
        break;
    case 1:
        target_type = AI_TYPE_BIT_BICYCLE;
        break;
    case 3:
        target_type = AI_TYPE_BIT_MOTO;
        break;
    case 2:
    case 4:
    case 5:
        target_type = AI_TYPE_BIT_CAR;
        break;
    default:
        return 0;
    }

    for (int i = 0; i < MAX_VIDEO_VG_LINE; i++)
    {
        VideoLineStruct *pstCrossLine = &pstVideoGate->data[i];
        if (0 == pstCrossLine->enable)
            continue;

        if (0 == anj_smart_get_integer_bit(pstCrossLine->type, target_type))
        {
            continue;
        }

        AJ_POINT_F line_start = {pstCrossLine->x0Pos, pstCrossLine->y0Pos};
        AJ_POINT_F line_end = {pstCrossLine->x1Pos, pstCrossLine->y1Pos};

        if (anj_smart_line_rect_intersect(line_start, line_end, pstAnjSmartBoxInfo))
        {
            *pCrossAlarm |= 1 << i;
            continue;
        }
    }

    return 0;
}

static void anj_smart_attr_init(AnjSmartAttr *pstAnjSmartAttrArray)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        PdAlarm *pstPdAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[cameraIndex];
        VideoGateAlarm *pstVideoGate = &pstAlarmCfg->aiAlarm.vgAlarm[cameraIndex];
        FaceDetectAlarm *pstFdAlarm = &pstAlarmCfg->aiAlarm.fdAlarm[cameraIndex];
        AnjSmartAttr *pstAnjSmartAttr = &pstAnjSmartAttrArray[cameraIndex];
        pstAnjSmartAttr->stAnjPdAttr.enable = pstPdAlarm->enable || pstVideoGate->enable;
        if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
            pstAnjSmartAttr->stAnjPdAttr.enable = 1;
        pstAnjSmartAttr->stAnjPdAttr.sensitivity = (10 - pstPdAlarm->sensitivity) / 10.0;
        pstAnjSmartAttr->stAnjPdAttr.minRectFilter = pstPdAlarm->minTargetRate;
        pstAnjSmartAttr->stAnjPdAttr.mdFilter = pstPdAlarm->nonMotionFilter;
        s_fLastSensitivity[cameraIndex] = pstAnjSmartAttr->stAnjPdAttr.sensitivity;
        s_fActualAlarmThreshold[cameraIndex] = pstAnjSmartAttr->stAnjPdAttr.sensitivity;

        pstAnjSmartAttr->stAnjFdAttr.enable = pstFdAlarm->enable;
        pstAnjSmartAttr->stAnjFdAttr.sensitivity = (10 - pstFdAlarm->sensitivity) / 10.0f;
        /* 配置互斥：fd 开则关 pd/pvd，下层只看 enable */
        if (pstAnjSmartAttr->stAnjFdAttr.enable)
        {
            pstAnjSmartAttr->stAnjPdAttr.enable = 0;
        }
    }
}

static int anj_smart_init(void)
{
    int iRet = 0;
    int iSubRet = 0;
    unsigned int smart_w = DEFAULT_SMART_WIDTH;
    unsigned int smart_h = DEFAULT_SMART_HEIGHT;

    if (s_stSmartInit)
	{
	    __ERR("had been init!\n");
		return iRet;
	}
    anj_smart_attr_init(gstAnjSmartAttr);
    if (anj_mw_smart_size_get(&gstAnjSmartAttr[0], &smart_w, &smart_h) != 0)
    {
        smart_w = DEFAULT_SMART_WIDTH;
        smart_h = DEFAULT_SMART_HEIGHT;
    }
    iRet = anj_smart_provider_pd_init(&gstAnjSmartAttr[0], anj_smart_data_cb);
    iSubRet = anj_smart_provider_pvd_init(&gstAnjSmartAttr[0], anj_smart_data_cb);
    if (iRet == 0)
    {
        iRet = iSubRet;
    }
    iSubRet = anj_smart_provider_fd_init(&gstAnjSmartAttr[0], anj_smart_data_cb);
    if (iRet == 0)
    {
        iRet = iSubRet;
    }
    iSubRet = anj_smart_provider_md_init((int)smart_w, (int)smart_h);
    if (iRet == 0)
    {
        iRet = iSubRet;
    }
    if (iRet == 0)
    {
        anj_mw_smart_register_yuv_cb(anj_smart_data_cb);
        iRet = anj_smart_luma_cache_alloc(smart_w, smart_h);
    }

    anj_smart_track_tune_load();
    if (iRet == 0)
    {
        pthread_mutex_lock(&s_stSmartMutex);
        s_stSmartInit = 1;
        pthread_mutex_unlock(&s_stSmartMutex);
    }
    else
    {
        anj_smart_luma_cache_free();
    }

    return iRet;
}

int anj_smart_size_get(int camera, int *width, int *height)
{
    unsigned int smart_w = DEFAULT_SMART_WIDTH;
    unsigned int smart_h = DEFAULT_SMART_HEIGHT;

    if (width == NULL || height == NULL || camera < 0 || camera >= ANJ_CAMERA_MAX_NUMS)
    {
        return -1;
    }

    if (anj_mw_smart_size_get(&gstAnjSmartAttr[camera], &smart_w, &smart_h) != 0)
    {
        smart_w = DEFAULT_SMART_WIDTH;
        smart_h = DEFAULT_SMART_HEIGHT;
    }

    *width = (int)smart_w;
    *height = (int)smart_h;
    return 0;
}

static int anj_smart_uninit(void)
{
    int camera = 0;

    if (s_stSmartInit == 0)
    {
        __ERR("not init!\n");
        return 0;
    }

    /* 等待正在执行的 YUV 回调退出后再销毁 */
    pthread_mutex_lock(&s_stSmartMutex);
    s_stSmartInit = 0;
    pthread_mutex_unlock(&s_stSmartMutex);

    /* 清除残留目标框，避免 restart 窗口内 VPSS 框残留 */
    for (camera = 0; camera < ANJ_CAMERA_MAX_NUMS; camera++)
    {
        event_rect_param_s stEventRectParam = {0};

        stEventRectParam.camera = camera;
        stEventRectParam.type = EVENT_ALARM_TYPE_AI_PD;
        stEventRectParam.s32RectCnt = 0;
        anj_osd_draw_rect(&stEventRectParam);
    }

    /* 停止向已销毁的 smart 模块投递 YUV，避免仅靠 s_stSmartInit 软停 */
    anj_mw_smart_register_yuv_cb(NULL);

    anj_smart_provider_fd_uninit();
    anj_smart_provider_pvd_uninit();
    anj_smart_provider_pd_uninit();
    anj_smart_provider_md_uninit();
    anj_smart_luma_cache_free();
    return 0;
}

static int anj_smart_restart_thread(void *ctx, int *bStart)
{
    __INFO("smart restart begin...\n");

    anj_smart_uninit();
    sleep(1);

    anj_smart_init();
    __INFO("smart restart end...\n");
    return 0;
}

static int anj_smart_rect_center_moved(int cx0, int cy0, int cx1, int cy1)
{
    int dx = cx1 - cx0;
    int dy = cy1 - cy0;

    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;

    return (dx > (SMART_PD_WIDTH * ANJ_SMART_HUMAN_MOVE_H_RATIO_PCT / 100)) ||
           (dy > (SMART_PD_HEIGHT * ANJ_SMART_HUMAN_MOVE_V_RATIO_PCT / 100));
}

static void anj_smart_human_move_ctx_reset(int camera)
{
    memset(&s_stHumanMoveCtx[camera], 0, sizeof(s_stHumanMoveCtx[camera]));
}

/**
 * 判断越线人形目标是否发生位移。
 * 单目标：每 2s 记录一次 rect 中心，与当前中心对比判断是否移动。
 * @param cross_pd_cnt: class_id==0 且 bCrossAlarm 成立的目标数量
 * @param center_x/center_y: 各目标中心坐标，长度至少为 cross_pd_cnt
 * @return 1 有移动，0 无移动
 */
static int anj_smart_human_move_check(int camera, int cross_pd_cnt,
                                      const int *center_x, const int *center_y)
{
    AnjSmartHumanMoveCtx *ctx = NULL;
    unsigned long long now_ms = 0;
    int cx = 0;
    int cy = 0;

    if (camera < 0 || camera >= ANJ_CAMERA_MAX_NUMS || center_x == NULL || center_y == NULL)
    {
        return 0;
    }

    if (ANJ_CUSTOMER_TYPE != CUSTOMER_WTD)
    {
        return 0;
    }
    ctx = &s_stHumanMoveCtx[camera];
    if (cross_pd_cnt <= 0)
    {
        anj_smart_human_move_ctx_reset(camera);
        return 0;
    }

    if (cross_pd_cnt >= 2)
    {
        return 1;
    }

    now_ms = anj_mw_get_cputime_ms(NULL);
    cx = center_x[0];
    cy = center_y[0];

    if (ctx->record_ms == 0 ||
        (now_ms - ctx->record_ms) > ANJ_SMART_HUMAN_MOVE_WINDOW_MS)
    {
        ctx->record_ms = now_ms;
        ctx->record_cx = cx;
        ctx->record_cy = cy;
        return 0;
    }

    return anj_smart_rect_center_moved(ctx->record_cx, ctx->record_cy, cx, cy);
}

static void anj_smart_rect_process(int camera, AnjSmartInfo *pstAnjSmartInfo)
{
    int class_id_for_alarm = -1; // 取首个classid作为报警
    int vaild_rect_num = 0;      // 有效目标框数量

    EventResult event_result = {0};
    event_rect_param_s stEventRectParam = {0};      // 所有目标框
    event_rect_param_s stEventVaildRectParam = {0}; // 有效目标框

    stEventRectParam.camera = camera;
    stEventRectParam.s32RectCnt = pstAnjSmartInfo->boxCnt;
    stEventVaildRectParam.camera = camera;

    int bPolygonTwinkle = 0;
    int bCrossLineTwinkle = 0;
    int bCrossAlarm = 0;
    int bPolygonAlarm = 0;
    int bThreshold = 0;
    int cross_pd_cnt = 0;
    int cross_pd_cx[MAX_EVENT_RECT_NUM] = {0};
    int cross_pd_cy[MAX_EVENT_RECT_NUM] = {0};

    for (int i = 0; i < stEventRectParam.s32RectCnt; i++)
    {
        bThreshold = anj_smart_target_threshold_compare(pstAnjSmartInfo->stBoxInfo[i].score, camera);
        if (0 == bThreshold)
        {
            __DBG("smart filter target rect[%d]:score:%f lower than threshold\n", i, pstAnjSmartInfo->stBoxInfo[i].score);
            continue;
        }

        stEventRectParam.event_rect[i].pos_x = pstAnjSmartInfo->stBoxInfo[i].x;
        stEventRectParam.event_rect[i].pos_y = pstAnjSmartInfo->stBoxInfo[i].y;
        stEventRectParam.event_rect[i].width = pstAnjSmartInfo->stBoxInfo[i].width;
        stEventRectParam.event_rect[i].height = pstAnjSmartInfo->stBoxInfo[i].height;
        /* PVD class_id: 0=人 1=自行车 2=车 3=摩托；自行车/摩托与人同色 */
        if (pstAnjSmartInfo->stBoxInfo[i].class_id == 2)
        {
            stEventRectParam.event_rect[i].u32Color = RGB_VALUE_BLUE;
        }
        else
        {
            stEventRectParam.event_rect[i].u32Color = RGB_VALUE_GREEN;
        }

        anj_smart_detect_rect_line_cross(&pstAnjSmartInfo->stBoxInfo[i], &bCrossAlarm, camera);
        bPolygonAlarm = anj_smart_rect_polygon_intersect(&pstAnjSmartInfo->stBoxInfo[i], camera);

        if (bCrossAlarm && pstAnjSmartInfo->stBoxInfo[i].class_id == 0 && cross_pd_cnt < MAX_EVENT_RECT_NUM)
        {
            cross_pd_cx[cross_pd_cnt] = pstAnjSmartInfo->stBoxInfo[i].x + pstAnjSmartInfo->stBoxInfo[i].width / 2;
            cross_pd_cy[cross_pd_cnt] = pstAnjSmartInfo->stBoxInfo[i].y + pstAnjSmartInfo->stBoxInfo[i].height / 2;
            cross_pd_cnt++;
        }

        if (bCrossAlarm || bPolygonAlarm)
        {
            if (0 == vaild_rect_num)
            {
                // 取第一个结果的class_id作为报警类型
                class_id_for_alarm = pstAnjSmartInfo->stBoxInfo[i].class_id;
            }

            stEventVaildRectParam.event_rect[vaild_rect_num].pos_x = pstAnjSmartInfo->stBoxInfo[i].x;
            stEventVaildRectParam.event_rect[vaild_rect_num].pos_y = pstAnjSmartInfo->stBoxInfo[i].y;
            stEventVaildRectParam.event_rect[vaild_rect_num].width = pstAnjSmartInfo->stBoxInfo[i].width;
            stEventVaildRectParam.event_rect[vaild_rect_num].height = pstAnjSmartInfo->stBoxInfo[i].height;
            stEventVaildRectParam.s32RectCnt = vaild_rect_num + 1;
            vaild_rect_num++;
        }

        bCrossLineTwinkle |= bCrossAlarm;
        bPolygonTwinkle |= bPolygonAlarm;
    }

    // 目标框显示和清除
    stEventRectParam.type = EVENT_ALARM_TYPE_AI_PD;
    anj_osd_draw_rect(&stEventRectParam);

    stEventRectParam.move = anj_smart_human_move_check(camera, cross_pd_cnt, cross_pd_cx, cross_pd_cy);
    anj_ispctl_smart_set(&stEventRectParam);

    // 越线侦测
    if (bCrossLineTwinkle)
    {
        anj_osd_cross_line_twinkle(bCrossAlarm);
        anj_alarm_video_gate_start(camera);
    }

    // AI报警
    if (class_id_for_alarm >= 0)
    {
        __DBG("smart ai detect camera:%d class_id:%d\n", camera, class_id_for_alarm);

        if (class_id_for_alarm == 0)
        {
            eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_TRACK, &event_result, (void *)&stEventVaildRectParam);
        }

        {
            AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
            unsigned char rectTwinkle = pstAlarmCfg->aiAlarm.pdAlarm[camera].alarmAction.rect_twinkle_enable;

            if (rectTwinkle == AI_TWINKLE_REGION && bPolygonTwinkle)
            {
                anj_osd_polygon_twinkle();
            }
            else if (rectTwinkle == AI_TWINKLE_AROUND && (bPolygonTwinkle || bCrossLineTwinkle))
            {
                anj_osd_frame_border_twinkle();
            }
        }
        anj_alarm_ai_detect_start(camera, class_id_for_alarm);
    }
}

static void anj_md_rect_process(int camera, MD_RESULT_S *pstMdResult)
{
    // 移动侦测报警
    if (pstMdResult->region_cnt > 0)
    {
        __INFO("smart detect motion! region cnt:%d\n", pstMdResult->region_cnt);
        anj_alarm_motion_detect_start(camera);
    }
}

void anj_smart_data_cb(int u32DevId, void *p_vir_addr, unsigned long long p_phy_addr, int len, void *param)
{
    EventResult event_result = {0};
    int fd_en = 0;
    int pd_en = 0;
    AnjSmartInfo stAnjSmartInfo = {0};
    MD_RESULT_S stMdResult = {0};

    (void)param;

    pthread_mutex_lock(&s_stSmartMutex);
    if (s_stSmartInit == 0 || anj_osd_lens_cover_get())
    {
        pthread_mutex_unlock(&s_stSmartMutex);
        return;
    }

    if (access("/tmp/yuv", F_OK) == 0)
    {
        remove("/tmp/yuv");
        pFile = fopen("/tmp/nfs/leo.yuv", "w+");
        if (pFile)
        {
            unsigned int y_size = (unsigned int)s_stSmartLumaCtx.width * (unsigned int)s_stSmartLumaCtx.height;
            unsigned int uv_size = y_size >> 1;
            unsigned int uv_offset = y_size;

            fwrite(p_vir_addr, y_size, 1, pFile);
            fwrite((unsigned char *)p_vir_addr + uv_offset, uv_size, 1, pFile);
            fclose(pFile);
            pFile = NULL;
        }
    }

    // 移动侦测处理
    anj_smart_provider_md_process(p_vir_addr, len, u32DevId, &stMdResult);
    anj_md_rect_process(u32DevId, &stMdResult);

    /* PD/PVD 编译互斥；FD 可与 PD 或 PVD 同链。配置互斥，若同时开则用人脸 */
    fd_en = gstAnjSmartAttr[u32DevId].stAnjFdAttr.enable;
    pd_en = gstAnjSmartAttr[u32DevId].stAnjPdAttr.enable;
    if ((fd_en == 0) && (pd_en == 0))
    {
        pthread_mutex_unlock(&s_stSmartMutex);
        return;
    }

    if ((u32DevId < ANJ_CAMERA_MAX_NUMS) && (p_vir_addr != NULL) &&
        (s_stSmartLumaCtx.cache[u32DevId] != NULL) &&
        (s_stSmartLumaCtx.width > 0) && (s_stSmartLumaCtx.height > 0))
    {
        int y_size = s_stSmartLumaCtx.width * s_stSmartLumaCtx.height;
        if (len >= y_size)
        {
            pthread_mutex_lock(&s_stSmartLumaCtx.mutex);
            memcpy(s_stSmartLumaCtx.cache[u32DevId], p_vir_addr, y_size);
            s_stSmartLumaCtx.valid[u32DevId] = 1;
            pthread_mutex_unlock(&s_stSmartLumaCtx.mutex);
        }
    }

    if (u32DevId == 0)
    {
        event_yuv_s stYuv = {
            .data = p_vir_addr,
            .width = s_stSmartLumaCtx.width,
            .height = s_stSmartLumaCtx.height,
        };
        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_ZXING_SET_IMAGE, &event_result, (void *)&stYuv);
    }

    if (fd_en)
    {
        anj_smart_provider_fd_process(p_vir_addr, p_phy_addr, len, &stAnjSmartInfo, &gstAnjSmartAttr[u32DevId]);
    }
    else
    {
        /* PD/PVD 编译互斥，未注册 provider 为 no-op */
        anj_smart_provider_pvd_process(p_vir_addr, p_phy_addr, len, &stAnjSmartInfo, &gstAnjSmartAttr[u32DevId]);
        anj_smart_provider_pd_process(p_vir_addr, p_phy_addr, len, &stAnjSmartInfo, &gstAnjSmartAttr[u32DevId]);
    }
    anj_smart_rect_process(u32DevId, &stAnjSmartInfo);
    if (MAX_SCL_PORT < 3)
    {
        anj_snap_on_yuv(u32DevId, p_vir_addr, p_phy_addr, len, NULL);
    }
    pthread_mutex_unlock(&s_stSmartMutex);
}

/**
 * 获取智能分析的亮度值, OSD反色处理需要
 * @param camera: 摄像头索引
 * @param x: 目标区域中心点的X坐标，基于参考分辨率
 * @param y: 目标区域中心点的Y坐标，基于参考分辨率
 * @param ref_w: 参考分辨率的宽度
 * @param ref_h: 参考分辨率的高度
 * @param luma: 输出参数，返回目标区域的亮度值（0-255）
 * @return: 0表示成功获取亮度值，-1表示失败（例如参数错误或亮度数据无效）
 * @note: 该函数会根据输入的坐标和参考分辨率计算出对应智能分析分辨率下的坐标，
 *        并从亮度缓存中获取该位置及其周围像素的亮度值进行平均，以提高亮度值的稳定性
 */
int anj_smart_luma_get(int camera, int x, int y, int ref_w, int ref_h, unsigned char *luma)
{
    int smart_w = 0;
    int smart_h = 0;
    int smart_grid_x = 0;
    int smart_grid_y = 0;
    int luma_sum = 0;
    int luma_sample_count = 0;
    int neighbor_grid_x_min = 0;
    int neighbor_grid_x_max = 0;
    int neighbor_grid_y_min = 0;
    int neighbor_grid_y_max = 0;

    if (luma == NULL || camera < 0 || camera >= ANJ_CAMERA_MAX_NUMS || ref_w <= 0 || ref_h <= 0)
    {
        return -1;
    }

    /*
     * 与 anj_smart_luma_cache_free 共用 luma mutex：
     * 必须在锁内再校验 cache，避免 uninit 释放后 UAF。
     */
    pthread_mutex_lock(&s_stSmartLumaCtx.mutex);
    if (s_stSmartInit == 0 ||
        (s_stSmartLumaCtx.width <= 0) || (s_stSmartLumaCtx.height <= 0) ||
        (s_stSmartLumaCtx.cache[camera] == NULL) ||
        (!s_stSmartLumaCtx.valid[camera]))
    {
        pthread_mutex_unlock(&s_stSmartLumaCtx.mutex);
        return -1;
    }

    /*
     * 坐标语义：
     *   (x, y)     : 以调用方给的参考分辨率 (ref_w, ref_h) 为基准的目标点
     *   (smart_x) : 映射到当前智能分析网格 width / height 上的列
     *   (smart_y) : 映射到当前智能分析网格 width / height 上的行
     */
    smart_w = s_stSmartLumaCtx.width;
    smart_h = s_stSmartLumaCtx.height;
    smart_grid_x = x * smart_w / ref_w;
    smart_grid_y = y * smart_h / ref_h;

    if (smart_grid_x < 0)
        smart_grid_x = 0;
    else if (smart_grid_x >= smart_w)
        smart_grid_x = smart_w - 1;

    if (smart_grid_y < 0)
        smart_grid_y = 0;
    else if (smart_grid_y >= smart_h)
        smart_grid_y = smart_h - 1;

    /* 取目标点周围的 3x3 邻域做平均，提升亮度稳定性 */
    neighbor_grid_x_min = (smart_grid_x > 0) ? (smart_grid_x - 1) : smart_grid_x;
    neighbor_grid_x_max = (smart_grid_x < (smart_w - 1)) ? (smart_grid_x + 1) : smart_grid_x;
    neighbor_grid_y_min = (smart_grid_y > 0) ? (smart_grid_y - 1) : smart_grid_y;
    neighbor_grid_y_max = (smart_grid_y < (smart_h - 1)) ? (smart_grid_y + 1) : smart_grid_y;

    for (int neighbor_grid_y = neighbor_grid_y_min; neighbor_grid_y <= neighbor_grid_y_max; ++neighbor_grid_y)
    {
        for (int neighbor_grid_x = neighbor_grid_x_min; neighbor_grid_x <= neighbor_grid_x_max; ++neighbor_grid_x)
        {
            luma_sum += s_stSmartLumaCtx.cache[camera][neighbor_grid_y * smart_w + neighbor_grid_x];
            luma_sample_count++;
        }
    }

    *luma = (unsigned char)((luma_sample_count > 0) ? (luma_sum / luma_sample_count)
                                                       : s_stSmartLumaCtx.cache[camera][smart_grid_y * smart_w + smart_grid_x]);
    pthread_mutex_unlock(&s_stSmartLumaCtx.mutex);
    return 0;
}

smart_mask_e anj_smart_mask_get()
{
    smart_mask_e eSmartMask = SMART_NULL_MASK;
    
    if (0 == s_stSmartInit)
    {
        return eSmartMask;
    }

    eSmartMask |= anj_smart_provider_md_mask_get();
    eSmartMask |= anj_smart_provider_pd_mask_get();
    eSmartMask |= anj_smart_provider_pvd_mask_get();
    eSmartMask |= anj_smart_provider_fd_mask_get();
    return eSmartMask;
}

int anj_smart_detect_rect_cross(int rect1_x1, int rect1_y1, int rect1_x2, int rect1_y2,
                                int rect2_x1, int rect2_y1, int rect2_x2, int rect2_y2)
{
    if (0 == s_stSmartInit)
    {
        return 0;
    }
    if (rect1_x1 > rect1_x2)
    {
        aj_swap_value(&rect1_x1, &rect1_x2);
    }
    if (rect1_y1 > rect1_y2)
    {
        aj_swap_value(&rect1_y1, &rect1_y2);
    }
    if (rect2_x1 > rect2_x2)
    {
        aj_swap_value(&rect2_x1, &rect2_x2);
    }
    if (rect2_y1 > rect2_y2)
    {
        aj_swap_value(&rect2_y1, &rect2_y2);
    }

    if (
        (rect1_x2 < rect2_x1) ||
        (rect1_x1 > rect2_x2) ||
        (rect1_y2 < rect2_y1) ||
        (rect1_y1 > rect2_y2))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

static float anj_smart_track_ema_weight_clamp(float weight)
{
    if (weight < 0.0f)
    {
        return 0.0f;
    }
    if (weight > 1.0f)
    {
        return 1.0f;
    }
    return weight;
}

void anj_smart_track_tune_load(void)
{
    char *str = NULL;

    s_stTrackEmaNewWeight = ANJ_SMART_TRACK_EMA_NEW_WEIGHT;
    str = anj_mw_read_file_buffer(PTZ_TRACK_EMA_DEBUG_FILE);
    if (str)
    {
        s_stTrackEmaNewWeight = anj_smart_track_ema_weight_clamp((float)atof(str));
        anj_mw_free(str);
        str = NULL;
    }
    __INFO("ptz track ema weight:%f (file:%s)\n", s_stTrackEmaNewWeight, PTZ_TRACK_EMA_DEBUG_FILE);
}

typedef struct
{
    int bActive;
    event_rect_s lockRect;
    float smoothCx;
    float smoothCy;
    float smoothW;
    float smoothH;
    unsigned long long lastDetectMs;
} AnjSmartTrackFilterCtx;

static AnjSmartTrackFilterCtx s_stTrackFilter[ANJ_CAMERA_MAX_NUMS] = {0};

static void anj_smart_track_rect_pick(AnjSmartTrackFilterCtx *ctx, event_rect_param_s *pstRectangle, event_rect_s *pstRaw)
{
    int min_distance = -1;
    int last_center_x = ctx->lockRect.pos_x + ctx->lockRect.width / 2;
    int last_center_y = ctx->lockRect.pos_y + ctx->lockRect.height / 2;

    for (int i = 0; i < pstRectangle->s32RectCnt; i++)
    {
        event_rect_s *pstRec = &pstRectangle->event_rect[i];
        int this_center_x = pstRec->pos_x + pstRec->width / 2;
        int this_center_y = pstRec->pos_y + pstRec->height / 2;
        int this_distance = (this_center_x - last_center_x) * (this_center_x - last_center_x) +
                            (this_center_y - last_center_y) * (this_center_y - last_center_y);

        if (min_distance < 0 || this_distance < min_distance)
        {
            min_distance = this_distance;
            *pstRaw = *pstRec;
        }
    }

    ctx->lockRect = *pstRaw;
}

int anj_smart_track_rect_update(int camera, event_rect_param_s *pstRectangle, event_rect_s *pstOut)
{
    if (0 == s_stSmartInit)
    {
        return 0;
    }
    AnjSmartTrackFilterCtx *ctx = NULL;

    if (camera < 0 || camera >= ANJ_CAMERA_MAX_NUMS || pstRectangle == NULL || pstOut == NULL ||
        pstRectangle->s32RectCnt <= 0)
    {
        return 0;
    }

    ctx = &s_stTrackFilter[camera];
    anj_smart_track_rect_pick(ctx, pstRectangle, pstOut);
    ctx->lastDetectMs = anj_mw_get_cputime_ms(NULL);
    return 1;
}

void anj_smart_track_rect_reset(int camera)
{
    if (0 == s_stSmartInit || camera < 0 || camera >= ANJ_CAMERA_MAX_NUMS)
    {
        return;
    }

    memset(&s_stTrackFilter[camera], 0, sizeof(s_stTrackFilter[camera]));
}

event_rect_s anj_smart_rec_get(event_rect_param_s *pstRectangle)
{
    static event_rect_s stLastTrackRec = {0};
    event_rect_s stZero = {0};

    if (0 == s_stSmartInit)
    {
        return stZero;
    }

    int min_distance = -1; /*最小的距离*/
    int this_distance = 0; /*区域中心坐标与上一个锁定区域的中心坐标距离*/
    int last_center_x = 0;
    int last_center_y = 0;
    int this_center_x = 0;
    int this_center_y = 0;
    last_center_x = stLastTrackRec.pos_x + stLastTrackRec.width / 2;
    last_center_y = stLastTrackRec.pos_y + stLastTrackRec.height / 2;

    /*锁定需要追踪的区域，检测到多个区域，当前区域和上一个锁定区域中心坐标最近的，为本次锁定区域*/
    for (int i = 0; i < pstRectangle->s32RectCnt; i++)
    {
        event_rect_s *pstRec = &pstRectangle->event_rect[i];
        this_center_x = pstRec->pos_x + pstRec->width / 2;
        this_center_y = pstRec->pos_y + pstRec->height / 2;
        this_distance = (this_center_x - last_center_x) * (this_center_x - last_center_x) +
                        (this_center_y - last_center_y) * (this_center_y - last_center_y);
        if (min_distance < 0)
        {
            min_distance = this_distance;
            stLastTrackRec = *pstRec;
        }
        else
        {
            if (this_distance < min_distance)
            {
                min_distance = this_distance;
                stLastTrackRec = *pstRec;
            }
        }
    }

    return stLastTrackRec;
}

void anj_smart_restart()
{
    if (0 == s_stSmartInit)
    {
        return ;
    }
    if (s_stSmartRestartThread.start != 0 && s_stSmartRestartThread.end == 0)
    {
        __ERR("smart restarting...\n");
        return;
    }

    memset(&s_stSmartRestartThread, 0, sizeof(anj_thread_s));
    s_stSmartRestartThread.bAutoDestroy = 1;
    strncpy(s_stSmartRestartThread.iThreadName, "smart_restart", sizeof(s_stSmartRestartThread.iThreadName) - 1);
    s_stSmartRestartThread.iThreadjob.ctx = (void *)&s_stSmartRestartThread;
    s_stSmartRestartThread.iThreadjob.func = anj_smart_restart_thread;
    anj_thread_task_create(&s_stSmartRestartThread);
}

void anj_smart_sensitivity_update(int camera, float sensitivity)
{
    if (0 == s_stSmartInit)
    {
        return ;
    }
    CHECK_VALUE_LIMIT_RANGE(sensitivity, 0.01, 0.6);

    if (s_fLastSensitivity[camera] == sensitivity)
    {
        return;
    }
    __INFO("anj_smart_sensitivity_update: camera:%d, sensitivity:%f\n", camera, sensitivity);
    s_fLastSensitivity[camera] = sensitivity;
    s_fActualAlarmThreshold[camera] = anj_mw_smart_set_sensitivity(sensitivity);
    CHECK_VALUE_LIMIT_RANGE(s_fActualAlarmThreshold[camera], 0.01, 0.6);
}

REGISTER_MODULE(anj_smart, MODULE_PRIORITY_SMART);
