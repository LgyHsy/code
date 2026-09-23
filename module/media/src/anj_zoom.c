#include "anj_mw_media_isp.h"
#include "anj_zoom.h"
#include "anj_osd.h"
#include "anj_smart.h"
#include "anj_module.h"
#include "anj_config_ptz.h"
#include "anj_sysctl.h"
#include "function_list.h"
#include "sdk_option.h"

#define ANJ_ZOOM_MIN_MULTIPLE (1.0)
#define ANJ_ZOOM_MULTIPLE_STEP (0.01)

#define TRACK_ZOOM_NORMAL_SPEED 102

#define TRACK_ZOOM_RESET_PERCENT 0.25f /* 变倍运行超过25%才允许重新变倍追踪 */

#define MOVEMENT_THRESHOLD_X 0.02f
#define MOVEMENT_THRESHOLD_Y 0.02f
#define MOVEMENT_THRESHOLD_SIZE 0.1f
#define MAX_MOVEMENT_THRESHOLD_SIZE 2.0f
#define MIN_MOVEMENT_THRESHOLD_SIZE 0.05f
/* 目标在画面中占比，1.0 表示占满宽/高，0.5 表示目标期望占画面一半宽/高 */
#define ZOOM_TARGET_FRAME_FRACTION 0.35f
#define ZOOM_TARGET_DEBUG_FILE "/tmp/zoom_target"
#define MAX_MOVE_SIZE_DEBUG_FILE "/tmp/max_move_size"

static double gstZoom_target = ZOOM_TARGET_FRAME_FRACTION;
static double gstMax_move_size = MAX_MOVEMENT_THRESHOLD_SIZE;

typedef enum zoom_table_mode_e
{
	ZOOM_TABLE_MODE_CENTER = 0, /* 后续手动变倍沿用 center table */
	ZOOM_TABLE_MODE_MOVE,		/* 后续手动变倍必须重新生成 move table */
} ZOOM_TABLE_MODE_E;

typedef enum zoom_track_state_e
{
	ZOOM_TRACK_STATE_IDLE = 0,
	ZOOM_TRACK_STATE_RUNNING,	/* 正在做 zoom track */
	ZOOM_TRACK_STATE_RETURNING, /* track end 后正在回退到追踪前 zoom */
} ZOOM_TRACK_STATE_E;

typedef struct zoom_runtime_param
{
	int bInit;
	double min_multiple;
	double max_multiple;
	DOUBLE_AREA_ENTRY curZoom;		   /* 当前实际 zoom 状态 */
	DOUBLE_AREA_ENTRY targetZoom;	   /* 当前这次 zoom 的目标状态 */
	DOUBLE_AREA_ENTRY focusAnchor;	   /* move table 模式下沿用的参考点，通常来自最近一次 zoom track */
	DOUBLE_AREA_ENTRY beforeTrackZoom; /* 本轮 track 开始前的 zoom 状态 */
	int bZoomOsdType;
	int bTrackInterrupted; /* 本轮 track 是否被外部命令打断 */
	int bFocusAnchorValid;
	int bForceTrackRezoom; /* 手动 zoom 打断过 track 后，下一次 zoom track 强制重建一次 move table */
	ZOOM_TABLE_MODE_E tableMode;
	ZOOM_TRACK_STATE_E trackState;
} ZOOM_RUNTIME_PARAM;

static pthread_mutex_t s_stZoomMutex = PTHREAD_MUTEX_INITIALIZER;
static ZOOM_RUNTIME_PARAM s_stZoomParam = {0};

static void anj_zoom_osd_set(int show)
{
	osd_custom_content_s osdZoomCustom = {0};
	osdZoomCustom.custom_show = show;
	osdZoomCustom.overlayText = s_stZoomParam.bZoomOsdType;
	osdZoomCustom.custom_x = 50;
	osdZoomCustom.custom_y = 99;
	osdZoomCustom.custom_location = POSITION_TYPE_BY_SCALE;
	anj_osd_debug_set(&osdZoomCustom);
}

static double anj_zoom_multiple_clamp(double multiple)
{
	if (multiple < s_stZoomParam.min_multiple)
		return s_stZoomParam.min_multiple;
	if (multiple > s_stZoomParam.max_multiple)
		return s_stZoomParam.max_multiple;
	return multiple;
}

