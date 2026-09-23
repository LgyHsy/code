#include "anj_config.h"
#include "anj_config_ptz.h"
#include "anj_mw_comm.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static IotPtzConfig s_stIotPtzConfig = {0};

static void anj_config_ptz_zoom_get(IXML_Node *pNode)
{
    IXML_Node *tmpAttr = pNode->firstAttr;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "CurMultiple"))
        {
            pstIotPtzConfig->m_zoom.cur_multiple = Str2Double(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MultipleStep"))
        {
            pstIotPtzConfig->m_zoom.multiple_step = Str2Double(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MaxMultiple"))
        {
            pstIotPtzConfig->m_zoom.max_multiple = Str2Double(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MinMultiple"))
        {
            pstIotPtzConfig->m_zoom.min_multiple = Str2Double(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
}

static void anj_config_ptz_laststep_get(IXML_Node *pNode)
{
    IXML_Node *tmpAttr = pNode->firstAttr;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "StepX"))
        {
            pstIotPtzConfig->m_resetStep.HStep = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "StepY"))
        {
            pstIotPtzConfig->m_resetStep.VStep = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
}

static void anj_config_ptz_maxstep_get(IXML_Node *pNode)
{
    IXML_Node *tmpAttr = pNode->firstAttr;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "StepX"))
        {
            pstIotPtzConfig->m_MaxStep.HStep = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "StepY"))
        {
            pstIotPtzConfig->m_MaxStep.VStep = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
}

static void anj_config_ptz_mistake_step_get(IXML_Node *pNode)
{
    IXML_Node *tmpAttr = pNode->firstAttr;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "StepX"))
        {
            pstIotPtzConfig->m_MistakeStep.HStep = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "StepY"))
        {
            pstIotPtzConfig->m_MistakeStep.VStep = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
}

static void anj_config_ptz_direction_get(IXML_Node *pNode)
{
    IXML_Node *tmpAttr = pNode->firstAttr;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "directionX"))
        {
            pstIotPtzConfig->m_ptzDir.HDir = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "directionY"))
        {
            pstIotPtzConfig->m_ptzDir.VDir = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
}

static void anj_config_ptz_cruise_get(IXML_Node *pNode)
{
    IXML_Node *tmpAttr = pNode->firstAttr;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pstIotPtzConfig->m_ptzCruiseEnable = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
}

static void anj_config_ptz_speed_get(IXML_Node *pNode)
{
    IXML_Node *tmpAttr = pNode->firstAttr;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "PtzSpeedH"))
        {
            pstIotPtzConfig->m_ptzSpeed.HSpeed = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "PtzSpeedV"))
        {
            pstIotPtzConfig->m_ptzSpeed.VSpeed = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
}

