/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2024, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#include "sys_inc.h"
#include "onvif.h"
#include "onvif_provisioning.h"


#ifdef PROVISIONING_SUPPORT

/************************************************************************************/
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_CFG g_onvif_cfg;

/************************************************************************************/

/**
 * @brief
 *  Continuously moves the camera left or right.
 *
 *  A device indicating MaximumPanMoves capability greater than zero shall
 *  support the provisional pan through the PanMove command.
 *
 * @return
 *  Possible return value : 
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoProvisioning
 **/
ONVIF_RET onvif_tpv_PanMove(tpv_PanMove_REQ * p_req)
{
    VideoSourceList * p_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSource);
    if (NULL == p_src)
    {
        return ONVIF_ERR_NoSource;
    }
    
    // todo : here add handler code ...

    return ONVIF_OK;
}

/**
 * @brief
 *  Continuously moves the camera up or down.
 *
 *  A device indicating MaximumTiltMoves capability greater than zero shall
 *  support the provisional tilt through the TiltMove command.
 *
 * @return
 *  Possible return value : 
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoProvisioning
 **/
ONVIF_RET onvif_tpv_TiltMove(tpv_TiltMove_REQ * p_req)
{
    VideoSourceList * p_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSource);
    if (NULL == p_src)
    {
        return ONVIF_ERR_NoSource;
    }
    
    // todo : here add handler code ...

    return ONVIF_OK;
}

/**
 * @brief
 *  Continuously moves the lens focal point in or out.
 *
 *  A device indicating MaximumZoomMoves capability greater than zero shall
 *  support the provisional zoom through the ZoomMove command.
 *
 * @return
 *  Possible return value : 
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoProvisioning
 **/
ONVIF_RET onvif_tpv_ZoomMove(tpv_ZoomMove_REQ * p_req)
{
    VideoSourceList * p_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSource);
    if (NULL == p_src)
    {
        return ONVIF_ERR_NoSource;
    }
    
    // todo : here add handler code ...

    return ONVIF_OK;
}

/**
 * @brief
 *  Continuously moves the camera clockwise or counterclockwise.
 *
 *  A device indicating MaximumRollMoves capability greater than zero shall 
 *  support the provisional roll through the RollMove command.
 *
 *  If a device supports the AutoLevel capability, the direction may be Auto.
 *  
 * @return
 *  Possible return value : 
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoProvisioning
 *  ONVIF_ERR_NoAutoMode
 **/
ONVIF_RET onvif_tpv_RollMove(tpv_RollMove_REQ * p_req)
{
    VideoSourceList * p_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSource);
    if (NULL == p_src)
    {
        return ONVIF_ERR_NoSource;
    }
    
    // todo : here add handler code ...

    return ONVIF_OK;
}

/**
 * @brief
 *  Continuously moves the camera lens in or out.
 *
 *  A device indicating MaximumFocusMoves capability greater than zero 
 *  shall support the provisional focus through the FocusMove command.
 *
 *  If a device supports the AutoFocus capability, the direction may be Auto.
 *
 * @return
 *  Possible return value : 
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoProvisioning
 *  ONVIF_ERR_NoAutoMode
 **/
ONVIF_RET onvif_tpv_FocusMove(tpv_FocusMove_REQ * p_req)
{
    VideoSourceList * p_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSource);
    if (NULL == p_src)
    {
        return ONVIF_ERR_NoSource;
    }
    
    // todo : here add handler code ...

    return ONVIF_OK;
}

/**
 * @brief
 *  Immediately stops movement on all axes.
 *
 * @return
 *  Possible return value : 
 *  ONVIF_ERR_NoSource
 **/
ONVIF_RET onvif_tpv_Stop(tpv_Stop_REQ * p_req)
{
    VideoSourceList * p_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSource);
    if (NULL == p_src)
    {
        return ONVIF_ERR_NoSource;
    }
    
    // todo : here add handler code ...

    return ONVIF_OK;
}

/**
 * @brief
 *  Returns information about how many provisioning operations have been 
 *  performed on each axis.
 *
 *  These values can be compared to the lifetime limits from 
 *  GetServiceCapabilities to determine how close to (or past) vendor defined
 *  life limits the device is. 
 *  The values shall survive a SetSystemFactoryDefault operation.
 *
 *  A single provisioning operation may increase the corresponding usage number
 *  by more than 1. 
 *  For example, pan usage may increment by the number of steps the stepper 
 *  motor has moved, which can be multiple steps per operation.
 *
 *  If a particular provisioning axis is not supported, the corresponding 
 *  usage value may be omitted from the response.
 *
 * @return
 *  Possible return value : 
 *  ONVIF_ERR_NoSource
 **/
ONVIF_RET onvif_tpv_GetUsage(tpv_GetUsage_REQ * p_req, tpv_GetUsage_RES * p_res)
{
    VideoSourceList * p_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSource);
    if (NULL == p_src)
    {
        return ONVIF_ERR_NoSource;
    }
    
    // todo : here add handler code ...
    

    return ONVIF_OK;
}


#endif // end of #ifdef PROVISIONING_SUPPORT