static int anj_zoom_is_min_multiple(double multiple)
{
	return DOUBLE_LESS_EQUAL(multiple, s_stZoomParam.min_multiple);
}

static void anj_zoom_area_clamp_locked(DOUBLE_AREA_ENTRY *pArea)
{
	double cropSpan = 0.0;

	if (pArea == NULL)
	{
		return;
	}

	pArea->width = anj_zoom_multiple_clamp(pArea->width);
	pArea->height = pArea->width;
	cropSpan = 1.0 / pArea->width;

	if (pArea->xPos < 0.0)
		pArea->xPos = 0.0;
	if (pArea->yPos < 0.0)
		pArea->yPos = 0.0;
	if (pArea->xPos > 1.0 - cropSpan)
		pArea->xPos = 1.0 - cropSpan;
	if (pArea->yPos > 1.0 - cropSpan)
		pArea->yPos = 1.0 - cropSpan;
}

static void anj_zoom_config_prepare(ZOOM_PARAM *pstZoomParam)
{
	if (pstZoomParam == NULL)
	{
		return;
	}

	if (DOUBLE_LESS_EQUAL(pstZoomParam->max_multiple, 0))
	{
		pstZoomParam->max_multiple = PTZ_ZOOM_MAX_MULTIPLE;
	}
	if (DOUBLE_LESS_EQUAL(pstZoomParam->min_multiple, 0))
	{
		pstZoomParam->min_multiple = ANJ_ZOOM_MIN_MULTIPLE;
	}
	if (DOUBLE_LESS_EQUAL(pstZoomParam->multiple_step, 0))
	{
		pstZoomParam->multiple_step = ANJ_ZOOM_MULTIPLE_STEP;
	}
	if (DOUBLE_LESS_EQUAL(pstZoomParam->cur_multiple, 0.0))
	{
		pstZoomParam->cur_multiple = ANJ_ZOOM_MIN_MULTIPLE;
	}
}

static void anj_zoom_param_init(double min_multiple, double max_multiple)
{
	if (DOUBLE_LESS_EQUAL(min_multiple, 0))
	{
		min_multiple = ANJ_ZOOM_MIN_MULTIPLE;
	}
	if (DOUBLE_LESS_EQUAL(max_multiple, 0))
	{
		max_multiple = PTZ_ZOOM_MAX_MULTIPLE;
	}

	s_stZoomParam.min_multiple = min_multiple;
	s_stZoomParam.max_multiple = max_multiple;
	s_stZoomParam.curZoom.xPos = 0;
	s_stZoomParam.curZoom.yPos = 0;
	s_stZoomParam.curZoom.width = min_multiple;
	s_stZoomParam.curZoom.height = min_multiple;
	s_stZoomParam.targetZoom = s_stZoomParam.curZoom;
	s_stZoomParam.focusAnchor = s_stZoomParam.curZoom;
	s_stZoomParam.beforeTrackZoom = s_stZoomParam.curZoom;
	s_stZoomParam.bZoomOsdType = OVERLAY_ZOOM_DIGITAL;
	s_stZoomParam.bTrackInterrupted = 0;
	s_stZoomParam.bFocusAnchorValid = 0;
	s_stZoomParam.bForceTrackRezoom = 0;
	s_stZoomParam.tableMode = ZOOM_TABLE_MODE_CENTER;
	s_stZoomParam.trackState = ZOOM_TRACK_STATE_IDLE;

	char *str = anj_mw_read_file_buffer(ZOOM_TARGET_DEBUG_FILE);
	if (str)
	{
		gstZoom_target = atof(str);
		anj_mw_free(str);
		str = NULL;
	}
	str = anj_mw_read_file_buffer(MAX_MOVE_SIZE_DEBUG_FILE);
	if (str)
	{
		gstMax_move_size = atof(str);
		anj_mw_free(str);
	}
	__INFO("gstZoom_target:%f gstMax_move_size:%f\n", gstZoom_target, gstMax_move_size);
}

