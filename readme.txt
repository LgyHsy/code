├── app                     #程序启动入口和看门狗使用
│   ├── inc
│   │   └── anj_watchdog.h
│   └── src
│       ├── anj_watchdog.c
│       └── main.c
├── auto_build.sh           #编译脚本（./auto_build MYF30）
├── CMakeLists.txt
├── daemon                  #程序守护进程
│   ├── CMakeLists.txt
│   ├── inc
│   └── src
│       └── daemon.c
├── Kconfig                 #menuconfig定制编译相关
├── middleware              #中间件编译成.a
│   ├── CMakeLists.txt
│   ├── comm                #公用通用代码，如日志等
│   │   ├── inc
│   │   │   └── anj_mw_comm.h
│   │   └── src
│   │       └── anj_mw_comm.c
│   ├── hwctrl              #硬件控制行为相关，如开关灯
│   │   ├── inc
│   │   │   └── anj_mw_hwctrl.h
│   │   └── src
│   │       └── anj_mw_hwctrl.c
│   ├── middleware_option.cmake
│   ├── platforms           #sdk平台选择
│   │   └── mstar_37x       
│   │       ├── hw          #基于sdk平台的硬件控制，如GPIO的拉高拉低
│   │       │   ├── inc
│   │       │   │   └── anj_mw_hw.h
│   │       │   └── src
│   │       │       └── anj_mw_hw.c
│   │       ├── media       #基于sdk平台的媒体相关，如出流
│   │       │   ├── audio
│   │       │   ├── comm
│   │       │   ├── inc
│   │       │   ├── isp
│   │       │   ├── osd
│   │       │   ├── smart
│   │       │   └── video
│   │       └── platform_option.cmake
│   └── rec                 #卡录
│       ├── inc
│       │   └── anj_mw_rec.h
│       └── src
│           └── anj_mw_rec.c
├── module                  #程序的模块功能
│   ├── alarm               #报警模块
│   │   ├── inc
│   │   │   └── anj_alarm.h
│   │   └── src
│   │       └── anj_alarm.c
│   ├── comm                #通用模块
│   │   ├── inc
│   │   └── src
│   ├── config              #配置
│   │   ├── inc
│   │   │   └── anj_config.h
│   │   └── src
│   │       └── anj_config.c
│   ├── factory             #工厂产测
│   │   ├── inc
│   │   │   └── anj_factory.h
│   │   └── src
│   │       └── anj_factory.c
│   ├── inc                 #模块加载
│   │   └── anj_module.h
│   ├── mbuf                #程序视频流缓存池
│   │   ├── inc
│   │   │   └── anj_mbuf.h
│   │   └── src
│   │       └── anj_mbuf.c
│   ├── media               #媒体相关
│   │   ├── inc
│   │   │   ├── anj_audio.h
│   │   │   ├── anj_ispctl.h
│   │   │   ├── anj_osd.h
│   │   │   ├── anj_smart.h
│   │   │   └── anj_video.h
│   │   └── src
│   │       ├── anj_audio.c
│   │       ├── anj_ispctl.c
│   │       ├── anj_osd.c
│   │       ├── anj_smart.c
│   │       └── anj_video.c
│   ├── module_option.cmake
│   ├── net                 #网络
│   │   ├── 4g
│   │   │   ├── inc
│   │   │   │   ├── anj_4g.h
│   │   │   │   └── anj_net.h
│   │   │   └── src
│   │   │       └── anj_4g.c
│   │   ├── inc
│   │   │   └── anj_net.h
│   │   ├── src
│   │   │   └── anj_net.c
│   │   ├── wifi
│   │   │   ├── inc
│   │   │   │   ├── anj_net.h
│   │   │   │   └── anj_wifi.h
│   │   │   └── src
│   │   │       └── anj_wifi.c
│   │   └── wire
│   │       ├── inc
│   │       │   ├── anj_net.h
│   │       │   └── anj_wire.h
│   │       └── src
│   │           └── anj_wire.c
│   ├── ota                 #升级
│   │   ├── inc
│   │   │   └── anj_ota.h
│   │   └── src
│   │       └── anj_ota.c
│   ├── ptz                 #云台控制
│   │   ├── inc
│   │   │   └── anj_ptz.h
│   │   └── src
│   │       └── anj_ptz.c
│   ├── record              #卡录逻辑
│   │   ├── inc
│   │   │   └── anj_record.h
│   │   └── src
│   │       └── anj_record.c
│   ├── sdcard              #sd卡管理
│   │   ├── inc
│   │   └── src
│   ├── service             
│   │   ├── anjpri          #局域网私有协议
│   │   │   ├── inc
│   │   │   │   └── anj_pri.h
│   │   │   └── src
│   │   │       └── anj_pri.c
│   │   ├── anjser          #连接anj云服务器
│   │   │   ├── inc
│   │   │   │   └── anj_ser.h
│   │   │   └── src
│   │   │       └── anj_ser.c
│   │   ├── anjweb          #连接anjweb
│   │   ├── gb28181         
│   │   │   ├── inc
│   │   │   │   └── anj_gb28181.h
│   │   │   └── src
│   │   │       └── anj_gb28181.c
│   │   ├── inc
│   │   │   └── anj_service.h
│   │   ├── onvif
│   │   │   ├── inc
│   │   │   │   └── anj_onvif.h
│   │   │   └── src
│   │   │       └── anj_onvif.c
│   │   ├── rtsp
│   │   │   ├── inc
│   │   │   │   └── anj_rtsp.h
│   │   │   └── src
│   │   │       └── anj_rtsp.c
│   │   └── src
│   │       └── anj_service.c
│   └── sysmng              #系统管理
│       ├── inc
│       │   └── anj_sysmng.h
│       └── src
│           └── anj_sysmng.c
├── parse_config.py
├── prebuild                #预编译的第三方库源码和驱动代码以及编译生成的.a .ko等
├── project                 #型号选择
│   ├── MYF30
│   │   ├── config          #型号对应的编译配置
│   │   │   └── def_config
│   │   └── include         #型号对应的属性，如单目/双目摄像头
│   │       └── project_option.h
│   └── project_option.cmake
├── readme.txt
└── tools                   #可用的工具
