假设摄像机IP为192.168.1.10，用户名admin，密码为123456
Aassumes that the camera IP is 192.168.1.10, username is "admin", password is "123456"

1、
打开/关闭摄像机日志网络跟踪
Open/Close SLOG TCP trace
	http://192.168.1.10/cgi-bin/console.cgi?enable=1&username=admin&password=123456

2、
重启IPC
Reboot IPC
	http://192.168.1.10/cgi-bin/rebootipc&username=admin&password=123456

3、
恢复出厂设置
Restore to factory configuration
	http://192.168.1.10/cgi-bin/factoryipc&username=admin&password=123456

4、
设置NTP服务器
Set NTP server configuration
	http://192.168.1.10/cgi-bin/settings/ntp/&enable=1&server=ipvs.icamra.com@port=123&username=admin&password=123456

5、
获取以太网卡MAC地址
Get mac address of ethernet card 
	http://192.168.1.10/cgi-bin/getmacaddr_eth0.cgi&username=admin&password=123456

6、
获取安佳通讯协议端口
Get the communication port of anjvision protocol
	http://192.168.1.10/cgi-bin/getptzport&username=admin&password=123456

7、
获取KERNEL版本信息
Get kernel version infomation
	http://192.168.1.10/cgi-bin/settings/system/version_info/kernelVersion&username=admin&password=123456

8、
获取ROOTFS版本信息：
Get rootfs version infomation
	http://192.168.1.10/cgi-bin/settings/system/version_info/fsVersion&username=admin&password=123456

9、
获取设备序列号
Get serialno of camera
	http://192.168.1.10/cgi-bin/settings/system/version_info/serialNumber&username=admin&password=123456

10、
获取设备类型
Get device type
	http://192.168.1.10/cgi-bin/settings/system/device_info/device_type&username=admin&password=123456


11、
jpeg抓图
snap jpeg

子码流抓图
snap sub stream
	http://192.168.1.10/snapshot.cgi/stream=0

主码流抓图
snap main stream
	http://192.168.1.10/snapshot.cgi/stream=1


当onvif认证打开的时候，需要填入正确的用户名/密码。否则不需要用户名/密码。
When the onvif authentication opens, you need to fill in the correct username and password. Otherwise username and password is not required. 
	http://192.168.1.10/snapshot.cgi/stream=0&username=admin&password=123456