static void anj_zoom_center_mode_restore_locked(int iCameraIdex)
{
	/* 只有真正回到 1 倍后，后续手动变倍才允许重新切回 center table */
	s_stZoomParam.curZoom.xPos = 0;
	s_stZoomParam.curZoom.yPos = 0;
	s_stZoomParam.curZoom.width = s_stZoomParam.min_multiple;
	s_stZoomParam.curZoom.height = s_stZoomParam.min_multiple;
	s_stZoomParam.targetZoom = s_stZoomParam.curZoom;
	s_stZoomParam.tableMode = ZOOM_TABLE_MODE_CENTER;
	s_stZoomParam.bFocusAnchorValid = 0;
	s_stZoomParam.bForceTrackRezoom = 0;
	anj_mw_media_isp_zoom_center_init(iCameraIdex, s_stZoomParam.max_multiple, TRACK_ZOOM_NORMAL_SPEED);
}

static void anj_zoom_runtime_state_sync_locked(int iCameraIdex, DOUBLE_AREA_ENTRY *pCurArea, double *pRunPercent)
{
	DOUBLE_AREA_ENTRY curZoom = s_stZoomParam.curZoom;
	double runPercent = 1.0;
	int iRet = anj_mw_media_isp_zoom_get(iCameraIdex, &curZoom, &runPercent);
	if (iRet != 0)
	{
		/*
		 * 底层在 idle 状态下可能拿不到 zoom 运行索引。
		 * 这里必须保留上一帧确认过的实际位置，不能把未来目标值当成当前位置，
		 * 否则 center table 场景下的 zoom_set(from, to) 会退化成同值到同值。
		 */
		curZoom = s_stZoomParam.curZoom;
		runPercent = 1.0;
	}

	anj_zoom_area_clamp_locked(&curZoom);
	s_stZoomParam.curZoom = curZoom;

	if (pCurArea)
	{
		*pCurArea = s_stZoomParam.curZoom;
	}
	if (pRunPercent)
	{
		*pRunPercent = runPercent;
	}
}

static void anj_zoom_runtime_stop_locked(int iCameraIdex, DOUBLE_AREA_ENTRY *pCurArea, double *pRunPercent)
{
	DOUBLE_AREA_ENTRY curZoom = {0};
	double runPercent = 1.0;

	/*
	 * 底层 zoom_stop 之后再去取 curIndex，容易直接回到 entry0。
	 * 对 move table 场景来说，entry0 只是这次表的起点，不是用户按下 stop 时的真实位置，
	 * 所以必须先抓当前位置，再下发 stop。
	 */
	anj_zoom_runtime_state_sync_locked(iCameraIdex, &curZoom, &runPercent);
	anj_mw_media_isp_zoom_stop(iCameraIdex);
	s_stZoomParam.curZoom = curZoom;

	if (pCurArea)
	{
		*pCurArea = curZoom;
	}
	if (pRunPercent)
	{
		*pRunPercent = runPercent;
	}
}

static void anj_zoom_center_mode_sync_locked(int iCameraIdex)
{
	if (s_stZoomParam.trackState != ZOOM_TRACK_STATE_IDLE)
	{
		return;
	}

	if (s_stZoomParam.tableMode == ZOOM_TABLE_MODE_MOVE &&
		anj_zoom_is_min_multiple(s_stZoomParam.curZoom.width))
	{
		anj_zoom_center_mode_restore_locked(iCameraIdex);
	}
}

static void anj_zoom_move_target_build_locked(DOUBLE_AREA_ENTRY *pTarget, double run_multiple)
{
	if (pTarget == NULL)
	{
		return;
	}

	/*
	 * track 结束后只要还没回到 1 倍，手动 zoom 就要沿用最近一次 track 的参考点。
	 */
	if (s_stZoomParam.bFocusAnchorValid)
	{
		*pTarget = s_stZoomParam.focusAnchor;
	}
	else
	{
		*pTarget = s_stZoomParam.curZoom;
	}
	pTarget->width = anj_zoom_multiple_clamp(run_multiple);
	pTarget->height = pTarget->width;

	/* 缩回最小时，目标画面必须回到完整画面，不能继续保留 track 时的偏移坐标 */
	if (anj_zoom_is_min_multiple(pTarget->width))
	{
		pTarget->xPos = 0;
		pTarget->yPos = 0;
	}

	anj_zoom_area_clamp_locked(pTarget);
}

