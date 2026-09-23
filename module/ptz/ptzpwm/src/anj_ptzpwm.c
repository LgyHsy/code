#include "anj_mw_comm.h"
#include "anj_mw_hwctrl.h"
#include "anj_config_ptz.h"
#include "anj_ptzpwm.h"
#include "anj_ptz_provider.h"

static int anj_pztpwm_period_get(int speed)
{
    int period = 30000000; /*单位纳秒*/
    switch (speed)
    {
    case PTZ_SPEED_1:
        period = 80000000;
        break;
    case PTZ_SPEED_2:
        period = 40000000;
        break;
    case PTZ_SPEED_3:
        period = 32000000;
        break;
    case PTZ_SPEED_4:
        period = 26666666;
        break;
    case PTZ_SPEED_5:
        period = 20000000;
        break;
    case PTZ_SPEED_6:
        period = 16000000;
        break;
    case PTZ_SPEED_7:
        period = 10000000;
        break;
    case PTZ_SPEED_8:
        period = 8000000;
        break;
    case PTZ_SPEED_9:
        period = 6666666;
        break;
    case PTZ_SPEED_10:
        period = 6666666;
        break;
    default:
        break;
    }
    return period;
}

static int anj_ptzpwm_control(int Mode, int Step, int period)
{
    static int runStep = 0;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapture = &pstMediaConfig->videoConfig[0].videoCapture;

    int iRet = 0;
    int Vdir = pstIotPtzConfig->m_ptzDir.VDir ^ pstVideoCapture->vflip;
    int Hdir = pstIotPtzConfig->m_ptzDir.HDir ^ pstVideoCapture->hflip;
    if (Step > 0)
    {
        runStep = Step;
    }
    switch (Mode)
    {
    case PTZ_CTL_MOTOR_LEFT:
    {
        if (Step > 0)
        {
            if (Hdir == 0)
            {
                anj_mw_hwctrl_motor_left(Step, period);
            }
            else
            {
                anj_mw_hwctrl_motor_right(Step, period);
            }
        }
        break;
    }
    case PTZ_CTL_MOTOR_RIGHT:
    {
        if (Step > 0)
        {
            if (Hdir == 0)
            {
                anj_mw_hwctrl_motor_right(Step, period);
            }
            else
            {
                anj_mw_hwctrl_motor_left(Step, period);
            }
        }
        break;
    }
    case PTZ_CTL_MOTOR_UP:
    {
        if (Step > 0)
        {
            if (Vdir == 0)
            {
                anj_mw_hwctrl_motor_up(Step, period);
            }
            else
            {
                anj_mw_hwctrl_motor_down(Step, period);
            }
        }
        break;
    }
    case PTZ_CTL_MOTOR_DOWN:
    {
        if (Step > 0)
        {
            if (Vdir == 0)
            {
                anj_mw_hwctrl_motor_down(Step, period);
            }
            else
            {
                anj_mw_hwctrl_motor_up(Step, period);
            }
        }
        break;
    }
    case PTZ_CTL_MOTOR_STOP:
    {
        iRet = anj_mw_hwctrl_motor_stop();
        iRet = runStep - iRet;
        runStep = 0;
        break;
    }
    case PTZ_CTL_REMAIN_STEP:
    {
        iRet = anj_mw_hwctrl_motor_remain_step();
        iRet = runStep - iRet;
        break;
    }
    default:
    {
        __INFO("get error Mode %d\n", Mode);
        iRet = -1;
        break;
    }
    }
    return iRet;
}

int anj_ptzpwm_operate(int mode, int arg, int speed)
{
    if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD &&
        anj_mw_file_exists("/opt/ch/slow_vert_speed_version") &&
        (mode == PTZ_CTL_MOTOR_UP || mode == PTZ_CTL_MOTOR_DOWN))
        speed = PTZ_SPEED_2;
    int iRet = anj_ptzpwm_control(mode, arg, anj_pztpwm_period_get(speed));
    return iRet;
}

int anj_ptzpwm_init()
{
    anj_mw_hwctrl_motor_init();
    return 0;
}

int anj_ptzpwm_uninit()
{
    anj_mw_hwctrl_motor_uninit();
    return 0;
}

static const anj_ptz_provider_ops s_stPtzPwmProviderOps = {
    .provider_name = "ptzpwm",
    .provider_priority = 100,
    .init = anj_ptzpwm_init,
    .uninit = anj_ptzpwm_uninit,
    .operate = anj_ptzpwm_operate,
    .debug = 0,
    .dir_set = 0,
    .speed_set = 0,
};

ANJ_LINK_KEEP(anj_keep_ptzpwm_provider);

__attribute__((constructor)) static void anj_ptzpwm_provider_register(void)
{
    anj_ptz_provider_register(&s_stPtzPwmProviderOps);
}

__attribute__((destructor)) static void anj_ptzpwm_provider_unregister(void)
{
    anj_ptz_provider_unregister(&s_stPtzPwmProviderOps);
}