static void anj_config_ptz_orientation_get(IXML_Node *pNode)
{
    IXML_Node *tmpAttr = pNode->firstAttr;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "LeftDownX"))
        {
            pstIotPtzConfig->m_3dOrientStep.left_down_point.HStep = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "LeftDownY"))
        {
            pstIotPtzConfig->m_3dOrientStep.left_down_point.VStep = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RightUpX"))
        {
            pstIotPtzConfig->m_3dOrientStep.right_up_point.HStep = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RightUpY"))
        {
            pstIotPtzConfig->m_3dOrientStep.right_up_point.VStep = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
}

static void anj_config_ptz_linescan_get(IXML_Node *pNode)
{
    IXML_Node *tmpAttr = pNode->firstAttr;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    PtzLineScan *ptzLineScan = &pstIotPtzConfig->m_ptzLineScan;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "LeftMarginX"))
        {
            ptzLineScan->left_margin.HStep = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "LeftMarginY"))
        {
            ptzLineScan->left_margin.VStep = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "LeftMarginMultiple"))
        {
            ptzLineScan->left_multiple = Str2Double(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RightMarginX"))
        {
            ptzLineScan->right_margin.HStep = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RightMarginY"))
        {
            ptzLineScan->right_margin.VStep = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RightMarginMultiple"))
        {
            ptzLineScan->right_multiple = Str2Double(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            ptzLineScan->enable = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }
}

static void anj_config_ptz_preset_get(IXML_Node *pNode)
{
    int presetId = 0;
    IXML_Node *tmpAttr = pNode->firstAttr;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "PresetNum"))
        {
            presetId = Str2Num(tmpAttr->nodeValue);
            pstIotPtzConfig->m_ptzPreset[presetId - 1].preset_id = presetId;
        }
        else if (!strcmp(tmpAttr->nodeName, "PresetName"))
        {
            if (presetId > 0 && presetId < MAX_PTZ_PRESET)
            {
                PtzPreset *ptzPreset = &pstIotPtzConfig->m_ptzPreset[presetId - 1];
                StrCpy(ptzPreset->name, sizeof(ptzPreset->name), tmpAttr->nodeValue);
            }
        }
        else if (!strcmp(tmpAttr->nodeName, "IsHome"))
        {
            if (Str2Num(tmpAttr->nodeValue))
            {
                pstIotPtzConfig->watch_guard = presetId;
            }
        }
        else if (!strcmp(tmpAttr->nodeName, "Duration"))
        {
            if (pstIotPtzConfig->watch_guard > 0 &&
                pstIotPtzConfig->watch_guard == presetId)
            {
                pstIotPtzConfig->watch_guard_time = Str2Num(tmpAttr->nodeValue);
            }
        }
        else if (!strcmp(tmpAttr->nodeName, "StepX"))
        {
            if (presetId > 0 && presetId < MAX_PTZ_PRESET)
            {
                PtzPreset *ptzPreset = &pstIotPtzConfig->m_ptzPreset[presetId - 1];
                ptzPreset->preset_step.HStep = Str2Num(tmpAttr->nodeValue);
            }
        }
        else if (!strcmp(tmpAttr->nodeName, "StepY"))
        {
            if (presetId > 0 && presetId < MAX_PTZ_PRESET)
            {
                PtzPreset *ptzPreset = &pstIotPtzConfig->m_ptzPreset[presetId - 1];
                ptzPreset->preset_step.VStep = Str2Num(tmpAttr->nodeValue);
            }
        }
        else if (!strcmp(tmpAttr->nodeName, "Multiple"))
        {
            if (presetId > 0 && presetId < MAX_PTZ_PRESET)
            {
                PtzPreset *ptzPreset = &pstIotPtzConfig->m_ptzPreset[presetId - 1];
                ptzPreset->zoom_multiple = Str2Double(tmpAttr->nodeValue);
            }
        }

        tmpAttr = tmpAttr->nextSibling;
    }
}

int anj_config_ptz_load()
{
    int iRet = 0;
    IXML_Document *pDocNode = NULL;
    IXML_NodeList *pNodelist = NULL;
    char *pCfgXml = anj_mw_read_file_buffer(CONFIG_PTZ_PATH);
    ANJ_CHK(pCfgXml != NULL, -1, "read failed");

    pDocNode = ixmlParseBuffer(pCfgXml);
    ANJ_CHK(pDocNode != NULL, -1, "ixmlParseBuffer error");

    pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "PresetList");
    ANJ_CHK(pNodelist != NULL, -1, "xmlDocument_getElementsByTagName error");
    IXML_Node *pNode = pNodelist->nodeItem;
    IXML_Node *tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "PresetInfo"))
        {
            anj_config_ptz_preset_get(tmpChild);
        }
        else if (!strcmp(tmpChild->nodeName, "LineScan"))
        {
            anj_config_ptz_linescan_get(tmpChild);
        }
        else if (!strcmp(tmpChild->nodeName, "Cruise"))
        {
            anj_config_ptz_cruise_get(tmpChild);
        }
        else if (!strcmp(tmpChild->nodeName, "LastStep"))
        {
            anj_config_ptz_laststep_get(tmpChild);
        }
        else if (!strcmp(tmpChild->nodeName, "MaxStep"))
        {
            anj_config_ptz_maxstep_get(tmpChild);
        }
        else if (!strcmp(tmpChild->nodeName, "MistakeStep"))
        {
            anj_config_ptz_mistake_step_get(tmpChild);
        }
        else if (!strcmp(tmpChild->nodeName, "Direction"))
        {
            anj_config_ptz_direction_get(tmpChild);
        }
        else if (!strcmp(tmpChild->nodeName, "PTZ"))
        {
            anj_config_ptz_speed_get(tmpChild);
        }
        else if (!strcmp(tmpChild->nodeName, "Orientation"))
        {
            anj_config_ptz_orientation_get(tmpChild);
        }
        else if (!strcmp(tmpChild->nodeName, "ZoomInfo"))
        {
            anj_config_ptz_zoom_get(tmpChild);
        }
        tmpChild = tmpChild->nextSibling;
    }