static void anj_zoom_track_target_build_locked(const PD_AREA_ENTRY *zoomTrackArea, DOUBLE_AREA_ENTRY *pTarget)
{
	double focusX = 0.0;
	double focusY = 0.0;
	double widthMultiple = 0.0;
	double heightMultiple = 0.0;
	double runMultiple = 0.0;
	double cropSpan = 0.0;

	if (zoomTrackArea == NULL || pTarget == NULL || zoomTrackArea->width <= 0 || zoomTrackArea->height <= 0)
	{
		return;
	}

	focusX = ((double)zoomTrackArea->xPos + (double)zoomTrackArea->width / 2.0) / (double)SMART_PD_WIDTH;
	focusY = ((double)zoomTrackArea->yPos + (double)zoomTrackArea->height / 3.0) / (double)SMART_PD_HEIGHT;

	widthMultiple = (double)SMART_PD_WIDTH * (double)gstZoom_target / (double)zoomTrackArea->width;
	heightMultiple = (double)SMART_PD_HEIGHT * (double)gstZoom_target / (double)zoomTrackArea->height;
	runMultiple = anj_zoom_multiple_clamp(fmin(widthMultiple, heightMultiple));
	cropSpan = 1.0 / runMultiple;

	pTarget->width = runMultiple;
	pTarget->height = runMultiple;
	pTarget->xPos = focusX - cropSpan / 2.0;
	pTarget->yPos = focusY - cropSpan / 2.0;

	/*
	 * 目标贴边时强制 crop 贴边，避免 focusY=height/3 居中导致
	 * 人已快出画但变倍窗口垂直方向拉不满最下方/最上方。
	 */
	{
		double top = (double)zoomTrackArea->yPos / (double)SMART_PD_HEIGHT;
		double bottom = ((double)zoomTrackArea->yPos + (double)zoomTrackArea->height) /
						(double)SMART_PD_HEIGHT;
		double left = (double)zoomTrackArea->xPos / (double)SMART_PD_WIDTH;
		double right = ((double)zoomTrackArea->xPos + (double)zoomTrackArea->width) /
					   (double)SMART_PD_WIDTH;

		if (bottom >= 0.90)
			pTarget->yPos = 1.0 - cropSpan;
		else if (top <= 0.10)
			pTarget->yPos = 0.0;

		if (right >= 0.90)
			pTarget->xPos = 1.0 - cropSpan;
		else if (left <= 0.10)
			pTarget->xPos = 0.0;
	}

	anj_zoom_area_clamp_locked(pTarget);
}

static int anj_zoom_track_rezoom(DOUBLE_AREA_ENTRY *pTarget, DOUBLE_AREA_ENTRY *pLastTarget, DOUBLE_AREA_ENTRY *pCur)
{
	float area = fabs(pTarget->width - pLastTarget->width);
	float move_x = fabs(pTarget->xPos - pLastTarget->xPos);
	float move_y = fabs(pTarget->yPos - pLastTarget->yPos);
	int need_zoom = (area > MOVEMENT_THRESHOLD_SIZE) || (move_x > MOVEMENT_THRESHOLD_X) || (move_y > MOVEMENT_THRESHOLD_Y);

	if (need_zoom)
	{
		float area2 = fabs(pTarget->width - pCur->width);
		float move_x2 = fabs(pTarget->xPos - pCur->xPos);
		float move_y2 = fabs(pTarget->yPos - pCur->yPos);
		return (area2 > MOVEMENT_THRESHOLD_SIZE) || (move_x2 > MOVEMENT_THRESHOLD_X) || (move_y2 > MOVEMENT_THRESHOLD_Y);
	}

	/* 目标太小或者太大时，强制重新生成 zoom table */
	if (pTarget->width > gstMax_move_size || pTarget->width < MIN_MOVEMENT_THRESHOLD_SIZE)
	{
		if (DOUBLE_NOT_EQUAL(pCur->width, pTarget->width))
		{
			return 1;
		}
	}

	return 0;
}

static void anj_zoom_core_init(int iCameraIdex, double min_multiple, double max_multiple)
{
	anj_zoom_param_init(min_multiple, max_multiple);
	anj_mw_media_isp_zoom_center_init(iCameraIdex, max_multiple, TRACK_ZOOM_NORMAL_SPEED);
	anj_mw_media_isp_zoom_track_init();
}

void anj_zoom_ctrl(int iCameraIdex, double run_multiple)
{
	if (!s_stZoomParam.bInit)
	{
		__ERR("not init\n");
		return;
	}
	DOUBLE_AREA_ENTRY curZoom = {0};
	DOUBLE_AREA_ENTRY targetZoom = {0};
	double maxMultiple = 0;
	int bUseMoveInit = 0;

	anj_mutex_lock(&s_stZoomMutex);
	run_multiple = anj_zoom_multiple_clamp(run_multiple);

	anj_zoom_runtime_stop_locked(iCameraIdex, &curZoom, NULL);
	anj_zoom_center_mode_sync_locked(iCameraIdex);

	bUseMoveInit = (s_stZoomParam.tableMode == ZOOM_TABLE_MODE_MOVE);
	if (bUseMoveInit)
	{
		anj_zoom_move_target_build_locked(&targetZoom, run_multiple);
		s_stZoomParam.tableMode = ZOOM_TABLE_MODE_MOVE;
	}
	else
	{
		targetZoom.xPos = 0;
		targetZoom.yPos = 0;
		targetZoom.width = run_multiple;
		targetZoom.height = run_multiple;
		anj_zoom_area_clamp_locked(&targetZoom);
	}

	s_stZoomParam.targetZoom = targetZoom;
	s_stZoomParam.bTrackInterrupted = 0;
	maxMultiple = s_stZoomParam.max_multiple;
	anj_mutex_unlock(&s_stZoomMutex);

	anj_zoom_osd_set(1);

	if (bUseMoveInit)
	{
		anj_mw_media_isp_zoom_move_init(iCameraIdex, maxMultiple, &curZoom, &targetZoom);
	}
	else
	{
		anj_mw_media_isp_zoom_set(iCameraIdex, curZoom.width, targetZoom.width);
	}
}

void anj_zoom_interrupt(int iCameraIdex, double run_multiple)
{
	if (!s_stZoomParam.bInit)
	{
		__ERR("not init\n");
		return;
	}
	int bAtMinZoom = 0;

	anj_mutex_lock(&s_stZoomMutex);
	run_multiple = anj_zoom_multiple_clamp(run_multiple);

	anj_zoom_runtime_stop_locked(iCameraIdex, NULL, NULL);
	anj_zoom_center_mode_sync_locked(iCameraIdex);
	bAtMinZoom = anj_zoom_is_min_multiple(s_stZoomParam.curZoom.width);

	if (s_stZoomParam.trackState != ZOOM_TRACK_STATE_IDLE)
	{
		/* track 未真正结束前收到 PTZ_ZOOM，先把 track 收尾，下一次手动 zoom 强制走 move table */
		s_stZoomParam.bTrackInterrupted = 1;
		s_stZoomParam.tableMode = bAtMinZoom ? ZOOM_TABLE_MODE_CENTER : ZOOM_TABLE_MODE_MOVE;
	}

	anj_zoom_move_target_build_locked(&s_stZoomParam.targetZoom, run_multiple);
	anj_mutex_unlock(&s_stZoomMutex);
}