endFunc:
    if (pNodelist)
    {
        ixmlNodeList_free(pNodelist);
    }
    if (pDocNode)
    {
        ixmlDocument_free(pDocNode);
    }
    if (pCfgXml)
    {
        anj_mw_free(pCfgXml);
    }
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    for (size_t i = 0; i < MAX_PTZ_PRESET; i++)
    {
        PtzPreset *ptzPreset = &pstIotPtzConfig->m_ptzPreset[i];
        if (ptzPreset->preset_id > 0)
        {
            __INFO("i:%d presetId:%d Hstep:%d Vstep:%d\n", i,
                   ptzPreset->preset_id, ptzPreset->preset_step.HStep, ptzPreset->preset_step.VStep);
        }
    }
    __INFO("watchGuard:%d duration:%d\n",
           pstIotPtzConfig->watch_guard, pstIotPtzConfig->watch_guard_time);
    return iRet;
}

char *anj_ptz_config_conver_xml(IotPtzConfig *ptzCfg)
{
    char *pe;
    char *pb;
    int bufSize = 8096;
    char *buf = anj_mw_malloc(bufSize);
    char escapeBuf[128] = {0};
    pb = buf;
    pe = buf + bufSize - 1;

    pb += snprintf(pb, pe - pb, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
    pb += snprintf(pb, pe - pb, "<PresetList>\r\n");

    for (size_t i = 0; i < MAX_PTZ_PRESET; i++)
    {
        PtzPreset *ptzPreset = &ptzCfg->m_ptzPreset[i];
        if (ptzPreset->preset_id > 0 && ptzPreset->preset_id < MAX_PTZ_PRESET)
        {
            pb += snprintf(pb, pe - pb, "<PresetInfo\r\n");
            pb += snprintf(pb, pe - pb, "PresetNum=\"%d\"\r\n", ptzPreset->preset_id);
            pb += snprintf(pb, pe - pb, "PresetName=\"%s\"\r\n", copy_with_escape(escapeBuf, ptzPreset->name));
            pb += snprintf(pb, pe - pb, "IsHome=\"%d\"\r\n", (ptzPreset->preset_id == ptzCfg->watch_guard));
            pb += snprintf(pb, pe - pb, "StepX=\"%d\"\r\n", ptzPreset->preset_step.HStep);
            pb += snprintf(pb, pe - pb, "StepY=\"%d\"\r\n", ptzPreset->preset_step.VStep);
            pb += snprintf(pb, pe - pb, "Multiple=\"%f\"\r\n", ptzPreset->zoom_multiple);
            pb += snprintf(pb, pe - pb, "Duration=\"%d\"\r\n",
                           (ptzPreset->preset_id == ptzCfg->watch_guard) ? ptzCfg->watch_guard_time : 0);
            pb += snprintf(pb, pe - pb, "/>\r\n");
        }
    }

    PtzLineScan *ptzLineScan = &ptzCfg->m_ptzLineScan;
    pb += snprintf(pb, pe - pb, "<LastStep StepX=\"%d\" StepY=\"%d\"/>\r\n", ptzCfg->m_resetStep.HStep, ptzCfg->m_resetStep.VStep);
    pb += snprintf(pb, pe - pb, "<MaxStep StepX=\"%d\" StepY=\"%d\"/>\r\n", ptzCfg->m_MaxStep.HStep, ptzCfg->m_MaxStep.VStep);
    pb += snprintf(pb, pe - pb, "<MistakeStep StepX=\"%d\" StepY=\"%d\"/>\r\n", ptzCfg->m_MistakeStep.HStep, ptzCfg->m_MistakeStep.VStep);
    pb += snprintf(pb, pe - pb, "<Direction directionX=\"%d\" directionY=\"%d\"/>\r\n", ptzCfg->m_ptzDir.HDir, ptzCfg->m_ptzDir.VDir);
    pb += snprintf(pb, pe - pb, "<LineScan Enable=\"%d\" LeftMarginX=\"%d\" LeftMarginY=\"%d\" RightMarginX=\"%d\" RightMarginY=\"%d\" LeftMarginMultiple=\"%f\" RightMarginMultiple=\"%f\"/>\r\n",
                   ptzLineScan->enable, ptzLineScan->left_margin.HStep, ptzLineScan->left_margin.VStep,
                   ptzLineScan->right_margin.HStep, ptzLineScan->right_margin.VStep,
                   ptzLineScan->left_multiple, ptzLineScan->right_multiple);
    pb += snprintf(pb, pe - pb, "<Cruise Enable=\"%d\"/>\r\n", ptzCfg->m_ptzCruiseEnable);
    pb += snprintf(pb, pe - pb, "<PTZ PtzSpeedH=\"%d\" PtzSpeedV=\"%d\"/>\r\n", ptzCfg->m_ptzSpeed.HSpeed, ptzCfg->m_ptzSpeed.VSpeed);
    pb += snprintf(pb, pe - pb, "<Orientation LeftDownX=\"%d\" LeftDownY=\"%d\" RightUpX=\"%d\" RightUpY=\"%d\"/>\r\n", 
                    ptzCfg->m_3dOrientStep.left_down_point.HStep, ptzCfg->m_3dOrientStep.left_down_point.VStep,
                    ptzCfg->m_3dOrientStep.right_up_point.HStep, ptzCfg->m_3dOrientStep.right_up_point.VStep);
    pb += snprintf(pb, pe - pb, "<ZoomInfo\r\n");
    pb += snprintf(pb, pe - pb, "CurMultiple=\"%f\"\r\n", ptzCfg->m_zoom.cur_multiple);
    pb += snprintf(pb, pe - pb, "MultipleStep=\"%f\"\r\n", ptzCfg->m_zoom.multiple_step);
    pb += snprintf(pb, pe - pb, "MaxMultiple=\"%f\"\r\n", ptzCfg->m_zoom.max_multiple);
    pb += snprintf(pb, pe - pb, "MinMultiple=\"%f\"\r\n", ptzCfg->m_zoom.min_multiple);
    pb += snprintf(pb, pe - pb, "/>\r\n");
    pb += snprintf(pb, pe - pb, "</PresetList>\r\n");

    return buf;
}

void anj_ptz_config_save(IotPtzConfig *pstIotPtzConfig)
{
    IotPtzConfig *ptzCfg = getIotPtzConfig();
    if (memcmp(ptzCfg, pstIotPtzConfig, sizeof(IotPtzConfig)))
    {
        *ptzCfg = *pstIotPtzConfig;
        char *buf = anj_ptz_config_conver_xml(ptzCfg);
        anj_mw_write_file(CONFIG_PTZ_PATH, 0, buf, strlen(buf));
        anj_mw_free(buf);
    }
}

IotPtzConfig *getIotPtzConfig()
{
    return &s_stIotPtzConfig;
}