void anj_zoom_track_start(int iCameraIdex, PD_AREA_ENTRY *zoomTrackArea, int reset)
{
	if (!s_stZoomParam.bInit)
	{
		__ERR("not init\n");
		return;
	}
	double runPercent = 1.0;
	double maxMultiple = 0;
	DOUBLE_AREA_ENTRY curZoom = {0};
	DOUBLE_AREA_ENTRY targetZoom = {0};
	int bForceTrackRezoom = 0;
	int bNewTrack = 0;

	if (reset == 0 && (zoomTrackArea == NULL || zoomTrackArea->width <= 0 || zoomTrackArea->height <= 0))
	{
		return;
	}

	anj_mutex_lock(&s_stZoomMutex);
	anj_zoom_runtime_state_sync_locked(iCameraIdex, &curZoom, &runPercent);
	anj_zoom_center_mode_sync_locked(iCameraIdex);

	if (reset)
	{
		targetZoom = s_stZoomParam.beforeTrackZoom;
		s_stZoomParam.trackState = ZOOM_TRACK_STATE_RETURNING;
	}
	else
	{
		if (s_stZoomParam.trackState == ZOOM_TRACK_STATE_IDLE)
		{
			s_stZoomParam.beforeTrackZoom = curZoom;
			bNewTrack = 1;
		}

		s_stZoomParam.trackState = ZOOM_TRACK_STATE_RUNNING;
		s_stZoomParam.bTrackInterrupted = 0;
		bForceTrackRezoom = s_stZoomParam.bForceTrackRezoom;

		/* 正在向已下发的极限倍率变倍，到位前不重建 zoom table */
		if (DOUBLE_LESS(runPercent, 1.0) &&
			(anj_zoom_is_min_multiple(s_stZoomParam.targetZoom.width) ||
			 DOUBLE_GREATER_EQUAL(s_stZoomParam.targetZoom.width, s_stZoomParam.max_multiple)) &&
			DOUBLE_NOT_EQUAL(curZoom.width, s_stZoomParam.targetZoom.width))
		{
			anj_mutex_unlock(&s_stZoomMutex);
			return;
		}

		anj_zoom_track_target_build_locked(zoomTrackArea, &targetZoom);

		/*
		 * 手动 zoom/stop 打断过一次 track 后，下一次 AI track 必须至少重建一次 move table。
		 * 正常跟踪时，目标变化不足或当前 table 尚未运行 25%，都继续跑现有 table，
		 * 避免检测帧频繁 stop/reload 导致小距离平移卡顿。
		 */
		if ((bNewTrack == 0) &&
			(bForceTrackRezoom == 0) &&
			((0 == anj_zoom_track_rezoom(&targetZoom, &s_stZoomParam.targetZoom, &curZoom)) ||
			 DOUBLE_LESS(runPercent, TRACK_ZOOM_RESET_PERCENT)))
		{
			anj_mutex_unlock(&s_stZoomMutex);
			return;
		}

		s_stZoomParam.tableMode = ZOOM_TABLE_MODE_MOVE;
		s_stZoomParam.focusAnchor = targetZoom;
		s_stZoomParam.bFocusAnchorValid = 1;
		s_stZoomParam.bForceTrackRezoom = 0;
	}

	anj_zoom_runtime_stop_locked(iCameraIdex, &curZoom, NULL);
	s_stZoomParam.targetZoom = targetZoom;
	maxMultiple = s_stZoomParam.max_multiple;
	anj_mutex_unlock(&s_stZoomMutex);

	anj_zoom_osd_set(1);

	anj_mw_media_isp_zoom_move_init(iCameraIdex, maxMultiple, &curZoom, &targetZoom);
}

void anj_zoom_track_stop(int iCameraIdex)
{
	if (!s_stZoomParam.bInit)
	{
		__ERR("not init\n");
		return;
	}
	int overTime = 50;
	int bRestoreBeforeTrackState = 0;
	DOUBLE_AREA_ENTRY curZoom = {0};
	double runPercent = 0;

	anj_mutex_lock(&s_stZoomMutex);
	if (s_stZoomParam.trackState == ZOOM_TRACK_STATE_IDLE)
	{
		anj_mutex_unlock(&s_stZoomMutex);
		return;
	}

	/*
	 * 只有正常 track end 且已经进入 returning 阶段时，才等待 zoom 回退跑完。
	 * 手动 PTZ_ZOOM/stop 打断的场景不能在这里强行回退，否则会把后续的 move table 上下文冲掉。
	 */
	bRestoreBeforeTrackState = (s_stZoomParam.trackState == ZOOM_TRACK_STATE_RETURNING &&
								s_stZoomParam.bTrackInterrupted == 0);
	anj_mutex_unlock(&s_stZoomMutex);

	if (bRestoreBeforeTrackState)
	{
		anj_mw_rsleep(100 * 1000);
		while (overTime-- > 0)
		{
			anj_mutex_lock(&s_stZoomMutex);
			anj_zoom_runtime_state_sync_locked(iCameraIdex, &curZoom, &runPercent);
			if (DOUBLE_GREATER_EQUAL(runPercent, 1) || AREA_EQUAL(curZoom, s_stZoomParam.beforeTrackZoom))
			{
				anj_mutex_unlock(&s_stZoomMutex);
				break;
			}
			anj_mutex_unlock(&s_stZoomMutex);
			anj_mw_rsleep(100 * 1000);
		}
	}

	anj_mutex_lock(&s_stZoomMutex);
	anj_zoom_runtime_stop_locked(iCameraIdex, &curZoom, NULL);
	s_stZoomParam.trackState = ZOOM_TRACK_STATE_IDLE;

	if (bRestoreBeforeTrackState)
	{
		s_stZoomParam.curZoom = s_stZoomParam.beforeTrackZoom;
		s_stZoomParam.targetZoom = s_stZoomParam.curZoom;
		s_stZoomParam.tableMode = ZOOM_TABLE_MODE_CENTER;
		s_stZoomParam.bFocusAnchorValid = 0;
		s_stZoomParam.bForceTrackRezoom = 0;
		anj_mw_media_isp_zoom_center_init(iCameraIdex, s_stZoomParam.max_multiple, TRACK_ZOOM_NORMAL_SPEED);
	}
	else
	{
		/*
		 * track 被手动命令打断后，当前 crop 仍然属于 move table 体系。
		 * 只有真实缩回到 1 倍时，才在这里切回 center table。
		 */
		s_stZoomParam.curZoom = curZoom;
		s_stZoomParam.targetZoom = curZoom;
		s_stZoomParam.tableMode = ZOOM_TABLE_MODE_MOVE;
		s_stZoomParam.bForceTrackRezoom = 1;
		anj_zoom_center_mode_sync_locked(iCameraIdex);
	}

	s_stZoomParam.bTrackInterrupted = 0;
	anj_mutex_unlock(&s_stZoomMutex);

	anj_zoom_osd_set(0);
}

int anj_zoom_run_get(int iCameraIdex, DOUBLE_AREA_ENTRY *cur_area, double *pRunPercent)
{
	if (!s_stZoomParam.bInit)
	{
		__ERR("not init\n");
		return -1;
	}
	anj_mutex_lock(&s_stZoomMutex);
	anj_zoom_runtime_state_sync_locked(iCameraIdex, cur_area, pRunPercent);
	if (pRunPercent && DOUBLE_GREATER(*pRunPercent, 1.0))
	{
		*pRunPercent = 1.0;
	}
	anj_mutex_unlock(&s_stZoomMutex);
	return 0;
}

void anj_zoom_stop(int iCameraIdex, double *pRunPercent)
{
	if (!s_stZoomParam.bInit)
	{
		__ERR("not init\n");
		return;
	}
	anj_mutex_lock(&s_stZoomMutex);
	anj_zoom_runtime_stop_locked(iCameraIdex, NULL, pRunPercent);
	s_stZoomParam.targetZoom = s_stZoomParam.curZoom;

	if (s_stZoomParam.trackState != ZOOM_TRACK_STATE_IDLE)
	{
		/* track 中收到 stop，只停止当前 zoom，track 的最终收尾交给 anj_zoom_track_stop */
		s_stZoomParam.bTrackInterrupted = 1;
		s_stZoomParam.bForceTrackRezoom = 1;
	}
	else
	{
		anj_zoom_center_mode_sync_locked(iCameraIdex);
	}

	anj_mutex_unlock(&s_stZoomMutex);

	anj_zoom_osd_set(0);
}

void anj_zoom_osd_type_set()
{
	if (!s_stZoomParam.bInit)
	{
		__ERR("not init\n");
		return;
	}
	if (s_stZoomParam.bZoomOsdType == OVERLAY_ZOOM_DIGITAL)
	{
		s_stZoomParam.bZoomOsdType = OVERLAY_ZOOM_PROGRESS;
	}
	else
	{
		s_stZoomParam.bZoomOsdType = OVERLAY_ZOOM_DIGITAL;
	}
}

static int anj_zoom_init(void)
{
	if (s_stZoomParam.bInit)
	{
		__ERR("had been init\n");
		return -1;
	}
	anj_config_ptz_load();
	IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
	anj_zoom_config_prepare(&pstIotPtzConfig->m_zoom);
	anj_zoom_core_init(0, pstIotPtzConfig->m_zoom.min_multiple, pstIotPtzConfig->m_zoom.max_multiple);
	anj_sysctl_capability_add(FUNCTION_PTZ_ZOOM);
	s_stZoomParam.bInit = 1;
	return 0;
}

static int anj_zoom_uninit(void)
{
	if (!s_stZoomParam.bInit)
	{
		__ERR("not init\n");
		return -1;
	}

	anj_zoom_stop(0, NULL);
	s_stZoomParam.bInit = 0;
	return 0;
}

REGISTER_MODULE(anj_zoom, MODULE_PRIORITY_ZOOM);
